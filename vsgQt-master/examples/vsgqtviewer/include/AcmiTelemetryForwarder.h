#pragma once

#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUdpSocket>
#include <QThread>
#include <QTimer>
#include <cmath>

class AcmiTelemetryForwarder : public QObject
{
    Q_OBJECT
public:
    explicit AcmiTelemetryForwarder(QObject* parent = nullptr) : QObject(parent)
    {
        _udpSocket = new QUdpSocket(this);
        _targetAddress = QHostAddress("127.0.0.1");
        _targetPort = 45460;
    }

    void setActiveEntity(const QString& id)
    {
        _activeEntityId = id;
        qDebug() << "[Telemetry] 激活遥测实体:" << id;
        _lastState.clear();
        _meshInitialized = false;
        
        // 【修复 2b：防止异步时序雪崩】
        // 标记为还不能发 mesh
        _meshReady = false;

        // 发送初始化包
        sendInitPacket(0.0);
        
        // 延迟 500ms 后才允许推送 Mesh 数据，给予前端充足的清空、动画时间
        QTimer::singleShot(500, this, [this]() {
            _meshReady = true;
            qDebug() << "[Telemetry] 500ms 初始化延时结束，允许 Mesh 投递";
        });
    }

    QString activeEntityId() const { return _activeEntityId; }

    // 回滚/Seek 时调用：发送 init 包清空遥测端数据，并重置速度计算状态
    void sendResetPacket()
    {
        if (_activeEntityId.isEmpty()) return;
        sendInitPacket(0.0);
        _lastState.clear();  // 防止回滚后出现异常大速度
        qDebug() << "[Telemetry] 发送 reset 包 (seek/rollback)";
    }

    void forward(const QString& entityId, double timeSec, double lat, double lon, double alt,
                 double yaw, double pitch, double roll)
    {
        if (entityId != _activeEntityId || _activeEntityId.isEmpty())
            return;

        // 根据 test_telemetry_system.py，2D 数据包包含 10 个左右的点比较合适
        // 但由于 forward 是每帧调用，我们可以每 10 帧发一包，或者每帧发一小包
        // 这里为了实时性，每帧发送包含 1 个点的 line 包，但确保 ID 和格式正确

        // 使用仿真时间生成字典，确保同步
        QJsonObject timeDict = getTimeDict(timeSec);

        // 1. Azimuth (Yaw)
        sendLinePoint(1, timeSec, yaw, timeDict);
        // 2. Pitch
        sendLinePoint(2, timeSec, pitch, timeDict);
        // 3. Distance (暂时用高度)
        sendLinePoint(3, timeSec, alt, timeDict);
        // 4. Speed
        double speed = calculateSpeed(entityId, timeSec, lat, lon, alt);
        sendLinePoint(4, timeSec, speed, timeDict);

        // 5. Mesh (由于 Mesh 更新消耗较大，可以降低频率，例如 2Hz)
        static int frameCount = 0;
        if (frameCount++ % 30 == 0 && _meshReady)
        {
            requestMeshUpdate(timeSec, timeDict);
        }
    }

private:
    QJsonObject getTimeDict(double simTimeSec)
    {
        QJsonObject obj;
        obj["timestamp"] = simTimeSec;
        obj["Time"] = simTimeSec;
        obj["time"] = simTimeSec;
        obj["t"] = simTimeSec;
        obj["T"] = simTimeSec;
        return obj;
    }

    void sendInitPacket(double simTimeSec)
    {
        QJsonObject obj;
        obj["Type"] = "init";
        obj.insert("timestamp", simTimeSec);

        _udpSocket->writeDatagram(QJsonDocument(obj).toJson(QJsonDocument::Compact), _targetAddress, _targetPort);
    }

    void sendLinePoint(int id, double x, double y, const QJsonObject& timeDict)
    {
        QJsonObject obj = timeDict;
        obj["Type"] = "line";
        obj["ID"] = id;

        QJsonArray dataArray;
        QJsonObject point;
        point["x"] = x;
        point["y"] = y;
        dataArray.append(point);
        obj["Data"] = dataArray;

        _udpSocket->writeDatagram(QJsonDocument(obj).toJson(QJsonDocument::Compact), _targetAddress, _targetPort);
    }

