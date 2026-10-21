#include "schemamodel.h"
#include "tomlsection.h"
#include "theme.h"
#include "themeschema.h"

#include <QColor>
#include <QHash>

#include <algorithm>

using ThemeSchema::Setting;
using ThemeSchema::Type;

namespace {
QString camel(const QString &snake)
{
    QString out;
    bool upper = false;
    for (const QChar c : snake) {
        if (c == u'_') {
            upper = true;
            continue;
        }
        out += upper ? c.toUpper() : c;
        upper = false;
    }
    return out;
}

QString title(const QString &snake)
{
    QString out = snake;
    out.replace(u'_', u' ');
    if (!out.isEmpty())
        out[0] = out[0].toUpper();
    return out;
}

QStringList choicesOf(const Setting &s)
{
    return toQString(s.choices).split(u' ', Qt::SkipEmptyParts);
}

QString displayTypeName(Type type)
{
    switch (type) {
    case Type::Color: return QStringLiteral("Color");
    case Type::Int: return QStringLiteral("Int");
    case Type::Real: return QStringLiteral("Real");
    case Type::Bool: return QStringLiteral("Bool");
    case Type::String: return QStringLiteral("String");
    case Type::Enum: return QStringLiteral("Enum");
    case Type::StringList: return QStringLiteral("StringList");
    case Type::ColorList: return QStringLiteral("ColorList");
    }
    return {};
}

QString defaultText(const Setting &s)
{
    const QString def = toQString(s.def);
    switch (s.type) {
    case Type::StringList:
    case Type::ColorList: {
        QStringList items = def.split(u' ', Qt::SkipEmptyParts);
        for (QString &item : items)
            item = u'"' + item + u'"';
        return u'[' + items.join(QLatin1String(", ")) + u']';
    }
    case Type::Int:
    case Type::Real:
    case Type::Bool:
        return def;
    default:
        return def.startsWith(u'@') ? def : u'"' + def + u'"';
    }
}

bool sectionShared(const QString &section)
{
    for (const ThemeSchema::Section &s : ThemeSchema::sections()) {
        if (toQString(s.name) == section)
            return s.shared;
    }
    return false;
}

QString sectionDoc(const QString &section)
{
    for (const ThemeSchema::Section &s : ThemeSchema::sections()) {
        if (toQString(s.name) == section)
            return toQString(s.doc);
    }
    return {};
}
}

ThemeSettingsModel::ThemeSettingsModel(Theme *theme, Mode mode, QObject *parent)
    : QAbstractListModel(parent)
    , m_theme(theme)
    , m_mode(mode)
{
    const auto settings = ThemeSchema::settings();
    const auto headings = ThemeSchema::headings();
    m_headingOfSetting.resize(settings.size(), -1);
    int current = -1;
    int next = 0;
    for (int i = 0; i < int(settings.size()); ++i) {
        while (next < int(headings.size()) && headings[next].first <= i)
            current = next++;
        if (current >= 0 && headings[current].section == settings[i].section)
            m_headingOfSetting[i] = current;
    }
    rebuildVisibleRows();
    updateChangedCount();
    connect(m_theme, &Theme::reloaded, this, &ThemeSettingsModel::refresh);
    connect(m_theme, &Theme::valueChanged, this, &ThemeSettingsModel::refresh);
}

