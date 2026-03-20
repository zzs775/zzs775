#include "SimDataManager.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
#include <QUdpSocket>
#include <QUuid>
#include <sstream>

SimDataManager::SimDataManager(QObject* parent) : QAbstractListModel(parent)
{
    // C++ 中不再主动调用 startUdpReceiver()，由 main.cpp 统一定义
}

SimDataManager::~SimDataManager()
{
    stopUdpReceiver();
}

int SimDataManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_data.count();
}

QVariant SimDataManager::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() < 0 || index.row() >= m_data.count())
        return QVariant();

    const auto& item = m_data.at(index.row());

    switch (role)
    {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case TypeRole:
        return SimEntityData::typeToString(item.type);
    case LatRole:
        return item.lat;
    case LonRole:
        return item.lon;
    case AltRole:
        return item.alt;
    case PosRole:
        return QString("N%1, E%2, Alt:%3")
            .arg(item.lat, 0, 'f', 4)
            .arg(item.lon, 0, 'f', 4)
            .arg(item.alt, 0, 'f', 1);
    case HeadingRole:
        return item.heading;
    case SpeedRole:
        return item.speed;
    case YawRole:
        return item.yaw;
    case PitchRole:
        return item.pitch;
    case RollRole:
        return item.roll;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SimDataManager::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "entityId";
    roles[NameRole] = "entityName";
    roles[TypeRole] = "entityType";
    roles[LatRole] = "entityLat";
    roles[LonRole] = "entityLon";
    roles[AltRole] = "entityAlt";
    roles[PosRole] = "entityPos";
    roles[HeadingRole] = "entityHeading";
    roles[SpeedRole] = "entitySpeed";
    roles[YawRole] = "entityYaw";
    roles[PitchRole] = "entityPitch";
    roles[RollRole] = "entityRoll";
    return roles;
}

void SimDataManager::addEntity(ModelType type, double lat, double lon, double alt)
{
    QString name = SimEntityData::defaultNameForType(type);
    QString id = generateIDForName(name);

    beginInsertRows(QModelIndex(), m_data.count(), m_data.count());

    SimEntityData entity(id, type, lat, lon, alt);
    m_data.append(entity);

    endInsertRows();

    qDebug() << "[SimDataManager] Entity added:" << id
             << "Type:" << SimEntityData::typeToString(type)
             << "Pos:" << lat << lon << alt;

    emit entityAdded(id);
}

void SimDataManager::addEntityWithName(const QString& name, ModelType type, double lat, double lon, double alt)
{
    QString id = generateIDForName(name);

    beginInsertRows(QModelIndex(), m_data.count(), m_data.count());

    SimEntityData entity(id, type, lat, lon, alt);
    entity.name = name;
    m_data.append(entity);

    endInsertRows();

    qDebug() << "[SimDataManager] Entity added with name:" << name << "ID:" << id;

    emit entityAdded(id);
}

bool SimDataManager::removeEntity(const QString& id, bool blacklist)
{
    for (int i = 0; i < m_data.count(); ++i)
    {
        if (m_data.at(i).id == id)
        {
            beginRemoveRows(QModelIndex(), i, i);
            m_data.removeAt(i);
            endRemoveRows();

            // 只有手动删除才加入黑名单，自动超时删除不加（否则回放倒退回不来）
            if (blacklist)
            {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                m_excludedIds.insert(id.toStdString());
                m_latestDataMap.erase(id.toStdString());
            }

            qDebug() << "[SimDataManager] Entity removed" << (blacklist ? "and blacklisted:" : ":") << id;
            emit entityRemoved(id);
            return true;
        }
    }

    qWarning() << "[SimDataManager] Entity not found for removal:" << id;
    return false;
}

