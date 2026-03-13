#include "QmlBridge.h"
#include "SimClock.h"
#include <QCursor>
#include <QDebug>

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
