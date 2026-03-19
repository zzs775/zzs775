#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QMap>

struct ModelInfo {
    QString fileName;       // 文件名: F-16A.glb
    QString displayName;    // 显示名: F-16A
    QString category;       // 分类: 飞机 / 导弹
    QString fullPath;       // 完整路径
};

class ModelListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum ModelRoles {
        FileNameRole = Qt::UserRole + 1,
        DisplayNameRole,
        CategoryRole,
        FullPathRole
    };

    explicit ModelListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void loadFromDirectory(const QString& directoryPath);
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QStringList getCategories() const;
    Q_INVOKABLE QVariantList getModelsByCategory(const QString& category) const;
    Q_INVOKABLE void setCategoryFilter(const QString& category);

private:
    QList<ModelInfo> m_allModels;       // 完整模型列表
    QList<ModelInfo> m_filteredModels;  // 当前过滤后的列表
    QString m_basePath;
    QString m_currentFilter;

    QString detectCategory(const QString& fileName);
    void applyFilter();
};