bool SimDataManager::updateEntityPosition(const QString& id, double lat, double lon, double alt, double yaw, double pitch, double roll)
{
    for (int i = 0; i < m_data.count(); ++i)
    {
        if (m_data.at(i).id == id)
        {
            if (m_data[i].lat != lat || m_data[i].lon != lon || m_data[i].alt != alt ||
                m_data[i].yaw != yaw || m_data[i].pitch != pitch || m_data[i].roll != roll)
            {
                m_data[i].lat = lat;
                m_data[i].lon = lon;
                m_data[i].alt = alt;
                m_data[i].yaw = yaw;
                m_data[i].pitch = pitch;
                m_data[i].roll = roll;

                QModelIndex idx = index(i);
                emit dataChanged(idx, idx, {LatRole, LonRole, AltRole, PosRole, YawRole, PitchRole, RollRole});

                emit entityUpdated(id);
            }
            return true;
        }
    }

    qWarning() << "[SimDataManager] Entity not found for update:" << id;
    return false;
}

QVariantMap SimDataManager::getEntity(const QString& id) const
{
    QVariantMap result;

    for (const auto& item : m_data)
    {
        if (item.id == id)
        {
            result["id"] = item.id;
            result["name"] = item.name;
            result["type"] = SimEntityData::typeToString(item.type);
            result["lat"] = item.lat;
            result["lon"] = item.lon;
            result["alt"] = item.alt;
            result["heading"] = item.heading;
            result["speed"] = item.speed;
            break;
        }
    }

    return result;
}

int SimDataManager::count() const
{
    return m_data.count();
}

void SimDataManager::clearAll()
{
    if (m_data.isEmpty())
        return;

    beginResetModel();
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        for (const auto& entity : m_data)
        {
            m_excludedIds.insert(entity.id.toStdString());
        }
        m_data.clear();
        m_latestDataMap.clear();
    }
    endResetModel();

    qDebug() << "[SimDataManager] All entities cleared and blacklisted";
}

SimEntityData* SimDataManager::findEntityById(const QString& id)
{
    for (int i = 0; i < m_data.count(); ++i)
    {
        if (m_data[i].id == id)
        {
            return &m_data[i];
        }
    }
    return nullptr;
}

QString SimDataManager::generateIDForName(const QString& baseName) const
{
    int count = 1;
    for (const auto& entity : m_data)
    {
        if (entity.id.startsWith(baseName + "_"))
        {
            count++;
        }
    }
    return QString("%1_%2").arg(baseName).arg(count, 2, 10, QChar('0'));
}

QVariantList SimDataManager::getEntitiesByType(const QString& typeFilter) const
{
    QVariantList result;
    for (const auto& item : m_data)
    {
        QString typeStr = SimEntityData::typeToString(item.type);
        bool match = false;
        if (typeFilter == "全部")
        {
            match = true;
        }
        else if (typeFilter == "飞机")
        {
            match = (item.type == ModelType::Aircraft);
        }
        else if (typeFilter == "导弹")
        {
            match = (item.type == ModelType::Missile);
        }
        else
        {
            // "其他" 类别：排除飞机和导弹
            match = (item.type != ModelType::Aircraft && item.type != ModelType::Missile);
        }
        if (match)
        {
            QVariantMap map;
            map["id"] = item.id;
            map["name"] = item.name;
            map["type"] = typeStr;
            map["lat"] = item.lat;
            map["lon"] = item.lon;
            map["alt"] = item.alt;
            map["heading"] = item.heading;
            map["speed"] = item.speed;
            map["yaw"] = item.yaw;
            map["pitch"] = item.pitch;
            map["roll"] = item.roll;
            result.append(map);
        }
    }
    return result;
}
std::unordered_map<std::string, UdpDataPacket> SimDataManager::getLatestUdpData()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_latestDataMap;
}

void SimDataManager::startUdpReceiver(quint16 port)
{
    m_udpThreadRunning = true;
    m_udpThread = std::thread(&SimDataManager::udpThreadFunc, this, port);
}

void SimDataManager::stopUdpReceiver()
{
    m_udpThreadRunning = false;
    if (m_udpThread.joinable())
    {
        m_udpThread.join();
    }
}

