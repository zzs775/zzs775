$target = 'C:\Users\cfh12\Desktop\rocky_qt\vsgQt-(rubulid)\vsgQt-master\examples\vsgqtviewer\src\SimDataManager.cpp'

$code = @'
#include "SimDataManager.h"
#include <QDebug>
#include <QModelIndex>
#include <QVariant>
#include <QUdpSocket>
#include <sstream>
#include <algorithm>
#include <cmath>

SimDataManager::SimDataManager(QObject* parent) : QAbstractListModel(parent) {}
SimDataManager::~SimDataManager() { stopUdpReceiver(); }

int SimDataManager::rowCount(const QModelIndex& parent) const { return m_data.count(); }

QVariant SimDataManager::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_data.count()) return QVariant();
    const auto& e = m_data.at(index.row());
    switch (role) {
        case IdRole:    return e.id;
        case NameRole:  return e.name;
        case TypeRole:  return static_cast<int>(e.type);
        case LatRole:   return e.lat;
        case LonRole:   return e.lon;
        case AltRole:   return e.alt;
        case YawRole:   return e.yaw;
        case PitchRole: return e.pitch;
        case RollRole:  return e.roll;
        default: return QVariant();
    }
}

QHash<int, QByteArray> SimDataManager::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole]    = "id";
    roles[NameRole]  = "name";
    roles[TypeRole]  = "type";
    roles[LatRole]   = "lat";
    roles[LonRole]   = "lon";
    roles[AltRole]   = "alt";
    roles[YawRole]   = "yaw";
    roles[PitchRole] = "pitch";
    roles[RollRole]  = "roll";
    return roles;
}

bool SimDataManager::_entityExists(const QString& id) const {
    for (const auto& e : m_data) if (e.id == id) return true;
    return false;
}

ModelType SimDataManager::_inferModelType(const QString& name) const {
    QString n = name.toLower();
    if (n.contains("aim") || n.contains("missile") || n.contains("r-77") || n.contains("r-27")) return ModelType::Missile;
    if (n.contains("ship") || n.contains("carrier")) return ModelType::Ship;
    if (n.contains("tank") || n.contains("ground")) return ModelType::Tank;
    if (n.contains("building")) return ModelType::Building;
    return ModelType::Aircraft;
}

void SimDataManager::addEntity(ModelType type, double lat, double lon, double alt) {
    addEntityWithName("", type, lat, lon, alt);
}

void SimDataManager::addEntityWithName(const QString& name, ModelType type, double lat, double lon, double alt) {
    beginInsertRows(QModelIndex(), m_data.count(), m_data.count());
    SimEntityData entity(QString::number(m_data.count()), type, lat, lon, alt);
    entity.name = name.isEmpty() ? SimEntityData::defaultNameForType(type) : name;
    m_data.append(entity);
    endInsertRows();
    emit entityAdded(entity.id);
}

void SimDataManager::addEntityWithId(const QString& id, const QString& name, ModelType type, double lat, double lon, double alt) {
    if (_entityExists(id)) return;
    beginInsertRows(QModelIndex(), m_data.count(), m_data.count());
    SimEntityData entity(id, type, lat, lon, alt);
    entity.name = name.isEmpty() ? SimEntityData::defaultNameForType(type) : name;
    m_data.append(entity);
    endInsertRows();
    qDebug() << "[SimDataManager] Entity added: ID=" << id << "Name=" << entity.name;
    emit entityAdded(id);
}

bool SimDataManager::removeEntity(const QString& id, bool blacklist) {
    for (int i = 0; i < m_data.count(); ++i) {
        if (m_data[i].id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_data.removeAt(i);
            endRemoveRows();
            if (blacklist) m_excludedIds.insert(id.toStdString());
            emit entityRemoved(id);
            return true;
        }
    }
    return false;
}

bool SimDataManager::updateEntityPosition(const QString& id, double lat, double lon, double alt, double yaw, double pitch, double roll) {
    for (int i = 0; i < m_data.count(); ++i) {
        if (m_data[i].id == id) {
            m_data[i].lat   = lat;   m_data[i].lon   = lon;   m_data[i].alt   = alt;
            m_data[i].yaw   = yaw;   m_data[i].pitch = pitch; m_data[i].roll  = roll;
            emit dataChanged(index(i), index(i));
            emit entityUpdated(id);
            return true;
        }
    }
    return false;
}

QVariantMap SimDataManager::getEntity(const QString& id) const {
    for (const auto& e : m_data) {
        if (e.id == id) {
            QVariantMap m;
            m["id"] = e.id; m["name"] = e.name; m["lat"] = e.lat; m["lon"] = e.lon; m["alt"] = e.alt;
            m["yaw"] = e.yaw; m["pitch"] = e.pitch; m["roll"] = e.roll;
            return m;
        }
    }
    return {};
}

int SimDataManager::count() const { return m_data.count(); }

void SimDataManager::clearAll() {
    beginResetModel();
    m_data.clear();
    endResetModel();
}

