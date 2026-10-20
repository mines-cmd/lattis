#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QtQmlIntegration>

// See docs/code/metadata.md.
class IdListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, ItemRole };

    using QAbstractListModel::QAbstractListModel;

    int count() const { return int(m_ids.size()); }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : count(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVariantList &items);

signals:
    void countChanged();

private:
    QList<QString> m_ids;
    QList<QVariantMap> m_items;
};