void SimDataManager::udpThreadFunc(quint16 port)
{
    QUdpSocket socket;
    if (!socket.bind(QHostAddress::Any, port))
    {
        qWarning() << "[SimDataManager] Failed to bind UDP port" << port;
        return;
    }
    qDebug() << "[SimDataManager] Listening for UDP on port" << port;

    while (m_udpThreadRunning)
    {
        if (socket.waitForReadyRead(100))
        {
            while (socket.hasPendingDatagrams())
            {
                QByteArray datagram;
                datagram.resize(socket.pendingDatagramSize());
                socket.readDatagram(datagram.data(), datagram.size());

                // 兼容二进制 UdpDataPacket
                if (datagram.size() == sizeof(UdpDataPacket))
                {
                    UdpDataPacket packet;
                    memcpy(&packet, datagram.constData(), sizeof(UdpDataPacket));
                    // 确保字符串安全
                    packet.id[15] = '\0';
                    packet.name[31] = '\0';

                    std::lock_guard<std::mutex> lock(m_dataMutex);
                    std::string idStr(packet.id);
                    // 处理实体销毁 或 已被排除
                    if (packet.destroyed || m_excludedIds.count(idStr))
                    {
                        m_latestDataMap.erase(idStr);
                    }
                    else
                    {
                        m_latestDataMap[idStr] = packet;
                    }
                }
                else
                {
                    // 尝试作为 ACMI 字符串解析
                    std::string line(datagram.constData(), datagram.size());
                    parseAcmiLine(line);
                }
            }
        }
    }
}