QVariantList SimDataManager::getEntitiesByType(const QString& typeFilter) const {
    QVariantList list;
    for (const auto& e : m_data) {
        if (typeFilter.isEmpty() || QString::number(static_cast<int>(e.type)) == typeFilter) {
            QVariantMap m;
            m["id"] = e.id; m["name"] = e.name;
            list.append(m);
        }
    }
    return list;
}

SimEntityData* SimDataManager::findEntityById(const QString& id) {
    for (auto& e : m_data) if (e.id == id) return &e;
    return nullptr;
}

double SimDataManager::acmiTimeMin() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_acmiTimeMinVal;
}

double SimDataManager::acmiTimeMax() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_acmiTimeMaxVal;
}

// ===== ACMI Buffer Playback =====

void SimDataManager::parseAcmiLine(const std::string& lineStr) {
    if (lineStr.empty()) return;

    if (lineStr.find("SimState=Start") != std::string::npos) {
        qDebug() << "[SimDataManager] SimState=Start: clearing all data";
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

    // Time stamp line: #seconds
    if (lineStr[0] == '#') {
        std::string timeStr = lineStr.substr(1);
        timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\r'), timeStr.end());
        timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
        double newTime = 0.0;
        try { newTime = std::stod(timeStr); } catch (...) { return; }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        if (m_currentParseTime >= 0.0 && !m_latestDataMap.empty()) {
            double staleThreshold = 10.0;
            for (auto it = m_latestDataMap.begin(); it != m_latestDataMap.end();) {
                auto uit = m_entityLastUpdateTime.find(it->first);
                if (uit == m_entityLastUpdateTime.end() || (m_currentParseTime - uit->second) > staleThreshold)
                    it = m_latestDataMap.erase(it);
                else
                    ++it;
            }
            AcmiFrame frame;
            frame.time = m_currentParseTime;
            frame.entities = m_latestDataMap;
            m_acmiFrames.push_back(std::move(frame));
        }

        m_currentParseTime = newTime;

        if (newTime > m_maxReceivedTime) {
            m_maxReceivedTime = newTime;
            emit maxReceivedTimeChanged(m_maxReceivedTime);
        }

        if (m_acmiFrames.empty()) {
            m_acmiTimeMinVal = newTime;
            m_acmiTimeMaxVal = newTime;
        } else {
            if (newTime < m_acmiTimeMinVal) m_acmiTimeMinVal = newTime;
            if (newTime > m_acmiTimeMaxVal) m_acmiTimeMaxVal = newTime;
        }
        return;
    }

    // Skip global object (ID=0)
    if (lineStr.find("0,") == 0) return;

    std::lock_guard<std::mutex> lock(m_dataMutex);

    // Entity removal: -ID
    if (lineStr[0] == '-') {
        std::string rawId = lineStr.substr(1);
        rawId.erase(std::remove(rawId.begin(), rawId.end(), '\r'), rawId.end());
        rawId.erase(std::remove(rawId.begin(), rawId.end(), '\n'), rawId.end());
        m_latestDataMap.erase(rawId);
        return;
    }

    // Data line: ID,T=...,Name=...,Color=...
    size_t commaPos = lineStr.find(',');
    if (commaPos == std::string::npos) return;
    std::string rawId = lineStr.substr(0, commaPos);
    rawId.erase(std::remove(rawId.begin(), rawId.end(), '\r'), rawId.end());
    rawId.erase(std::remove(rawId.begin(), rawId.end(), '\n'), rawId.end());
    if (m_excludedIds.count(rawId)) return;

    std::string rest = lineStr.substr(commaPos + 1);

    UdpDataPacket packet;
    memset(&packet, 0, sizeof(UdpDataPacket));
    strncpy(packet.id, rawId.c_str(), 15);
    if (m_latestDataMap.find(rawId) != m_latestDataMap.end())
        packet = m_latestDataMap[rawId];

    std::stringstream ss(rest);
    std::string token;
    while (std::getline(ss, token, ',')) {
        while (!token.empty() && std::isspace((unsigned char)token.back()))  token.pop_back();
        while (!token.empty() && std::isspace((unsigned char)token.front())) token.erase(0, 1);

        if (token.find("T=") == 0) {
            std::stringstream tss(token.substr(2));
            std::string tp;
            std::vector<std::string> parts;
            while (std::getline(tss, tp, '|')) parts.push_back(tp);
            try {
                if (parts.size() > 0 && !parts[0].empty()) packet.lon   = std::stod(parts[0]);
                if (parts.size() > 1 && !parts[1].empty()) packet.lat   = std::stod(parts[1]);
                if (parts.size() > 2 && !parts[2].empty()) packet.alt   = std::stod(parts[2]);
                if (parts.size() > 3 && !parts[3].empty()) packet.roll  = std::stod(parts[3]) * 180.0 / M_PI;
                if (parts.size() > 4 && !parts[4].empty()) packet.pitch = std::stod(parts[4]) * 180.0 / M_PI;
                if (parts.size() > 5 && !parts[5].empty()) packet.yaw   = std::stod(parts[5]) * 180.0 / M_PI;
            } catch (...) {}
        } else if (token.find("Name=") == 0) {
            m_entityNames[rawId] = token.substr(5);
        } else if (token.find("Color=") == 0) {
            std::string color = token.substr(6);
            if (color == "C") {
                m_latestDataMap.erase(rawId);
                return;
            }
            m_entityColors[rawId] = color;
        }
    }

    std::string finalName = m_entityNames[rawId];
    strncpy(packet.name, finalName.c_str(), 31);
    m_latestDataMap[rawId] = packet;
    m_entityLastUpdateTime[rawId] = m_currentParseTime;
    m_acmiPacketCount.fetch_add(1, std::memory_order_relaxed);
}

std::unordered_map<std::string, UdpDataPacket> SimDataManager::getDataAtTime(double timeSec) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_acmiFrames.empty()) return {};
    auto it = std::upper_bound(m_acmiFrames.begin(), m_acmiFrames.end(), timeSec,
        [](double val, const AcmiFrame& f) { return val < f.time; });
    if (it == m_acmiFrames.begin()) return m_acmiFrames.front().entities;
    --it;
    return it->entities;
}