void ThemeSettingsModel::ensureLinks() const
{
    const auto settings = ThemeSchema::settings();
    if (m_groupMask.isEmpty()) {
        m_groupMask.resize(settings.size());
        const auto groups = ThemeSchema::groups();
        for (int i = 0; i < int(settings.size()); ++i) {
            quint32 mask = 0;
            for (int g = 0; g < int(groups.size()); ++g) {
                if (ThemeSchema::inGroup(groups[g], settings[i]))
                    mask |= quint32(1) << g;
            }
            m_groupMask[i] = mask;
        }
    }
    if (!m_linksDirty)
        return;
    m_linksDirty = false;
    m_linkTarget.assign(settings.size(), QString());
    m_followers.assign(settings.size(), 0);
    m_ownValue.assign(settings.size(), false);
    QHash<QString, int> counts;
    for (int i = 0; i < int(settings.size()); ++i) {
        const Setting &s = settings[i];
        const QVariant raw = m_theme->rawValue(toQString(s.section), toQString(s.key));
        const bool set = raw.isValid();
        const QString effective = set ? (raw.typeId() == QMetaType::QString ? raw.toString() : QString())
                                      : toQString(s.def);
        if (effective.startsWith(u'@')) {
            QString target = effective.mid(1);
            if (!target.contains(u'.'))
                target.prepend(QLatin1String("palette."));
            m_linkTarget[i] = target;
            ++counts[target];
        } else if (set && s.def.starts_with('@')) {
            m_ownValue[i] = true;
        }
    }
    for (int i = 0; i < int(settings.size()); ++i)
        m_followers[i] = counts.value(toQString(settings[i].section) + u'.' + toQString(settings[i].key));
}

void ThemeSettingsModel::refresh()
{
    m_linksDirty = true;
    updateChangedCount();
    // With "changed only" or "following" on, a reset or theme switch can add or remove rows. Only
    // reset the model when the rows really change: a reset rebuilds every delegate, which would
    // close an open color picker in the middle of a drag.
    if (m_changedOnly || !m_following.isEmpty()) {
        const QList<int> rows = visibleRows();
        if (rows != m_visible) {
            beginResetModel();
            setVisible(rows);
            endResetModel();
            emit sectionsChanged();
            emit groupsChanged();
            return;
        }
        if (m_changedOnly)
            emit groupsChanged();
    }
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1));
}

void ThemeSettingsModel::updateChangedCount()
{
    int count = 0;
    for (const Setting &s : ThemeSchema::settings()) {
        const bool color = ThemeSchema::isColorSetting(s);
        const bool inMode = m_mode == Mode::Both || color == (m_mode == Mode::Colors);
        if (inMode && m_theme->isCustomized(toQString(s.section), toQString(s.key)))
            ++count;
    }
    if (count == m_changedCount)
        return;
    m_changedCount = count;
    emit changedCountChanged();
}

int ThemeSettingsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visible.size();
}

QVariant ThemeSettingsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
        return {};

    const Setting &s = ThemeSchema::settings()[m_visible[index.row()]];
    const QString section = toQString(s.section);
    const QString key = toQString(s.key);

    switch (role) {
    case SectionRole: return section;
    case SectionDocRole: return sectionDoc(section);
    case SectionTitleRole: return title(section);
    case KeyRole: return key;
    case TitleRole: return title(key);
    case CamelKeyRole: return camel(key);
    case TypeRole: return displayTypeName(s.type);
    case DocRole: return toQString(s.doc);
    case MinRole: return s.min;
    case MaxRole: return s.max;
    case ChoicesRole: return choicesOf(s);
    case OnReleaseRole: return s.onRelease;
    case DefaultRole: return defaultText(s);
    case ValueRole: return m_theme->resolvedValue(section, key);
    case RawRole: return m_theme->rawValue(section, key);
    case SourceLabelRole: return m_theme->sourceLabel(section, key);
    case IsCustomRole: return m_theme->isCustomized(section, key);
    case RevertTextRole: return m_theme->revertText(section, key);
    case FirstInSectionRole:
        return index.row() == 0 || toQString(ThemeSchema::settings()[m_visible[index.row() - 1]].section) != section;
    case LastInSectionRole:
        return index.row() == m_visible.size() - 1
            || toQString(ThemeSchema::settings()[m_visible[index.row() + 1]].section) != section;
    case SubsectionRole: {
        const int heading = m_headingOfRow[index.row()];
        return heading < 0 ? QString() : toQString(ThemeSchema::headings()[heading].title);
    }
    case SubsectionCountRole: return m_cardSize[index.row()];
    case FirstInCardRole: return index.row() == 0 || !sameCard(index.row() - 1, index.row());
    case LastInCardRole: return index.row() == m_visible.size() - 1 || !sameCard(index.row(), index.row() + 1);
    case HalfRole:
        return ThemeSchema::isColorSetting(s) ? QStringLiteral("colors") : QStringLiteral("style");
    case LinkTargetRole:
        ensureLinks();
        return m_linkTarget[m_visible[index.row()]];
    case FollowersRole:
        ensureLinks();
        return m_followers[m_visible[index.row()]];
    case OwnValueRole:
        ensureLinks();
        return bool(m_ownValue[m_visible[index.row()]]);
    }
    return {};
}