// 核心解析函数：逐行解析 ACMI 文件内容
// ACMI 文件每一行通常是一个时间戳标记(如 #10.0)或一个实体更新(如 320,T=121.1|31.2|...)
void SimDataManager::parseAcmiLine(const std::string& lineStr)
{
    // 跳过空行和文件头
    if (lineStr.empty() || lineStr.find("FileType=") == 0 || lineStr.find("FileVersion=") == 0)
    {
        return;
    }

    // ===== 全局属性与仿真重置 =====
    if (lineStr.find("SimState=Start") != std::string::npos)
    {
        qDebug() << "[SimDataManager] 检测到 SimState=Start，准备清空所有实体";
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_latestDataMap.clear();
            m_acmiFrames.clear();
            m_excludedIds.clear();
            m_acmiTimeMinVal = 0.0;
            m_acmiTimeMaxVal = 0.0;
            m_currentParseTime = -1.0;
            m_maxReceivedTime = 0.0;
            m_acmiPacketCount.store(0);
            m_entityColors.clear();
            m_entityNames.clear();
            m_entityLastUpdateTime.clear();
        }
        emit sceneResetRequested();
        return;
    }

    // ===== 处理时间戳行: #seconds =====
    if (lineStr[0] == '#')
    {
        std::string timeStr = lineStr.substr(1);
        // 去除换行符
        timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\r'), timeStr.end());
        timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());

        double newTime = 0.0;
        try
        {
            newTime = std::stod(timeStr);
        }
        catch (...)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        // 【核心：时间序列快照存储】
        // 如果有上一帧数据，将其快照存入缓冲区，用于后续的平滑插值计算
        if (m_currentParseTime >= 0.0 && !m_latestDataMap.empty())
        {
            // [恢复失联清理]
            // 为了防止没有发显式 '-ID' 删除指令的模型（如某些导弹命中后直接不发数据）永远卡在天空中，
            // 必须进行超时清理。但是阈值不能太短，ACMI 匀速直线时可能有几秒不发更新。
            // 这里设置为 10.0 秒。如果 10 秒钟内都没有任何更新且没发 '-ID'，我们认为它死了。
            double staleThreshold = 10.0;

            for (auto it = m_latestDataMap.begin(); it != m_latestDataMap.end();)
            {
                auto uit = m_entityLastUpdateTime.find(it->first);
                if (uit == m_entityLastUpdateTime.end() || (m_currentParseTime - uit->second) > staleThreshold)
                {
                    it = m_latestDataMap.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            AcmiFrame frame;
            frame.time = m_currentParseTime;
            frame.entities = m_latestDataMap;
            m_acmiFrames.push_back(std::move(frame));
        }

        m_currentParseTime = newTime;

        if (newTime > m_maxReceivedTime)
        {
            m_maxReceivedTime = newTime;
            // 通知 QML 更新时间轴白线（可见缓冲进度）
            emit maxReceivedTimeChanged(m_maxReceivedTime);
        }

        // 更新时间范围
        if (m_acmiFrames.empty())
        {
            m_acmiTimeMinVal = newTime;
            m_acmiTimeMaxVal = newTime;
        }
        else
        {
            if (newTime < m_acmiTimeMinVal) m_acmiTimeMinVal = newTime;
            if (newTime > m_acmiTimeMaxVal) m_acmiTimeMaxVal = newTime;
        }
        return;
    }

    // 跳过全局对象行 (ID=0)
    if (lineStr.find("0,") == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_dataMutex);

    // 实体销毁: -ID
    if (lineStr[0] == '-')
    {
        std::string rawId = lineStr.substr(1);
        // 去除可能的换行符
        rawId.erase(std::remove(rawId.begin(), rawId.end(), '\r'), rawId.end());
        rawId.erase(std::remove(rawId.begin(), rawId.end(), '\n'), rawId.end());
        // ★ 保留原始十六进制 ID，不进行转换，与 ACMI 文件中的展示保持一致
        m_latestDataMap.erase(rawId);
        return;
    }

    // 数据行: ID,T=...,Name=...,Color=...
    size_t commaPos = lineStr.find(',');
    if (commaPos == std::string::npos) return;

    std::string rawId = lineStr.substr(0, commaPos);
    // ★ 保留原始 ACMI ID（保持十六进制字符串，如 "320" "400" 等）
    //   不进行 hex → decimal 转换，确保列表中的 ID 与原始 ACMI 文件完全匹配
    rawId.erase(std::remove(rawId.begin(), rawId.end(), '\r'), rawId.end());
    rawId.erase(std::remove(rawId.begin(), rawId.end(), '\n'), rawId.end());

    if (m_excludedIds.count(rawId)) return;

    std::string rest = lineStr.substr(commaPos + 1);

    // 初始化/获取 packet
    UdpDataPacket packet;
    memset(&packet, 0, sizeof(UdpDataPacket));
    strncpy(packet.id, rawId.c_str(), 15);
    packet.destroyed = false;

    // 如果已存在数据，先继承上一帧的位置和姿态
    if (m_latestDataMap.find(rawId) != m_latestDataMap.end())
    {
        packet = m_latestDataMap[rawId];
    }

    // 手动拆分属性，因为可能包含多个逗号
    std::stringstream ss(rest);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        // 去除首尾空白
        while (!token.empty() && std::isspace(token.back())) token.pop_back();
        while (!token.empty() && std::isspace(token.front())) token.erase(0, 1);

        if (token.find("T=") == 0)
        {
            std::string tValues = token.substr(2);
            std::vector<std::string> parts;
            std::stringstream tss(tValues);
            std::string tpart;
            while (std::getline(tss, tpart, '|'))
            {
                parts.push_back(tpart);
            }

            try
            {
                if (parts.size() > 0 && !parts[0].empty()) packet.lon = std::stod(parts[0]);
                if (parts.size() > 1 && !parts[1].empty()) packet.lat = std::stod(parts[1]);
                if (parts.size() > 2 && !parts[2].empty()) packet.alt = std::stod(parts[2]);
                if (parts.size() > 3 && !parts[3].empty()) packet.roll = std::stod(parts[3]) * 180.0 / M_PI; // ACMI 是弧度，转度
                if (parts.size() > 4 && !parts[4].empty()) packet.pitch = std::stod(parts[4]) * 180.0 / M_PI;
                if (parts.size() > 5 && !parts[5].empty()) packet.yaw = std::stod(parts[5]) * 180.0 / M_PI;
            }
            catch (...)
            {
            }
        }
        else if (token.find("Name=") == 0)
        {
            std::string name = token.substr(5);
            m_entityNames[rawId] = name;
        }
        else if (token.find("Color=") == 0)
        {
            std::string color = token.substr(6);
            if (color == "C")
            {
                m_latestDataMap.erase(rawId); // 从累积快照中彻底移除
                return;                       // 不再写入 m_latestDataMap
            }
            m_entityColors[rawId] = color;
        }
    }

    // 填充 Name 字段 (从缓存中恢复)
    std::string finalName = m_entityNames[rawId];
    strncpy(packet.name, finalName.c_str(), 31);

    m_latestDataMap[rawId] = packet;
    m_entityLastUpdateTime[rawId] = m_currentParseTime;
    m_acmiPacketCount.fetch_add(1, std::memory_order_relaxed); // 统计接收包数

    // DEBUG LOG:
    // qDebug() << "[ACMI] Parsed Id:" << rawId.c_str() << "Name:" << packet.name
    //          << "Lat:" << packet.lat << "Lon:" << packet.lon << "Alt:" << packet.alt;
}