    void requestMeshUpdate(double timeSec, const QJsonObject& timeDict)
    {
        // qDebug() << "[Mesh] >>> requestMeshUpdate 开始, timeSec=" << timeSec 
        //         << "target=" << _targetAddress.toString() << ":" << _targetPort;

        int rows = 30, cols = 30;
        QJsonObject init = timeDict;
        init["Type"] = "mesh_init";
        init["ID"] = 5;
        init["rows"] = rows;
        init["cols"] = cols;
        init["func_type"] = "wave";
        QByteArray initData = QJsonDocument(init).toJson(QJsonDocument::Compact);
        qint64 initBytes = _udpSocket->writeDatagram(initData, _targetAddress, _targetPort);
        // qDebug() << "[Mesh] mesh_init 已发送," << initBytes << "bytes, 内容:" << initData.left(200);
        
        QThread::msleep(2); // ★【给遥测前端的救命药】强迫暂停 2ms，让 ECharts 引擎有时间在收到后续切片前立刻重置缓冲区！兜底网络并发导致的乱序
        
        QJsonArray zValues;
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                double z = 5.0 * std::sin(r * 0.2 + timeSec) * std::cos(c * 0.2 + timeSec);
                zValues.append(z);
            }
        }
        
        int chunkSize = 100;
        int totalPoints = rows * cols;
        int totalPackets = (totalPoints + chunkSize - 1) / chunkSize;
        // qDebug() << "[Mesh] totalPoints=" << totalPoints << "totalPackets=" << totalPackets;
        
        double rollingTs = timeDict["timestamp"].toDouble();
        
        for (int i = 0; i < totalPackets; ++i)
        {
            rollingTs += 0.001; // ✅ 每包递增，严格单调
            
            QJsonObject data;
            // 全量更新所有时间字段
            data["timestamp"] = rollingTs;
            data["Time"] = rollingTs;
            data["time"] = rollingTs;
            data["t"] = rollingTs;
            data["T"] = rollingTs;
            
            data["Type"] = "mesh_z_values";
            data["ID"] = 5;
            data["packet_index"] = i;
            data["total_packets"] = totalPackets;
            
            QJsonArray chunk;
            int startIndex = i * chunkSize;
            for (int j = 0; j < chunkSize && (startIndex + j) < totalPoints; ++j)
            {
                chunk.append(zValues[startIndex + j]);
            }
            data["z_values"] = chunk;
            _udpSocket->writeDatagram(QJsonDocument(data).toJson(QJsonDocument::Compact), _targetAddress, _targetPort);
            
            QThread::msleep(1); 
        }
        
        // 3. 发 mesh_finish ── 继续递增，绝不回头
        rollingTs += 0.001; // 比最后一个 chunk 还要新
        
        QJsonObject fin;
        fin["timestamp"] = rollingTs;
        fin["Time"] = rollingTs;
        fin["time"] = rollingTs;
        fin["t"] = rollingTs;
        fin["T"] = rollingTs;
        fin["Type"] = "mesh_finish";
        fin["ID"] = 5;
        qint64 finBytes = _udpSocket->writeDatagram(QJsonDocument(fin).toJson(QJsonDocument::Compact), _targetAddress, _targetPort);
        // qDebug() << "[Mesh] <<< mesh_finish 已发送," << finBytes << "bytes, 完整周期结束";
    }
    double calculateSpeed(const QString& id, double time, double lat, double lon, double alt)
    {
        if (!_lastState.contains(id))
        {
            _lastState[id] = {time, lat, lon, alt};
            return 0.0;
        }

        auto last = _lastState[id];
        double dt = time - last.time;
        if (dt <= 1e-6) return 0.0;

        double dLat = (lat - last.lat) * 111319.9;
        double dLon = (lon - last.lon) * 111319.9 * std::cos(lat * M_PI / 180.0);
        double dAlt = alt - last.alt;

        double dist = std::sqrt(dLat * dLat + dLon * dLon + dAlt * dAlt);
        _lastState[id] = {time, lat, lon, alt};
        return dist / dt;
    }

    struct LastState
    {
        double time;
        double lat;
        double lon;
        double alt;
    };

    QUdpSocket* _udpSocket;
    QHostAddress _targetAddress;
    quint16 _targetPort;
    QString _activeEntityId;
    QMap<QString, LastState> _lastState;
    bool _meshInitialized = false;
    bool _meshReady = false;
};