QHash<int, QByteArray> ThemeSettingsModel::roleNames() const
{
    return {
        {SectionRole, "section"},
        {SectionDocRole, "sectionDoc"},
        {SectionTitleRole, "sectionTitle"},
        {KeyRole, "key"},
        {TitleRole, "title"},
        {CamelKeyRole, "camelKey"},
        {TypeRole, "type"},
        {DocRole, "doc"},
        {MinRole, "min"},
        {MaxRole, "max"},
        {ChoicesRole, "choices"},
        {OnReleaseRole, "onRelease"},
        {DefaultRole, "defaultText"},
        {ValueRole, "value"},
        {RawRole, "raw"},
        {SourceLabelRole, "sourceLabel"},
        {IsCustomRole, "isCustom"},
        {RevertTextRole, "revertText"},
        {FirstInSectionRole, "firstInSection"},
        {LastInSectionRole, "lastInSection"},
        {SubsectionRole, "subsection"},
        {SubsectionCountRole, "subsectionCount"},
        {FirstInCardRole, "firstInCard"},
        {LastInCardRole, "lastInCard"},
        {HalfRole, "half"},
        {LinkTargetRole, "linkTarget"},
        {FollowersRole, "followers"},
        {OwnValueRole, "ownValue"},
    };
}

QString ThemeSettingsModel::show() const
{
    switch (m_show) {
    case Show::Colors: return QStringLiteral("colors");
    case Show::Style: return QStringLiteral("style");
    case Show::All: break;
    }
    return QStringLiteral("all");
}

void ThemeSettingsModel::setShow(const QString &show)
{
    const Show value = show == QLatin1String("colors") ? Show::Colors
        : show == QLatin1String("style") ? Show::Style : Show::All;
    if (value == m_show)
        return;
    m_show = value;
    emit showChanged();
    applyFilters();
}

void ThemeSettingsModel::applyFilters()
{
    beginResetModel();
    rebuildVisibleRows();
    endResetModel();
    emit sectionsChanged();
    emit groupsChanged();
}

void ThemeSettingsModel::setGroup(const QString &group)
{
    int bit = -1;
    const auto groups = ThemeSchema::groups();
    for (int g = 0; g < int(groups.size()); ++g) {
        if (toQString(groups[g].name) == group)
            bit = g;
    }
    const QString value = bit >= 0 ? group : QStringLiteral("everything");
    const bool clearsFollowing = bit >= 0 && !m_following.isEmpty();
    if (value == m_group && !clearsFollowing)
        return;
    const bool groupChange = value != m_group;
    m_group = value;
    m_groupBit = bit;
    if (clearsFollowing) {
        m_following.clear();
        emit followingChanged();
    }
    if (groupChange)
        emit groupChanged();
    applyFilters();
}

void ThemeSettingsModel::setFollowing(const QString &target)
{
    const bool clearsGroup = !target.isEmpty() && m_group != QLatin1String("everything");
    if (target == m_following && !clearsGroup)
        return;
    const bool followingChange = target != m_following;
    m_following = target;
    if (clearsGroup) {
        m_group = QStringLiteral("everything");
        m_groupBit = -1;
        emit groupChanged();
    }
    if (followingChange)
        emit followingChanged();
    applyFilters();
}

