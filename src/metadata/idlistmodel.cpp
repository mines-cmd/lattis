#include "idlistmodel.h"

#include <QSet>

QVariant IdListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= count())
        return {};
    if (role == IdRole)
        return m_ids.at(index.row());
    if (role == ItemRole)
        return m_items.at(index.row());
    return {};
}

QHash<int, QByteArray> IdListModel::roleNames() const
{
    return {{IdRole, "rid"}, {ItemRole, "item"}};
}

void IdListModel::setItems(const QVariantList &items)
{
    const int before = count();
    QList<QString> ids;
    QList<QVariantMap> maps;
    ids.reserve(items.size());
    maps.reserve(items.size());
    for (const QVariant &v : items) {
        maps.append(v.toMap());
        ids.append(maps.last().value(QStringLiteral("id")).toString());
    }

    const QSet<QString> wanted(ids.cbegin(), ids.cend());
    for (int last = count() - 1; last >= 0; --last) {
        if (wanted.contains(m_ids.at(last)))
            continue;
        int first = last;
        while (first > 0 && !wanted.contains(m_ids.at(first - 1)))
            --first;
        beginRemoveRows({}, first, last);
        m_ids.remove(first, last - first + 1);
        m_items.remove(first, last - first + 1);
        endRemoveRows();
        last = first;
    }

    const QSet<QString> had(m_ids.cbegin(), m_ids.cend());
    for (int i = 0; i < ids.size(); ++i) {
        if (i >= count() || m_ids.at(i) != ids.at(i)) {
            if (had.contains(ids.at(i))) {
                const int from = int(m_ids.indexOf(ids.at(i), i + 1));
                beginMoveRows({}, from, from, {}, i);
                m_ids.move(from, i);
                m_items.move(from, i);
                endMoveRows();
            } else {
                int end = i + 1;
                while (end < ids.size() && !had.contains(ids.at(end)))
                    ++end;
                beginInsertRows({}, i, end - 1);
                for (int k = i; k < end; ++k) {
                    m_ids.insert(k, ids.at(k));
                    m_items.insert(k, maps.at(k));
                }
                endInsertRows();
                i = end - 1;
                continue;
            }
        }
        if (m_items.at(i) != maps.at(i)) {
            m_items[i] = maps.at(i);
            emit dataChanged(index(i), index(i), {ItemRole});
        }
    }

    if (count() != before)
        emit countChanged();
}
