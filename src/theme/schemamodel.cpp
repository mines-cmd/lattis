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
    rebuildVisibleRows();
    updateChangedCount();
    connect(m_theme, &Theme::reloaded, this, &ThemeSettingsModel::refresh);
    connect(m_theme, &Theme::valueChanged, this, &ThemeSettingsModel::refresh);
}

void ThemeSettingsModel::refresh()
{
    updateChangedCount();
    // With "changed only" on, a reset or theme switch can add or remove rows. Only reset the
    // model when the rows really change: a reset rebuilds every delegate, which would close
    // an open color picker in the middle of a drag.
    if (m_changedOnly) {
        const QList<int> rows = visibleRows();
        if (rows != m_visible) {
            beginResetModel();
            m_visible = rows;
            endResetModel();
            emit sectionsChanged();
            return;
        }
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
    case HalfRole:
        return ThemeSchema::isColorSetting(s) ? QStringLiteral("colors") : QStringLiteral("style");
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
        {HalfRole, "half"},
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
    beginResetModel();
    rebuildVisibleRows();
    endResetModel();
    emit sectionsChanged();
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
    beginResetModel();
    rebuildVisibleRows();
    endResetModel();
    emit sectionsChanged();
}

void ThemeSettingsModel::setChangedOnly(bool changedOnly)
{
    if (changedOnly == m_changedOnly)
        return;
    m_changedOnly = changedOnly;
    emit changedOnlyChanged();
    beginResetModel();
    rebuildVisibleRows();
    endResetModel();
    emit sectionsChanged();
}

QVariantList ThemeSettingsModel::sections() const
{
    QVariantList out;
    QString last;
    for (const int i : m_visible) {
        const QString section = toQString(ThemeSchema::settings()[i].section);
        if (section == last)
            continue;
        last = section;
        out.append(QVariantMap{{QStringLiteral("name"), section}, {QStringLiteral("title"), title(section)}});
    }
    return out;
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

QList<int> ThemeSettingsModel::visibleRows() const
{
    QList<int> rows;
    const auto settings = ThemeSchema::settings();
    for (int i = 0; i < int(settings.size()); ++i) {
        const Setting &s = settings[i];
        if (!listsHalf(ThemeSchema::isColorSetting(s)))
            continue;
        if (s.section == "tags" && s.key == "palette")
            continue;
        if (m_changedOnly && !m_theme->isCustomized(toQString(s.section), toQString(s.key)))
            continue;
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
                continue;
        }
        rows.append(i);
    }
    if (m_mode == Mode::Both) {
        // The schema may interleave a section's colors and style settings.
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
            return ThemeSchema::isColorSetting(x) && !ThemeSchema::isColorSetting(y);
        });
    }
    return rows;
}

void ThemeSettingsModel::rebuildVisibleRows()
{
    m_visible = visibleRows();
}