QVariantList ThemeSettingsModel::groups() const
{
    ensureLinks();
    const auto groups = ThemeSchema::groups();
    QList<int> counts(groups.size(), 0);
    int all = 0;
    const auto settings = ThemeSchema::settings();
    for (int i = 0; i < int(settings.size()); ++i) {
        if (!passes(i, false))
            continue;
        ++all;
        for (int g = 0; g < int(groups.size()); ++g) {
            if (m_groupMask[i] & (quint32(1) << g))
                ++counts[g];
        }
    }
    QVariantList out;
    out.append(QVariantMap{{QStringLiteral("name"), QStringLiteral("everything")},
                           {QStringLiteral("title"), QStringLiteral("Everything")},
                           {QStringLiteral("count"), all}});
    for (int g = 0; g < int(groups.size()); ++g) {
        out.append(QVariantMap{{QStringLiteral("name"), toQString(groups[g].name)},
                               {QStringLiteral("title"), toQString(groups[g].title)},
                               {QStringLiteral("count"), counts[g]}});
    }
    return out;
}

bool ThemeSettingsModel::listsHalf(bool color) const
{
    switch (m_mode) {
    case Mode::Colors: return color;
    case Mode::Style: return !color;
    case Mode::Both: break;
    }
    return m_show == Show::All || color == (m_show == Show::Colors);
}

void ThemeSettingsModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    emit filterTextChanged();
    applyFilters();
}

void ThemeSettingsModel::setChangedOnly(bool changedOnly)
{
    if (changedOnly == m_changedOnly)
        return;
    m_changedOnly = changedOnly;
    emit changedOnlyChanged();
    applyFilters();
}

QVariantList ThemeSettingsModel::sections() const
{
    QVariantList out;
    QVariantList subsections;
    QString last;
    const auto flush = [&] {
        if (out.isEmpty())
            return;
        QVariantMap section = out.takeLast().toMap();
        section.insert(QStringLiteral("subsections"), subsections);
        out.append(section);
        subsections.clear();
    };
    for (int row = 0; row < m_visible.size(); ++row) {
        const QString section = toQString(ThemeSchema::settings()[m_visible[row]].section);
        if (section != last) {
            flush();
            last = section;
            out.append(QVariantMap{{QStringLiteral("name"), section},
                                   {QStringLiteral("title"), title(section)},
                                   {QStringLiteral("shared"), sectionShared(section)}});
        }
        const int heading = m_headingOfRow[row];
        if (heading >= 0 && (row == 0 || !sameCard(row - 1, row))) {
            subsections.append(QVariantMap{
                {QStringLiteral("title"), toQString(ThemeSchema::headings()[heading].title)},
                {QStringLiteral("count"), m_cardSize[row]},
            });
        }
    }
    flush();
    return out;
}

bool ThemeSettingsModel::sameCard(int row, int other) const
{
    return ThemeSchema::settings()[m_visible[row]].section == ThemeSchema::settings()[m_visible[other]].section
        && m_headingOfRow[row] == m_headingOfRow[other];
}

int ThemeSettingsModel::firstRow(const QString &section, const QString &subsection) const
{
    for (int row = 0; row < m_visible.size(); ++row) {
        const int heading = m_headingOfRow[row];
        if (heading >= 0 && toQString(ThemeSchema::settings()[m_visible[row]].section) == section
            && toQString(ThemeSchema::headings()[heading].title) == subsection) {
            return row;
        }
    }
    return -1;
}

int ThemeSettingsModel::firstRow(const QString &section) const
{
    for (int row = 0; row < m_visible.size(); ++row) {
        if (toQString(ThemeSchema::settings()[m_visible[row]].section) == section)
            return row;
    }
    return -1;
}

