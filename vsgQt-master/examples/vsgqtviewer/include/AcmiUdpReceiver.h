#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

// ACMI 实体数据包（JSON 解析后的结构体）
struct AcmiEntityPacket {
    QString id;             // 实体 ID（十进制字符串）
    double  lon   = 0.0;   // 经度（度）
    double  lat   = 0.0;   // 纬度（度）
    double  alt   = 0.0;   // 高度（米，WGS84 椭球面以上）
    double  roll  = 0.0;   // 横滚角（度）
    double  pitch = 0.0;   // 俯仰角（度）
    double  yaw   = 0.0;   // 航向角（度，从北顺时针）
    QString name;           // 模型名称，如 'F-16A'
    QString color;          // 阵营，如 'RedTeam'
    double  time  = 0.0;   // 仿真时间（秒）
    bool    valid = false;  // 解析是否成功
    bool    destroyed = false; // 是否已销毁
};

// 注册到 Qt 元类型系统，以便跨线程信号槽传递
Q_DECLARE_METATYPE(AcmiEntityPacket)

/**
 * AcmiUdpReceiver - 独立子线程 UDP JSON 接收器
 *
 * 使用方式：
 *   auto thread = new QThread(this);
 *   auto receiver = new AcmiUdpReceiver(19999);
 *   receiver->moveToThread(thread);
 *   connect(thread, &QThread::started, receiver, &AcmiUdpReceiver::startListening);
 *   connect(receiver, &AcmiUdpReceiver::entityPacketReceived, ...槽函数...);
 *   thread->start();
 */
class AcmiUdpReceiver : public QObject {
    Q_OBJECT
public:
    explicit AcmiUdpReceiver(quint16 port, QObject* parent = nullptr)
        : QObject(parent), _port(port) {}

    ~AcmiUdpReceiver() override {
        if (_socket) {
            _socket->close();
            delete _socket;
            _socket = nullptr;
        }
    }

    // 静态 JSON 解析方法（纯函数，无状态，线程安全）
    static AcmiEntityPacket parseJson(const QByteArray& data) {
        AcmiEntityPacket pkt;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "[ACMI JSON] 解析错误:" << err.errorString() << "数据内容:" << data;
            return pkt; // valid = false
        }

        QJsonObject obj = doc.object();
        pkt.id    = obj.value("id").toString();
        pkt.lon   = obj.value("lon").toDouble();
        pkt.lat   = obj.value("lat").toDouble();
        pkt.alt   = obj.value("alt").toDouble();
        pkt.roll  = obj.value("roll").toDouble();
        pkt.pitch = obj.value("pitch").toDouble();
        pkt.yaw   = obj.value("yaw").toDouble();
        pkt.name  = obj.value("name").toString();
        pkt.color = obj.value("color").toString();
        pkt.time  = obj.value("time").toDouble();
        pkt.destroyed = obj.value("destroyed").toBool(false);
        pkt.valid = !pkt.id.isEmpty();

        return pkt;
    }

public slots:
    void startListening() {
        _socket = new QUdpSocket(this);
        if (!_socket->bind(QHostAddress::Any, _port)) {
            qWarning() << "[AcmiUdpReceiver] 绑定 UDP 端口失败:" << _port;
            return;
        }
        qDebug() << "[AcmiUdpReceiver] 正在监听 UDP 端口:" << _port;

        // DirectConnection: 在当前线程（子线程）中执行 onReadyRead
        connect(_socket, &QUdpSocket::readyRead,
                this, &AcmiUdpReceiver::onReadyRead,
                Qt::DirectConnection);
    }

signals:
    // 跨线程信号：连接时使用 Qt::QueuedConnection
    void entityPacketReceived(AcmiEntityPacket pkt);

private slots:
    void onReadyRead() {
        while (_socket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(_socket->pendingDatagramSize());
            _socket->readDatagram(datagram.data(), datagram.size());

            // 调试：打印收到的原始字节大小
            // qDebug() << "[ACMI UDP] 收到字节数:" << datagram.size();

            AcmiEntityPacket pkt = parseJson(datagram);
            if (pkt.valid) {
                emit entityPacketReceived(pkt);
            }
        }
    }

private:
    QUdpSocket* _socket = nullptr;
    quint16     _port;
};