std::vector<InterpolatedPacket> SimDataManager::getInterpolatedData(double timeSec) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::vector<InterpolatedPacket> result;
    if (m_acmiFrames.empty()) return result;

    auto it1 = std::upper_bound(m_acmiFrames.begin(), m_acmiFrames.end(), timeSec,
        [](double val, const AcmiFrame& f) { return val < f.time; });

    if (it1 == m_acmiFrames.begin()) {
        for (const auto& kv : m_acmiFrames.front().entities)
            result.push_back({kv.first, kv.second.lat, kv.second.lon, kv.second.alt,
                              kv.second.pitch, kv.second.yaw, kv.second.roll, kv.second.name,
                              m_entityColors[kv.first]});
        return result;
    }
    if (it1 == m_acmiFrames.end()) {
        for (const auto& kv : m_acmiFrames.back().entities)
            result.push_back({kv.first, kv.second.lat, kv.second.lon, kv.second.alt,
                              kv.second.pitch, kv.second.yaw, kv.second.roll, kv.second.name,
                              m_entityColors[kv.first]});
        return result;
    }

    auto it0 = std::prev(it1);
    double t0 = it0->time, t1 = it1->time;
    double alpha = (t1 - t0 > 1e-9) ? (timeSec - t0) / (t1 - t0) : 0.0;
    alpha = std::max(0.0, std::min(1.0, alpha));

    auto lerpAngle = [](double a, double b, double al) -> double {
        double d = b - a;
        while (d >  180.0) d -= 360.0;
        while (d < -180.0) d += 360.0;
        return a + d * al;
    };

    for (const auto& kv : it0->entities) {
        const std::string& id = kv.first;
        const UdpDataPacket& p0 = kv.second;
        if (m_excludedIds.count(id)) continue;

        auto f1it = it1->entities.find(id);
        if (f1it != it1->entities.end()) {
            const UdpDataPacket& p1 = f1it->second;
            result.push_back({
                id,
                p0.lat + (p1.lat - p0.lat) * alpha,
                p0.lon + (p1.lon - p0.lon) * alpha,
                p0.alt + (p1.alt - p0.alt) * alpha,
                lerpAngle(p0.pitch, p1.pitch, alpha),
                lerpAngle(p0.yaw,   p1.yaw,   alpha),
                lerpAngle(p0.roll,  p1.roll,  alpha),
                std::string(p0.name),
                m_entityColors[id]
            });
        } else {
            result.push_back({id, p0.lat, p0.lon, p0.alt, p0.pitch, p0.yaw, p0.roll, p0.name, m_entityColors[id]});
        }
    }
    return result;
}

// ===== UDP Live Mode =====
void SimDataManager::startUdpReceiver(quint16 port) {
    m_udpThreadRunning = true;
    m_udpThread = std::thread(&SimDataManager::udpThreadFunc, this, port);
}

void SimDataManager::stopUdpReceiver() {
    m_udpThreadRunning = false;
    if (m_udpThread.joinable()) m_udpThread.join();
}

void SimDataManager::udpThreadFunc(quint16 port) {
    QUdpSocket socket;
    socket.bind(port);
    while (m_udpThreadRunning) {
        if (socket.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(socket.pendingDatagramSize());
            socket.readDatagram(datagram.data(), datagram.size());
            if (static_cast<size_t>(datagram.size()) == sizeof(UdpDataPacket)) {
                UdpDataPacket* p = reinterpret_cast<UdpDataPacket*>(datagram.data());
                std::lock_guard<std::mutex> lock(m_dataMutex);
                m_latestDataMap[p->id] = *p;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

std::unordered_map<std::string, UdpDataPacket> SimDataManager::getLatestUdpData() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_latestDataMap;
}
'@

[System.IO.File]::WriteAllText($target, $code, [System.Text.Encoding]::UTF8)
Write-Host "SimDataManager.cpp rewritten successfully."