int ThemeSettingsModel::rowOf(const QString &section, const QString &key) const
{
    for (int row = 0; row < m_visible.size(); ++row) {
        const Setting &s = ThemeSchema::settings()[m_visible[row]];
        if (toQString(s.section) == section && toQString(s.key) == key)
            return row;
    }
    return -1;
}

// withSlot: also apply the group / following filter. The group counts leave it out.
bool ThemeSettingsModel::passes(int setting, bool withSlot) const
{
    const Setting &s = ThemeSchema::settings()[setting];
    if (!listsHalf(ThemeSchema::isColorSetting(s)))
        return false;
    if (s.section == "tags" && s.key == "palette")
        return false;
    if (m_changedOnly && !m_theme->isCustomized(toQString(s.section), toQString(s.key)))
        return false;
    if (withSlot) {
        ensureLinks();
        if (!m_following.isEmpty() && m_linkTarget[setting] != m_following)
            return false;
        if (m_groupBit >= 0 && !(m_groupMask[setting] & (quint32(1) << m_groupBit)))
            return false;
    }
    if (!m_filterText.isEmpty()) {
        const QString section = toQString(s.section);
        const QString key = toQString(s.key);
        QString haystack = section + u' ' + key + u' ' + toQString(s.doc);
        if (s.type == Type::Color) {
            const QVariant raw = m_theme->rawValue(section, key);
            if (raw.typeId() == QMetaType::QString)
                haystack += u' ' + raw.toString();
            const QVariant resolved = m_theme->resolvedValue(section, key);
            if (resolved.typeId() == QMetaType::QColor) {
                const QColor c = resolved.value<QColor>();
                haystack += u' ' + c.name(QColor::HexRgb);
                if (c.alpha() < 255) {
                    // App convention: #rrggbbaa, not Qt's #aarrggbb.
                    haystack += u' ' + QString::asprintf("#%02x%02x%02x%02x",
                                                         c.red(), c.green(), c.blue(), c.alpha());
                }
            }
        }
        haystack.replace(u'_', u' ');
        if (!haystack.contains(QString(m_filterText).replace(u'_', u' '), Qt::CaseInsensitive))
            return false;
    }
    return true;
}

QList<int> ThemeSettingsModel::visibleRows() const
{
    QList<int> rows;
    const auto settings = ThemeSchema::settings();
    for (int i = 0; i < int(settings.size()); ++i) {
        if (passes(i, true))
            rows.append(i);
    }
    if (m_mode == Mode::Both) {
        // The schema may interleave a section's colors and style settings; within a sub-heading,
        // colors come first.
        QHash<QByteArray, int> order;
        for (const int i : rows)
            order.insert(QByteArray(settings[i].section.data(), settings[i].section.size()), order.size());
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
            const Setting &x = settings[a];
            const Setting &y = settings[b];
            const int ox = order.value(QByteArray(x.section.data(), x.section.size()));
            const int oy = order.value(QByteArray(y.section.data(), y.section.size()));
            if (ox != oy)
                return ox < oy;
            if (m_headingOfSetting[a] != m_headingOfSetting[b])
                return m_headingOfSetting[a] < m_headingOfSetting[b];
            return ThemeSchema::isColorSetting(x) && !ThemeSchema::isColorSetting(y);
        });
    }
    return rows;
}

void ThemeSettingsModel::rebuildVisibleRows()
{
    setVisible(visibleRows());
}

void ThemeSettingsModel::setVisible(const QList<int> &rows)
{
    m_visible = rows;
    m_headingOfRow.resize(rows.size());
    m_cardSize.resize(rows.size());
    for (int row = 0; row < rows.size(); ++row)
        m_headingOfRow[row] = m_headingOfSetting[rows[row]];
    for (int row = 0; row < rows.size();) {
        int end = row + 1;
        while (end < rows.size() && sameCard(row, end))
            ++end;
        for (int i = row; i < end; ++i)
            m_cardSize[i] = end - row;
        row = end;
    }
}
