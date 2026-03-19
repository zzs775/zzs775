#include "QmlBridge.h"
#include "SimClock.h"
#include <QCursor>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QFile>
#include "AcmiTelemetryForwarder.h"

extern AppState g_appState;

void QmlBridge::requestAddModel(const QString& modelType)
{
    qDebug() << ">>> [C++] 桥接器收到请求:" << modelType;
    g_appState.pendingModelType = modelType;
    g_appState.isPlacingMode = true;
    if (m_window)
    {
        m_window->setCursor(Qt::CrossCursor);
    }
}

void QmlBridge::selectModel(const QString& fileName, const QString& category, const QString& fullPath)
{
    qDebug() << ">>> [C++] 选择模型:" << fileName << "分类:" << category << "路径:" << fullPath;

    g_appState.pendingModelType = fileName;
    g_appState.pendingModelPath = fullPath;
    g_appState.isPlacingMode = true;

    if (m_window)
    {
        m_window->setCursor(Qt::CrossCursor);
    }

    emit modelPlacementRequested(fileName, fullPath);
}

void QmlBridge::focusEntity(const QString& id)
{
    qDebug() << ">>> [C++] 请求关注实体:" << id;
    emit focusEntityRequested(id);
}

void QmlBridge::removeEntity(const QString& id)
{
    qDebug() << ">>> [C++] 请求删除实体:" << id;
}

void QmlBridge::untether()
{
    qDebug() << ">>> [C++] 请求取消视角锁定";
    g_appState.followedTargetId = "";
    emit focusEntityRequested(""); // 发送空字符串表示取消锁定
}

void QmlBridge::showTelemetry(const QString& id)
{
    qDebug() << ">>> [C++] 显示遥测数据:" << id;

    // ★ 设置当前正在观测的实体 ID（QML 用于高亮数据按钮）
    m_activeDataEntityId = id;
    emit activeDataEntityIdChanged();

    // ★ 【关键修复】无论进程是否已在运行，都必须先切换激活实体
    // setActiveEntity 内部会发送 {"Type":"init"} 包触发遥测端重置数据
    if (m_telemetryForwarder)
    {
        m_telemetryForwarder->setActiveEntity(id);
        qDebug() << ">>> [C++] 已切换遥测激活实体并发送 reset 包:" << id;
    }

    // ★ 检查进程是否已在运行 —— 只有不在运行时才需要启动
    if (m_telemetryProcess && m_telemetryProcess->state() == QProcess::Running)
    {
        qDebug() << ">>> 遥测程序已在运行，跳过重新启动（reset 包已发送）";
        return;
    }

    // 创建新进程（如果之前的已结束）
    if (!m_telemetryProcess)
    {
        m_telemetryProcess = new QProcess(this);
    }

    // 优先查找环境变量 TELEMETRY_EXE，其次在程序目录旁查找
    QString telemetryExe;
    const char* envExe = std::getenv("TELEMETRY_EXE");
    if (envExe && std::strlen(envExe) > 0)
    {
        telemetryExe = QString::fromUtf8(envExe);
    }
    else
    {
        // 尝试与主程序同目录
        telemetryExe = QCoreApplication::applicationDirPath() + "/appchart_merged.exe";
    }

    if (QFile::exists(telemetryExe))
    {
        qDebug() << ">>> 启动外部遥测程序:" << telemetryExe;
        
        // 【关键修复】：防父进程“环境变量投毒”！
        // 主程序 vsgQt 在 main() 里全局强行设了 qputenv("QT_QUICK_BACKEND", "software");
        // 这会导致被它拉起的子进程也用软件渲染，Chromium 的核心硬加速(WebGL)因为没有真实GPU通道而当场暴毙！
        // 所以我们必须把系统的正常环境变量克隆一份，把 QT_QUICK_BACKEND 删掉，再给子进程用。
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("QT_QUICK_BACKEND");
        env.remove("QT_QUICK_CONTROLS_STYLE"); // 阻止按钮退化为 Basic 纯平样式
        m_telemetryProcess->setProcessEnvironment(env);

        m_telemetryProcess->start(telemetryExe, QStringList());
    }
    else
    {
        qWarning() << ">>> 遥测程序不存在:" << telemetryExe;
    }
}