void SimDataManager::addEntityWithId(const QString& id, const QString& name, ModelType type, double lat, double lon, double alt)
{
    beginInsertRows(QModelIndex(), m_data.count(), m_data.count());

    SimEntityData entity(id, type, lat, lon, alt);
    entity.name = name.isEmpty() ? SimEntityData::defaultNameForType(type) : name;
    m_data.append(entity);

    endInsertRows();

    qDebug() << "[SimDataManager] Entity automatically registered: ID=" << id << "Name=" << entity.name;
    emit entityAdded(id);
}

bool SimDataManager::_entityExists(const QString& id) const
{
    for (const auto& entity : m_data)
    {
        if (entity.id == id) return true;
    }
    return false;
}

ModelType SimDataManager::_inferModelType(const QString& name) const
{
    QString n = name.toLower();
    if (n.contains("f-16") || n.contains("f16") || n.contains("su-27") || n.contains("su27") ||
        n.contains("mig") || n.contains("j-") || n.contains("f-15") || n.contains("f-18") ||
        n.contains("aircraft") || n.contains("plane"))
    {
        return ModelType::Aircraft;
    }
    if (n.contains("aim") || n.contains("r-27") || n.contains("r-73") || n.contains("r-77") ||
        n.contains("r27") || n.contains("r73") || n.contains("missile"))
    {
        return ModelType::Missile;
    }
    if (n.contains("ship") || n.contains("carrier"))
    {
        return ModelType::Ship;
    }
    if (n.contains("vehicle") || n.contains("tank") || n.contains("truck"))
    {
        return ModelType::Vehicle;
    }
    if (n.contains("building") || n.contains("base") || n.contains("tower"))
    {
        return ModelType::Building;
    }
    return ModelType::Aircraft;
}

// ===== ACMI 缓冲回放：消费者接口 =====

std::unordered_map<std::string, UdpDataPacket> SimDataManager::getDataAtTime(double timeSec)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_acmiFrames.empty()) return {};

    // 二分查找：找到第一个 time > timeSec 的帧，然后退一步
    auto it = std::upper_bound(
        m_acmiFrames.begin(), m_acmiFrames.end(), timeSec,
        [](double val, const AcmiFrame& frame) { return val < frame.time; });

    if (it == m_acmiFrames.begin())
    {
        auto entities = m_acmiFrames.front().entities;
        for (auto eit = entities.begin(); eit != entities.end();)
        {
            if (m_excludedIds.count(eit->first))
                eit = entities.erase(eit);
            else
                ++eit;
        }
        return entities;
    }
    --it;
    auto entities = it->entities;
    for (auto eit = entities.begin(); eit != entities.end();)
    {
        if (m_excludedIds.count(eit->first))
            eit = entities.erase(eit);
        else
            ++eit;
    }
    return entities;
}

