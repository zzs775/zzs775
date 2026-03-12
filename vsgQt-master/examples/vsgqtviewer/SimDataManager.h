#pragma once

#include <algorithm>
#include <vector>

#include "SimData.h"
#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QRandomGenerator>
#include <QThread>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#pragma pack(push, 1)
struct UdpDataPacket
{
    char id[16];
    char name[32]; // 新增名称字段，用于二进制直接传输
    double lat, lon, alt;
    double pitch, yaw, roll;
    bool destroyed;
    char color[32];
};
#pragma pack(pop)

// 插值后的数据快照点，用于消除阶梯效应
struct InterpolatedPacket
{
    std::string id;
    double lat, lon, alt;
    double pitch, yaw, roll;
    std::string name;
    std::string color;
};

class SimDataManager : public QAbstractListModel
{
    Q_OBJECT

public:
    enum SimDataRoles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        LatRole,
        LonRole,
        AltRole,
        PosRole,
        HeadingRole,
        SpeedRole,
        YawRole,
        PitchRole,
        RollRole
    };

    explicit SimDataManager(QObject* parent = nullptr);
    ~SimDataManager() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    Q_PROPERTY(double maxReceivedTime READ maxReceivedTime NOTIFY maxReceivedTimeChanged)

    double maxReceivedTime() const { return m_maxReceivedTime; }

    Q_INVOKABLE void addEntity(ModelType type, double lat, double lon, double alt);
    Q_INVOKABLE void addEntityWithName(const QString& name, ModelType type, double lat, double lon, double alt);
    Q_INVOKABLE bool removeEntity(const QString& id);
    Q_INVOKABLE bool updateEntityPosition(const QString& id, double lat, double lon, double alt, double yaw = 0.0, double pitch = 0.0, double roll = 0.0);
    Q_INVOKABLE QVariantMap getEntity(const QString& id) const;
    Q_INVOKABLE int count() const;
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE QVariantList getEntitiesByType(const QString& typeFilter) const;

    const QList<SimEntityData>& getAllData() const { return m_data; }
    SimEntityData* findEntityById(const QString& id);

    // ===== 现代二进制 UDP 轮询系统 (Step 2) =====
    void startUdpReceiver(quint16 port = 19999);
    void stopUdpReceiver();
    std::unordered_map<std::string, UdpDataPacket> getLatestUdpData();

    // 内部创建实体（供回放系统发现新 ID 时自动调用）
    void addEntityWithId(const QString& id, const QString& name, ModelType type, double lat, double lon, double alt);

    // ===== ACMI 缓冲回放系统 =====
    struct AcmiFrame
    {
        double time;                                             // ACMI 时间偏移（秒）
        std::unordered_map<std::string, UdpDataPacket> entities; // 该时刻所有实体快照
    };

    // 获取指定时刻的实体数据快照（消费者接口）
    std::unordered_map<std::string, UdpDataPacket> getDataAtTime(double timeSec);

    // 新增：获取线性插值后的数据点（解决一抽一抽的问题）
    std::vector<InterpolatedPacket> getInterpolatedData(double timeSec);

    // 状态查询
    int acmiPacketCount() const { return m_acmiPacketCount.load(); }
    double acmiTimeMin() const;
    double acmiTimeMax() const;
    bool isAcmiBufferMode() const { return m_acmiBufferMode.load(); }

    // ACMI 解析器
    void parseAcmiLine(const std::string& line);

signals:
    void maxReceivedTimeChanged(double time);
    void sceneResetRequested();
    void entityAdded(const QString& id);
    void entityRemoved(const QString& id);
    void entityUpdated(const QString& id);

private:
    // 现代二进制 UDP 接收线程
    void udpThreadFunc(quint16 port);
    std::thread m_udpThread;
    std::atomic<bool> m_udpThreadRunning{false};
    mutable std::mutex m_dataMutex;
    std::unordered_map<std::string, UdpDataPacket> m_latestDataMap;

    bool _entityExists(const QString& id) const;
    ModelType _inferModelType(const QString& name) const;

    // 通用
    QList<SimEntityData> m_data;
    QString generateIDForName(const QString& baseName) const;
    std::unordered_set<std::string> m_excludedIds; // 被用户手动删除的 ID 列表，不再通过 UDP 重放恢复

    // ACMI 实体元数据缓存
    std::unordered_map<std::string, std::string> m_entityNames;
    std::unordered_map<std::string, std::string> m_entityColors;

    // ===== ACMI 缓冲回放数据 =====
    std::vector<AcmiFrame> m_acmiFrames;       // 时间有序帧缓冲区（生产者写、消费者读）
    double m_currentParseTime = -1.0;          // 当前正在解析的 ACMI 时间戳
    double m_acmiTimeMinVal = 0.0;             // 已接收数据的最小时间
    double m_acmiTimeMaxVal = 0.0;             // 已接收数据的最大时间
    std::atomic<int> m_acmiPacketCount{0};     // 已接收的 ACMI 数据包总数
    std::atomic<bool> m_acmiBufferMode{false}; // 是否检测到 ACMI 时间戳（启用缓冲模式）
    double m_maxReceivedTime = 0.0;            // 已缓冲的最大时间
};
