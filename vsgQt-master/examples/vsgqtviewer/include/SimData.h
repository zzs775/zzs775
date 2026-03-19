#pragma once

#include <QString>
#include <QHash>
#include <QVariant>

enum class ModelType {
    Aircraft,
    Missile,
    Ship,
    Vehicle,
    Building,
    Unknown
};

struct SimEntityData {
    QString id;
    ModelType type;
    QString name;
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    double heading = 0.0;
    double speed = 0.0;
    double yaw = 0.0;      // 方位角
    double pitch = 0.0;    // 俯仰角
    double roll = 0.0;     // 滚转角

    SimEntityData() = default;

    SimEntityData(const QString& entityId, ModelType entityType, double latitude, double longitude, double altitude)
        : id(entityId), type(entityType), lat(latitude), lon(longitude), alt(altitude) {
        name = defaultNameForType(entityType);
    }

    static QString defaultNameForType(ModelType type) {
        switch (type) {
            case ModelType::Aircraft: return QStringLiteral("Aircraft");
            case ModelType::Missile: return QStringLiteral("Missile");
            case ModelType::Ship: return QStringLiteral("Ship");
            case ModelType::Vehicle: return QStringLiteral("Vehicle");
            case ModelType::Building: return QStringLiteral("Building");
            default: return QStringLiteral("Unknown");
        }
    }

    static QString typeToString(ModelType type) {
        switch (type) {
            case ModelType::Aircraft: return QStringLiteral("Aircraft");
            case ModelType::Missile: return QStringLiteral("Missile");
            case ModelType::Ship: return QStringLiteral("Ship");
            case ModelType::Vehicle: return QStringLiteral("Vehicle");
            case ModelType::Building: return QStringLiteral("Building");
            default: return QStringLiteral("Unknown");
        }
    }
};