void QmlBridge::cancelPlacement()
{
    g_appState.isPlacingMode = false;
    g_appState.pendingModelPath = "";
    g_appState.pendingModelType = "";
    if (m_window)
    {
        m_window->setCursor(Qt::ArrowCursor);
    }
}

void QmlBridge::minimizeWindow()
{
    if (m_window) {
        m_window->showMinimized();
    }
}

void QmlBridge::maximizeWindow()
{
    if (m_window) {
        if (m_window->windowState() & Qt::WindowMaximized) {
            m_window->showNormal();
        } else {
            m_window->showMaximized();
        }
        emit windowStateChanged();
    }
}

void QmlBridge::closeWindow()
{
    if (m_window) {
        m_window->close();
    }
}

void QmlBridge::startWindowDrag()
{
    if (m_window) {
        m_window->startSystemMove();
    }
}

double QmlBridge::simProgress() const
{
    return _clock ? _clock->progress() : 0.0;
}
QString QmlBridge::simCurrentTime() const
{
    return _clock ? _clock->timeStringAt(_clock->progress()) : "";
}
double QmlBridge::simTotalSeconds() const
{
    return _clock ? _clock->totalSeconds() : 0.0;
}
QString QmlBridge::simStartTime() const
{
    return _clock ? _clock->timeStringAt(0.0) : "";
}
QString QmlBridge::simStopTime() const
{
    return _clock ? _clock->timeStringAt(1.0) : "";
}
bool QmlBridge::simAnimating() const
{
    return _clock ? _clock->isAnimating() : false;
}
double QmlBridge::simMultiplier() const
{
    return _clock ? _clock->multiplier() : 1.0;
}
double QmlBridge::maxBufferedProgress() const
{
    return m_maxBufferedProgress;
}

void QmlBridge::updateCamera(double lon, double lat, double alt, double fov, double pitch, double heading, double viewH, const QString& scale)
{
    m_longitude = lon;
    m_latitude = lat;
    m_altitude = alt;
    m_fov = fov;
    m_pitch = pitch;
    m_heading = heading;
    m_viewHeight = viewH;
    m_scaleText = scale;
    emit cameraChanged();
}

void QmlBridge::updatePerf(double ms, int fps)
{
    m_frameMs = ms;
    m_fps = fps;
    emit perfChanged();
}

void QmlBridge::setSimClock(SimClock* clock)
{
    _clock = clock;
    if (_clock)
    {
        connect(_clock, &SimClock::timeChanged, this, &QmlBridge::simTimeChanged);
        connect(_clock, &SimClock::rangeChanged, this, &QmlBridge::simRangeChanged);
        connect(_clock, &SimClock::animatingChanged, this, &QmlBridge::simAnimatingChanged);
        connect(_clock, &SimClock::multiplierChanged, this, &QmlBridge::simMultiplierChanged);
    }
}

void QmlBridge::seekToProgress(double p)
{
    if (_clock) _clock->seekToProgress(p);
}

QString QmlBridge::timeStringAt(double p) const
{
    if (_clock) return _clock->timeStringAt(p);
    return "";
}

void QmlBridge::setSimAnimating(bool animating)
{
    if (_clock) _clock->setAnimating(animating);
}

void QmlBridge::setSimMultiplier(double multiplier)
{
    if (_clock) _clock->setMultiplier(multiplier);
}

void QmlBridge::stopSim()
{
    if (_clock) _clock->stop();
}

void QmlBridge::adjustTimelineCapacity(double delta)
{
    if (_clock)
    {
        // 滚轮向上(delta > 0)：时间条容量变长
        // 滚轮向下(delta < 0)：时间条容量变短
        double factor = (delta > 0) ? 1.2 : 0.8333;
        _clock->adjustTimeSpan(factor);
    }
}

void QmlBridge::updateAcmiStats(int count, bool bufferMode)
{
    if (m_acmiPacketCount != count || m_acmiBufferMode != bufferMode)
    {
        m_acmiPacketCount = count;
        m_acmiBufferMode = bufferMode;
        emit acmiStatsChanged();
    }
}

void QmlBridge::updateMaxBufferedProgress(double progress)
{
    if (m_maxBufferedProgress != progress)
    {
        m_maxBufferedProgress = progress;
        emit maxBufferedProgressChanged();
    }
}
