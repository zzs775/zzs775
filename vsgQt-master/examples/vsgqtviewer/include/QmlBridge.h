#pragma once
#include <QObject>
#include <QString>
#include <QWindow>

struct AppState
{
    QString pendingModelType = "";
    QString pendingModelPath = "";
    bool isPlacingMode = false;
    QString followedTargetId = "";
    bool _isWaitingForBuffer = false; // ★ 内部状态：是否因为缓冲垫底而自动暂停
};

class SimClock;

class QmlBridge : public QObject
{
    Q_OBJECT

    // 第一组，相机状态栏数据
    Q_PROPERTY(double longitude READ longitude NOTIFY cameraChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY cameraChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY cameraChanged)
    Q_PROPERTY(double fov READ fov NOTIFY cameraChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY cameraChanged)
    Q_PROPERTY(double heading READ heading NOTIFY cameraChanged)
    Q_PROPERTY(double viewHeight READ viewHeight NOTIFY cameraChanged)
    Q_PROPERTY(QString scaleText READ scaleText NOTIFY cameraChanged)
    Q_PROPERTY(double frameMs READ frameMs NOTIFY perfChanged)
    Q_PROPERTY(int fps READ fps NOTIFY perfChanged)

    // 第二组，时间轴数据
    Q_PROPERTY(double simProgress READ simProgress NOTIFY simTimeChanged)
    Q_PROPERTY(QString simCurrentTime READ simCurrentTime NOTIFY simTimeChanged)
    Q_PROPERTY(double simTotalSeconds READ simTotalSeconds NOTIFY simRangeChanged)
    Q_PROPERTY(QString simStartTime READ simStartTime NOTIFY simRangeChanged)
    Q_PROPERTY(QString simStopTime READ simStopTime NOTIFY simRangeChanged)
    Q_PROPERTY(bool simAnimating READ simAnimating NOTIFY simAnimatingChanged)
    Q_PROPERTY(double simMultiplier READ simMultiplier NOTIFY simMultiplierChanged)

    // 第三组，ACMI 数据接收状态
    Q_PROPERTY(int acmiPacketCount READ acmiPacketCount NOTIFY acmiStatsChanged)
    Q_PROPERTY(bool acmiBufferMode READ acmiBufferMode NOTIFY acmiStatsChanged)
    Q_PROPERTY(double maxBufferedProgress READ maxBufferedProgress NOTIFY maxBufferedProgressChanged)
    Q_PROPERTY(bool isMaximized READ isMaximized NOTIFY windowStateChanged)

    // 第四组：当前正在观测遥测数据的实体 ID（用于数据按钮高亮）
    Q_PROPERTY(QString activeDataEntityId READ activeDataEntityId NOTIFY activeDataEntityIdChanged)

public:
    explicit QmlBridge(QObject* parent = nullptr) : QObject(parent) {}

    void setBackgroundWindow(QWindow* window) { m_window = window; }

    Q_INVOKABLE void requestAddModel(const QString& modelType);
    Q_INVOKABLE void selectModel(const QString& fileName, const QString& category, const QString& fullPath);
    Q_INVOKABLE void focusEntity(const QString& id);
    Q_INVOKABLE void removeEntity(const QString& id);
    Q_INVOKABLE void cancelPlacement();
    Q_INVOKABLE void untether();
    Q_INVOKABLE void showTelemetry(const QString& id);

    Q_INVOKABLE void minimizeWindow();
    Q_INVOKABLE void maximizeWindow();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void startWindowDrag();

    // READ for Phase 1
    double longitude() const { return m_longitude; }
    double latitude() const { return m_latitude; }
    double altitude() const { return m_altitude; }
    double fov() const { return m_fov; }
    double pitch() const { return m_pitch; }
    double heading() const { return m_heading; }
    double viewHeight() const { return m_viewHeight; }
    QString scaleText() const { return m_scaleText; }
    double frameMs() const { return m_frameMs; }
    int fps() const { return m_fps; }
    int acmiPacketCount() const { return m_acmiPacketCount; }
    bool acmiBufferMode() const { return m_acmiBufferMode; }
    QString activeDataEntityId() const { return m_activeDataEntityId; }

    // READ and Invokables for Phase 2
    double simProgress() const;
    QString simCurrentTime() const;
    double simTotalSeconds() const;
    QString simStartTime() const;
    QString simStopTime() const;
    bool simAnimating() const;
    double simMultiplier() const;
    double maxBufferedProgress() const;
    bool isMaximized() const { return m_window && (m_window->windowState() & Qt::WindowMaximized); }

    void updateCamera(double lon, double lat, double alt, double fov, double pitch, double heading, double viewH, const QString& scale);
    void updatePerf(double ms, int fps);
    void setSimClock(SimClock* clock);
    void updateMaxBufferedProgress(double progress);

    Q_INVOKABLE void seekToProgress(double p);
    Q_INVOKABLE QString timeStringAt(double p) const;

    Q_INVOKABLE void setSimAnimating(bool animating);
    Q_INVOKABLE void setSimMultiplier(double multiplier);
    Q_INVOKABLE void stopSim();

    Q_INVOKABLE void adjustTimelineCapacity(double delta);

    void updateAcmiStats(int count, bool bufferMode);

    class AcmiTelemetryForwarder* telemetryForwarder() { return m_telemetryForwarder; }
    void setTelemetryForwarder(class AcmiTelemetryForwarder* f) { m_telemetryForwarder = f; }

signals:
    void modelPlacementRequested(const QString& fileName, const QString& fullPath);
    void focusEntityRequested(const QString& id);
    void targetUntethered();

    void cameraChanged();
    void perfChanged();
    void simTimeChanged();
    void simRangeChanged();
    void simAnimatingChanged();
    void simMultiplierChanged();
    void acmiStatsChanged();
    void maxBufferedProgressChanged();
    void windowStateChanged();
    void activeDataEntityIdChanged();

private:
    QWindow* m_window = nullptr;
    SimClock* _clock = nullptr;
    class QProcess* m_telemetryProcess = nullptr;  // ★ 追踪遥测进程状态

    double m_longitude = 0.0;
    double m_latitude = 0.0;
    double m_altitude = 0.0;
    double m_fov = 0.0;
    double m_pitch = 0.0;
    double m_heading = 0.0;
    double m_viewHeight = 0.0;
    QString m_scaleText = "0 m";
    double m_frameMs = 0.0;
    int m_fps = 0;
    int m_acmiPacketCount = 0;
    bool m_acmiBufferMode = false;
    double m_maxBufferedProgress = 0.0;
    class AcmiTelemetryForwarder* m_telemetryForwarder = nullptr;
    QString m_activeDataEntityId;
};
