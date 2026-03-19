#include "ModelListModel.h"
#include <QDir>
#include <QDebug>

ModelListModel::ModelListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ModelListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_filteredModels.size();
}

QVariant ModelListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_filteredModels.size())
        return QVariant();

    const ModelInfo& model = m_filteredModels[index.row()];

    switch (role) {
    case FileNameRole:
        return model.fileName;
    case DisplayNameRole:
        return model.displayName;
    case CategoryRole:
        return model.category;
    case FullPathRole:
        return model.fullPath;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ModelListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[FileNameRole] = "fileName";
    roles[DisplayNameRole] = "displayName";
    roles[CategoryRole] = "category";
    roles[FullPathRole] = "fullPath";
    return roles;
}

QString ModelListModel::detectCategory(const QString& fileName) {
    QString lower = fileName.toLower();

    // 飞机关键词
    if (lower.contains("f-16") || lower.contains("f16") ||
        lower.contains("su-27") || lower.contains("su27") ||
        lower.contains("mig") || lower.contains("fighter") ||
        lower.contains("aircraft") || lower.contains("plane")) {
        return QStringLiteral("飞机");
    }

    // 导弹关键词
    if (lower.contains("aim") || lower.contains("missile") ||
        lower.contains("r27") || lower.contains("r73") ||
        lower.contains("r-27") || lower.contains("r-73")) {
        return QStringLiteral("导弹");
    }

    // 船舰关键词
    if (lower.contains("ship") || lower.contains("boat") ||
        lower.contains("carrier") || lower.contains("destroyer")) {
        return QStringLiteral("船舰");
    }

    // 车辆关键词
    if (lower.contains("tank") || lower.contains("vehicle") ||
        lower.contains("car") || lower.contains("truck")) {
        return QStringLiteral("车辆");
    }

    // 建筑关键词
    if (lower.contains("building") || lower.contains("house") ||
        lower.contains("tower") || lower.contains("bunker")) {
        return QStringLiteral("建筑");
    }

    return QStringLiteral("其他");
}

void ModelListModel::loadFromDirectory(const QString& directoryPath) {
    beginResetModel();
    m_allModels.clear();
    m_filteredModels.clear();
    m_basePath = directoryPath;

    QDir dir(directoryPath);
    if (!dir.exists()) {
        qWarning() << "ModelListModel: 目录不存在 ->" << directoryPath;
        endResetModel();
        return;
    }

    QStringList filters;
    filters << "*.glb" << "*.gltf" << "*.obj" << "*.fbx";
    dir.setNameFilters(filters);

    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Readable);
    for (const QFileInfo& fileInfo : files) {
        ModelInfo info;
        info.fileName = fileInfo.fileName();
        info.displayName = fileInfo.baseName();
        info.fullPath = fileInfo.absoluteFilePath();
        info.category = detectCategory(info.fileName);
        m_allModels.append(info);
        qDebug() << "ModelListModel: 加载模型 ->" << info.displayName << "(" << info.category << ")";
    }

    // 初始无过滤，显示全部
    m_filteredModels = m_allModels;
    m_currentFilter.clear();

    endResetModel();
    qDebug() << "ModelListModel: 共加载" << m_allModels.size() << "个模型";
}

void ModelListModel::setCategoryFilter(const QString& category) {
    if (m_currentFilter == category) return;
    m_currentFilter = category;
    beginResetModel();
    applyFilter();
    endResetModel();
    qDebug() << "[ModelListModel] 过滤分类:" << category << "-> 显示" << m_filteredModels.size() << "个模型";
}

void ModelListModel::applyFilter() {
    m_filteredModels.clear();
    if (m_currentFilter.isEmpty() || m_currentFilter == "全部") {
        m_filteredModels = m_allModels;
    } else if (m_currentFilter == "其他") {
        // "其他" = 排除飞机和导弹
        for (const auto& m : m_allModels) {
            if (m.category != "飞机" && m.category != "导弹") {
                m_filteredModels.append(m);
            }
        }
    } else {
        for (const auto& m : m_allModels) {
            if (m.category == m_currentFilter) {
                m_filteredModels.append(m);
            }
        }
    }
}

QVariantMap ModelListModel::get(int index) const {
    if (index < 0 || index >= m_filteredModels.size())
        return QVariantMap();

    const ModelInfo& model = m_filteredModels[index];
    QVariantMap result;
    result["fileName"] = model.fileName;
    result["displayName"] = model.displayName;
    result["category"] = model.category;
    result["fullPath"] = model.fullPath;
    return result;
}

QStringList ModelListModel::getCategories() const {
    QStringList categories;
    for (const ModelInfo& model : m_allModels) {
        if (!categories.contains(model.category)) {
            categories.append(model.category);
        }
    }
    return categories;
}

QVariantList ModelListModel::getModelsByCategory(const QString& category) const {
    QVariantList result;
    for (int i = 0; i < m_allModels.size(); ++i) {
        if (m_allModels[i].category == category) {
            QVariantMap map;
            map["fileName"] = m_allModels[i].fileName;
            map["displayName"] = m_allModels[i].displayName;
            map["category"] = m_allModels[i].category;
            map["fullPath"] = m_allModels[i].fullPath;
            result.append(map);
        }
    }
    return result;
}