// 【仿真核心接口：插值点计算】
// 作用：根据当前播放时间，在两个 ACMI 数据帧之间进行平滑插值，彻底消除跳过感。
std::vector<InterpolatedPacket> SimDataManager::getInterpolatedData(double timeSec)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::vector<InterpolatedPacket> result;
    if (m_acmiFrames.empty()) return result;

    // 1. 使用二分查找 (upper_bound) 找到时间轴上刚好在当前时间之后的"下一帧" (it1)
    auto it1 = std::upper_bound(
        m_acmiFrames.begin(), m_acmiFrames.end(), timeSec,
        [](double val, const AcmiFrame& frame) { return val < frame.time; });

    // 2. 特殊情况处理：时间太早
    if (it1 == m_acmiFrames.begin())
    {
        // 时间还没到，返回第一帧
        for (const auto& [id, p] : m_acmiFrames.front().entities)
            result.push_back({id, p.lat, p.lon, p.alt, p.pitch, p.yaw, p.roll, p.name});
        return result;
    }
    // 3. 时间太晚（已经播完了）
    if (it1 == m_acmiFrames.end())
    {
        // 时间已结束，返回最后一帧
        for (const auto& [id, p] : m_acmiFrames.back().entities)
            result.push_back({id, p.lat, p.lon, p.alt, p.pitch, p.yaw, p.roll, p.name});
        return result;
    }

    // it0 为前一帧，it1 为后一帧
    auto it0 = std::prev(it1);
    const AcmiFrame& f0 = *it0;
    const AcmiFrame& f1 = *it1;

    // === 【新增】：插值帧数探测日志 ===
    static double s_last_f0_time = -1.0;
    static double s_last_f1_time = -1.0;
    static int s_interp_count = 0;

    // 如果 f0 有变化，意味着引擎已经“跨越”了那道数据缝隙，进入了下一个数据的区间。此时结算上一个区间。
    if (std::abs(f0.time - s_last_f0_time) > 1e-5)
    {
        if (s_last_f0_time >= 0.0) 
        {
            qDebug() << "=======================================";
            qDebug() << "[插值统计] 突破数据缝隙结算！";
            qDebug() << "  ACMI真实数据区间 : [ t=" << QString::number(s_last_f0_time, 'f', 2) << "s -> t=" << QString::number(s_last_f1_time, 'f', 2) << "s ]";
            qDebug() << "  该段真实数据时长 : " << QString::number(s_last_f1_time - s_last_f0_time, 'f', 3) << "秒";
            qDebug() << "  ▶▶ 引擎在这里面硬生生为你插了【" << s_interp_count << "】帧平滑补间画面！◀◀";
            qDebug() << "=======================================";
        }
        s_last_f0_time = f0.time;
        s_last_f1_time = f1.time;
        s_interp_count = 0;
    }
    s_interp_count++;
    // ==================================

    // 计算插值系数 alpha (0.0 ~ 1.0)
    double denominator = f1.time - f0.time;
    double alpha = (denominator > 1e-6) ? (timeSec - f0.time) / denominator : 0.0;
    // 【关键保护】钳位到 [0,1]，防止时钟轻微抖动导致 alpha 溢出，进而引起模型"前冲/后退"跳变
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;

    // 遍历前一帧的所有实体，如果后一帧也有，则插值；否则保持前一帧
    for (const auto& [id, p0] : f0.entities)
    {
        if (m_excludedIds.count(id)) continue;

        auto it_f1 = f1.entities.find(id);
        if (it_f1 != f1.entities.end())
        {
            const auto& p1 = it_f1->second;
            InterpolatedPacket ip;
            ip.id = id;
            ip.name = p0.name;

            // 线性插值位置
            ip.lat = p0.lat + (p1.lat - p0.lat) * alpha;
            ip.lon = p0.lon + (p1.lon - p0.lon) * alpha;
            ip.alt = p0.alt + (p1.alt - p0.alt) * alpha;

            // 插值姿态（角度插值简单处理）
            auto lerpAngle = [](double a, double b, double t) {
                double diff = b - a;
                while (diff > 180.0) diff -= 360.0;
                while (diff < -180.0) diff += 360.0;
                return a + diff * t;
            };

            ip.pitch = lerpAngle(p0.pitch, p1.pitch, alpha);
            ip.yaw = lerpAngle(p0.yaw, p1.yaw, alpha);
            ip.roll = lerpAngle(p0.roll, p1.roll, alpha);

            ip.color = m_entityColors.count(id) ? m_entityColors[id] : "";

            result.push_back(ip);
        }
    }

    return result;
}

double SimDataManager::acmiTimeMin() const
{
    // 不需要锁：只在主线程调用，且 double 读取是原子的
    return m_acmiTimeMinVal;
}

double SimDataManager::acmiTimeMax() const
{
    return m_acmiTimeMaxVal;
}
