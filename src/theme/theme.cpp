#include "theme.h"
#include "gamepagemigration.h"
#include "tomlsection.h"
#include "schemamodel.h"
#include "suggestion.h"
#include "themeschema.h"
#include "tomlpatch.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QSaveFile>

#include <toml++/toml.hpp>

#include <algorithm>

Q_LOGGING_CATEGORY(lcTheme, "launcher.theme")

using ThemeSchema::Setting;
using ThemeSchema::Type;

namespace {
constexpr Setting kColorProto{"palette", "", Type::Color, "", ""};

QString fullKey(const Setting &s)
{
    return toQString(s.section) + u'.' + toQString(s.key);
}

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

const QHash<QString, const Setting *> &settingIndex()
{
    static const QHash<QString, const Setting *> index = [] {
        QHash<QString, const Setting *> h;
        for (const Setting &s : ThemeSchema::settings())
            h.insert(fullKey(s), &s);
        return h;
    }();
    return index;
}

const Setting *findSetting(const QString &key)
{
    return settingIndex().value(key);
}

bool isSection(const QString &name)
{
    const auto sections = ThemeSchema::sections();
    return std::any_of(sections.begin(), sections.end(),
                       [&](const ThemeSchema::Section &s) { return name == toQString(s.name); });
}

QStringList choicesOf(const Setting &s)
{
    return toQString(s.choices).split(u' ', Qt::SkipEmptyParts);
}

// #rgb, #rrggbb or CSS-style #rrggbbaa (QColor would read 8 digits as #aarrggbb).
std::optional<QColor> parseHex(const QString &value)
{
    const QString s = value.trimmed();
    if (!s.startsWith(u'#'))
        return std::nullopt;
    if (s.size() == 4 || s.size() == 7) {
        const QColor c = QColor::fromString(s);
        return c.isValid() ? std::optional(c) : std::nullopt;
    }
    if (s.size() == 9) {
        bool ok = false;
        const uint v = s.mid(1).toUInt(&ok, 16);
        if (ok)
            return QColor((v >> 24) & 0xff, (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
    }
    return std::nullopt;
}

QVariant nodeToVariant(const toml::node &node)
{
    if (auto v = node.value_exact<std::string>())
        return QString::fromStdString(*v);
    if (auto v = node.value_exact<int64_t>())
        return qlonglong(*v);
    if (auto v = node.value_exact<double>())
        return *v;
    if (auto v = node.value_exact<bool>())
        return *v;
    if (const toml::array *arr = node.as_array()) {
        QVariantList list;
        for (const toml::node &item : *arr)
            list.append(nodeToVariant(item));
        return list;
    }
    return {};
}

QVariant defaultRaw(const Setting &s)
{
    const QString def = toQString(s.def);
    if (def.startsWith(u'@'))
        return def;
    switch (s.type) {
    case Type::Int:
        return def.toLongLong();
    case Type::Real:
        return def.toDouble();
    case Type::Bool:
        return def == QLatin1String("true");
    case Type::StringList:
    case Type::ColorList: {
        QVariantList list;
        for (const QString &item : def.split(u' ', Qt::SkipEmptyParts))
            list.append(item);
        return list;
    }
    default:
        return def;
    }
}

QString quoted(const QString &s)
{
    QString escaped = s;
    escaped.replace(u'\\', QLatin1String("\\\\")).replace(u'"', QLatin1String("\\\""));
    return u'"' + escaped + u'"';
}

QString formatDefault(const Setting &s)
{
    const QString def = toQString(s.def);
    switch (s.type) {
    case Type::StringList:
    case Type::ColorList: {
        QStringList items = def.split(u' ', Qt::SkipEmptyParts);
        for (QString &item : items)
            item = quoted(item);
        return u'[' + items.join(QLatin1String(", ")) + u']';
    }
    case Type::Int:
    case Type::Real:
    case Type::Bool:
        return def.startsWith(u'@') ? quoted(def) : def;
    default:
        return quoted(def);
    }
}

QString formatScalar(const QVariant &v, Type type)
{
    if (v.typeId() == QMetaType::QString) {
        const QString str = v.toString();
        if (str.startsWith(u'@'))
            return quoted(str);
    }
    switch (type) {
    case Type::Color: {
        // Typed text arrives as a string; parse it as #rrggbbaa, not QColor's #aarrggbb.
        const QColor c = v.typeId() == QMetaType::QString ? parseHex(v.toString()).value_or(QColor())
                                                          : v.value<QColor>();
        QString hex = QStringLiteral("#%1%2%3")
                          .arg(c.red(), 2, 16, QChar(u'0'))
                          .arg(c.green(), 2, 16, QChar(u'0'))
                          .arg(c.blue(), 2, 16, QChar(u'0'));
        if (c.alpha() != 255)
            hex += QStringLiteral("%1").arg(c.alpha(), 2, 16, QChar(u'0'));
        return quoted(hex.toLower());
    }
    case Type::Int:
        return QString::number(v.toLongLong());
    case Type::Real:
        return QString::number(v.toDouble(), 'g', 12);
    case Type::Bool:
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case Type::String:
    case Type::Enum:
        return quoted(v.toString());
    case Type::StringList: {
        QStringList items;
        for (const QVariant &item : v.toList())
            items.append(quoted(item.toString()));
        return u'[' + items.join(QLatin1String(", ")) + u']';
    }
    case Type::ColorList: {
        QStringList items;
        for (const QVariant &item : v.toList())
            items.append(formatScalar(item, Type::Color));
        return u'[' + items.join(QLatin1String(", ")) + u']';
    }
    }
    return {};
}

QVariant toRawShape(const QVariant &v, Type type)
{
    if (v.typeId() == QMetaType::QString && v.toString().startsWith(u'@'))
        return v;
    switch (type) {
    case Type::Int:
        return qlonglong(v.toLongLong());
    case Type::Real:
        return v.toDouble();
    case Type::Bool:
        return v.toBool();
    case Type::String:
    case Type::Enum:
        return v.toString();
    case Type::ColorList: {
        QVariantList out;
        for (const QVariant &item : v.toList())
            out.append(toRawShape(item, Type::Color));
        return out;
    }
    case Type::Color:
    case Type::StringList:
    default:
        return v;
    }
}

QString describeType(const Setting &s)
{
    const QString name = toQString(ThemeSchema::typeName(s.type));
    switch (s.type) {
    case Type::Int:
    case Type::Real:
        return QStringLiteral("%1, %2 to %3").arg(name).arg(s.min).arg(s.max);
    case Type::Enum:
    case Type::StringList:
        return name + u' ' + choicesOf(s).join(QLatin1String(", "));
    case Type::Color:
        return s.fallback.empty() ? name : QStringLiteral("%1, fallback %2").arg(name, toQString(s.fallback));
    default:
        return name;
    }
}

const char kReferenceHeader[] = R"(# Every %1 setting with its default value.
# This file is regenerated each time the app starts: don't edit it, copy what you
# need into your own %2 file and change it there.
#
# Values:
#   colors   "#rrggbb" or "#rrggbbaa" (with alpha)
#   lists    ["a", "b"]; [] for none
# Any value can instead reference another setting of the same type:
#   "@accent"            a [palette] color
#   "@radius.medium"     any setting, as section.key
# If a value can't be used, "<key>_fallback" is tried, then the default below.
)";

const char kSettingsFile[] = R"(# Lattis settings

[appearance]
# Colors from themes/colors/ and a style from themes/styles/ (file names without .toml).
colors = "Ember"
style = "Soft"

# Keyboard shortcuts: action = "keys", or a list of keys; [] turns one off. Settings >
# Keyboard lists every action with its default keys.
[shortcuts]
# toggle_favorite = "Ctrl+D"
)";

const QStringList kBuiltInColors = {QStringLiteral("Ember"), QStringLiteral("Moss"), QStringLiteral("Mythos"),
                                    QStringLiteral("Steam"), QStringLiteral("Catppuccin"), QStringLiteral("Gruvbox"),
                                    QStringLiteral("Rose Pine"), QStringLiteral("Nord")};
const QStringList kBuiltInStyles = {QStringLiteral("Sharp"), QStringLiteral("Soft"),
                                    QStringLiteral("Round"), QStringLiteral("Steam")};
const QStringList kBuiltInThemes = kBuiltInColors;

const QString kFallbackColors = QStringLiteral("Ember");
const QString kFallbackStyle = QStringLiteral("Soft");

const QString kBuiltInPrefix = QStringLiteral(":/qt/qml/Launcher/resources/themes/");
}

QString Theme::folder(Half half)
{
    return half == Half::Colors ? QStringLiteral("colors") : QStringLiteral("styles");
}

QString Theme::word(Half half)
{
    return half == Half::Colors ? QStringLiteral("colors") : QStringLiteral("style");
}

Theme::Half Theme::halfOf(const QString &fullKey)
{
    const Setting *s = findSetting(fullKey);
    return !s || ThemeSchema::isColorSetting(*s) ? Half::Colors : Half::Style;
}

Theme::Theme(QString dataDir, Uninstall::Trash trash, QObject *parent)
    : QQmlPropertyMap(this, parent)
    , m_dataDir(std::move(dataDir))
    , m_trash(std::move(trash))
{
    s_instance = this;

    for (const ThemeSchema::Section &section : ThemeSchema::sections()) {
        const QString name = toQString(section.name);
        auto *map = new ThemeSection(this);
        m_sections.insert(name, map);
        insert(camel(name), QVariant::fromValue<QObject *>(map));
    }

    // Editors often replace files atomically, which fires several events and
    // drops file watches; debounce and re-add the paths after each reload.
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadTimer,
            qOverload<>(&QTimer::start));
    connect(&m_reloadTimer, &QTimer::timeout, this, [this] {
        reload();
        watch();
    });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &Theme::flushPendingSaves);

    ensureFiles();
    reload();
    watch();

    m_colorsSchema = new ThemeSettingsModel(this, ThemeSettingsModel::Mode::Colors, this);
    m_styleSchema = new ThemeSettingsModel(this, ThemeSettingsModel::Mode::Style, this);
    m_customizeSchema = new ThemeSettingsModel(this, ThemeSettingsModel::Mode::Both, this);
}

QObject *Theme::customizeSchema() const
{
    return m_customizeSchema;
}

QObject *Theme::colorsSchema() const
{
    return m_colorsSchema;
}

QObject *Theme::styleSchema() const
{
    return m_styleSchema;
}

Theme *Theme::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Theme::warn(const QString &message)
{
    if (m_quiet)
        return;
    qCWarning(lcTheme).noquote() << message;
    m_problems.append(message);
}

void Theme::ensureFiles()
{
    QDir().mkpath(m_dataDir + QStringLiteral("/themes/colors"));
    QDir().mkpath(m_dataDir + QStringLiteral("/themes/styles"));

    const QString settings = m_dataDir + QStringLiteral("/settings.toml");
    if (!QFile::exists(settings)) {
        QFile file(settings);
        if (file.open(QIODevice::WriteOnly))
            file.write(kSettingsFile);
        else
            warn(QStringLiteral("could not create %1").arg(settings));
    }
    dropMaterialTheme();
    migrateRadiusTiers();

    writeReference(Half::Colors);
    writeReference(Half::Style);
}

void Theme::dropMaterialTheme()
{
    for (const QString &name : legacyThemeNames()) {
        const QString path = userThemePath(name);
        try {
            const toml::table tbl = toml::parse_file(path.toStdString());
            if (tbl["extends"].value<std::string>() == "material")
                TomlPatch::setValueInFile(path, QString(), QStringLiteral("extends"), quoted(QStringLiteral("default")));
        } catch (const toml::parse_error &) {
        }
    }
    if (readActiveThemeName() == QLatin1String("material")) {
        QString error;
        const QString settings = m_dataDir + QStringLiteral("/settings.toml");
        if (!TomlPatch::setValueInFile(settings, QStringLiteral("appearance"), QStringLiteral("theme"),
                                       quoted(QStringLiteral("Mythos")), &error))
            warn(QStringLiteral("could not update %1: %2").arg(settings, error));
    }
}

void Theme::migrateRadiusTiers()
{
    for (const QString &name : legacyThemeNames()) {
        const QString path = userThemePath(name);

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString original = QString::fromUtf8(file.readAll());
        file.close();

        toml::table tbl;
        try {
            tbl = toml::parse(original.toStdString());
        } catch (const toml::parse_error &) {
            continue;
        }

        const bool hasOldRadius = tbl["layout"]["radius"].value<int64_t>().has_value();
        const bool hasOldSmall = tbl["layout"]["radius_small"].value<int64_t>().has_value();
        const bool hasAnyRef = original.contains(QLatin1String("\"@layout.radius_small\""))
            || original.contains(QLatin1String("\"@layout.radius\""));

        if (!hasOldRadius && !hasOldSmall && !hasAnyRef)
            continue;

        QString text = original;

        // Replace longer key first to avoid matching "@layout.radius" inside "@layout.radius_small".
        text.replace(QLatin1String("\"@layout.radius_small\""), QLatin1String("\"@radius.small\""));
        text.replace(QLatin1String("\"@layout.radius\""), QLatin1String("\"@radius.medium\""));

        if (hasOldRadius) {
            const int64_t v = *tbl["layout"]["radius"].value<int64_t>();
            if (!tbl["radius"]["medium"].value<int64_t>().has_value()) {
                const auto r = TomlPatch::setValue(text, QStringLiteral("radius"),
                                                   QStringLiteral("medium"), QString::number(v));
                if (r.ok)
                    text = r.text;
            }
            const auto r = TomlPatch::removeValue(text, QStringLiteral("layout"), QStringLiteral("radius"));
            if (r.ok)
                text = r.text;
        }

        if (hasOldSmall) {
            const int64_t v = *tbl["layout"]["radius_small"].value<int64_t>();
            if (!tbl["radius"]["small"].value<int64_t>().has_value()) {
                const auto r = TomlPatch::setValue(text, QStringLiteral("radius"),
                                                   QStringLiteral("small"), QString::number(v));
                if (r.ok)
                    text = r.text;
            }
            const auto r = TomlPatch::removeValue(text, QStringLiteral("layout"),
                                                  QStringLiteral("radius_small"));
            if (r.ok)
                text = r.text;
        }

        if (text == original)
            continue;

        QSaveFile out(path);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(text.toUtf8());
            out.commit();
        }
    }
}

void Theme::writeReference(Half half)
{
    QString out = QString::fromUtf8(kReferenceHeader)
                      .arg(half == Half::Colors ? QStringLiteral("color") : QStringLiteral("style"),
                           half == Half::Colors ? QStringLiteral("colors (e.g. themes/colors/Ember.toml)")
                                                : QStringLiteral("style (e.g. themes/styles/Soft.toml)"));
    for (const ThemeSchema::Section &section : ThemeSchema::sections()) {
        QString body;
        for (const Setting &s : ThemeSchema::settings()) {
            if (s.section != section.name || (half == Half::Colors) != ThemeSchema::isColorSetting(s))
                continue;
            body += QStringLiteral("%1 = %2   # %3 (%4)\n")
                        .arg(toQString(s.key), formatDefault(s), toQString(s.doc), describeType(s));
        }
        if (body.isEmpty())
            continue;
        out += u'\n';
        for (const QString &line : toQString(section.doc).split(u'\n'))
            out += QStringLiteral("# %1\n").arg(line);
        out += QStringLiteral("[%1]\n").arg(toQString(section.name)) + body;
    }

    const QString path = userHalfPath(half, QStringLiteral("reference"));
    const QByteArray content = out.toUtf8();
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == content)
        return;
    existing.close();

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(content);
        file.commit();
    }
}

QString Theme::readAppearance(Half half) const
{
    const QString path = m_dataDir + QStringLiteral("/settings.toml");
    try {
        const toml::table tbl = toml::parse_file(path.toStdString());
        if (auto name = tbl["appearance"][word(half).toStdString()].value<std::string>(); name && !name->empty())
            return QString::fromStdString(*name);
    } catch (const toml::parse_error &e) {
        qCWarning(lcTheme).noquote() << QStringLiteral("%1:%2: %3")
                                            .arg(path)
                                            .arg(e.source().begin.line)
                                            .arg(toQString(e.description()));
    }
    return half == Half::Colors ? kFallbackColors : kFallbackStyle;
}

QString Theme::readActiveThemeName() const
{
    const QString path = m_dataDir + QStringLiteral("/settings.toml");
    try {
        const toml::table tbl = toml::parse_file(path.toStdString());
        if (auto name = tbl["appearance"]["theme"].value<std::string>())
            return QString::fromStdString(*name);
    } catch (const toml::parse_error &) {
    }
    return QStringLiteral("Ember");
}

QString Theme::userThemePath(const QString &name) const
{
    return m_dataDir + QStringLiteral("/themes/") + name + QStringLiteral(".toml");
}

QString Theme::userHalfPath(Half half, const QString &name) const
{
    return m_dataDir + QStringLiteral("/themes/") + folder(half) + u'/' + name + QStringLiteral(".toml");
}

QString Theme::halfPath(Half half, const QString &name, bool builtInOnly) const
{
    if (name.isEmpty() || name.contains(u'/') || name.contains(u'\\'))
        return {};
    if (!builtInOnly) {
        const QString user = userHalfPath(half, name);
        if (QFile::exists(user))
            return user;
    }
    const QString builtIn = kBuiltInPrefix + folder(half) + u'/' + name + QStringLiteral(".toml");
    return QFile::exists(builtIn) ? builtIn : QString();
}

std::optional<Theme::Pair> Theme::readPair(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray data = file.readAll();
    try {
        const toml::table tbl = toml::parse(std::string_view(data.constData(), size_t(data.size())));
        const auto colors = tbl["colors"].value<std::string>();
        const auto style = tbl["style"].value<std::string>();
        if (!colors && !style)
            return std::nullopt;
        return Pair{QString::fromStdString(colors.value_or(std::string())),
                    QString::fromStdString(style.value_or(std::string()))};
    } catch (const toml::parse_error &) {
        return std::nullopt;
    }
}

std::optional<Theme::Pair> Theme::pairOf(const QString &theme) const
{
    if (theme.isEmpty() || theme.contains(u'/') || theme.contains(u'\\'))
        return std::nullopt;
    if (auto pair = readPair(userThemePath(theme)))
        return pair;
    return readPair(kBuiltInPrefix + theme + QStringLiteral(".toml"));
}

QStringList Theme::namesIn(Half half) const
{
    QStringList list = half == Half::Colors ? kBuiltInColors : kBuiltInStyles;
    QStringList user;
    const QDir dir(m_dataDir + QStringLiteral("/themes/") + folder(half));
    for (const QString &f : dir.entryList({QStringLiteral("*.toml")}, QDir::Files)) {
        const QString name = QFileInfo(f).completeBaseName();
        if (name != QLatin1String("reference") && !list.contains(name))
            user.append(name);
    }
    user.sort(Qt::CaseInsensitive);
    return list + user;
}

QStringList Theme::legacyThemeNames() const
{
    QStringList list;
    const QDir themes(m_dataDir + QStringLiteral("/themes"));
    for (const QString &f : themes.entryList({QStringLiteral("*.toml")}, QDir::Files)) {
        if (f != QLatin1String("reference.toml"))
            list.append(QFileInfo(f).completeBaseName());
    }
    return list;
}

QString Theme::deriveActiveTheme() const
{
    for (const QString &name : availableThemes()) {
        const auto pair = pairOf(name);
        if (pair && pair->colors == m_activeColors && pair->style == m_activeStyle)
            return name;
    }
    return {};
}

bool Theme::applyHalfFile(Half half, const QString &name, bool builtInOnly, QHash<QString, Entry> &merged,
                          QStringList &chain, bool parentsOnly)
{
    const QString chainKey = name + (builtInOnly ? QStringLiteral(" (built in)") : QString());
    if (chain.contains(chainKey)) {
        warn(QStringLiteral("%1 extend each other in a loop: %2")
                 .arg(folder(half),
                      (chain << chainKey).join(QStringLiteral(" → "))));
        return true;
    }
    const QString path = halfPath(half, name, builtInOnly);
    if (path.isEmpty()) {
        warn(QStringLiteral("%1 \"%2\" not found in %3/themes/%4").arg(word(half), name, m_dataDir, folder(half)));
        return true;
    }
    chain.append(chainKey);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return true;
    const QByteArray data = file.readAll();
    const bool user = !path.startsWith(u':');
    const QString display = user ? QStringLiteral("themes/%1/%2.toml").arg(folder(half), name)
                                 : QStringLiteral("built-in %1/%2.toml").arg(folder(half), name);

    toml::table tbl;
    try {
        tbl = toml::parse(std::string_view(data.constData(), size_t(data.size())), path.toStdString());
    } catch (const toml::parse_error &e) {
        warn(QStringLiteral("%1:%2: %3")
                 .arg(display)
                 .arg(e.source().begin.line)
                 .arg(toQString(e.description())));
        return false;
    }

    // A user file named after a built-in, with no `extends` or one naming itself, layers on that built-in.
    // A built-in extends the original built-in, so editing Soft doesn't change Sharp.
    QString parent;
    bool parentBuiltIn = !user;
    if (const toml::node *node = tbl.get("extends")) {
        if (auto parentName = node->value<std::string>())
            parent = QString::fromStdString(*parentName);
        else
            warn(QStringLiteral("%1: extends must be a name in quotes").arg(display));
    }
    if (user && (parent.isEmpty() || parent == name) && !halfPath(half, name, true).isEmpty()) {
        parent = name;
        parentBuiltIn = true;
    }
    if (!parent.isEmpty() && !applyHalfFile(half, parent, parentBuiltIn, merged, chain))
        return false;
    if (parentsOnly)
        return true;

    QStringList sectionNames;
    for (const ThemeSchema::Section &s : ThemeSchema::sections())
        sectionNames.append(toQString(s.name));

    for (const auto &[sectionKey, sectionNode] : tbl) {
        const QString section = toQString(sectionKey.str());
        const QString where = QStringLiteral("%1:%2").arg(display).arg(sectionNode.source().begin.line);
        if (section == QLatin1String("extends") || (half == Half::Style && section == QLatin1String("description")))
            continue;
        const toml::table *table = sectionNode.as_table();
        if (!table) {
            warn(QStringLiteral("%1: \"%2\" must be inside a [section]").arg(where, section));
            continue;
        }
        if (!isSection(section)) {
            warn(QStringLiteral("%1: unknown section [%2]%3").arg(where, section, suggestion(section, sectionNames)));
            continue;
        }

        QStringList keys;
        for (const Setting &s : ThemeSchema::settings()) {
            if (toQString(s.section) == section)
                keys.append(toQString(s.key));
        }

        for (const auto &[key, node] : *table) {
            const QString keyName = toQString(key.str());
            const QString line = QStringLiteral("%1:%2").arg(display).arg(node.source().begin.line);
            QString base = keyName;
            if (base.endsWith(QLatin1String("_fallback")))
                base.chop(9);
            if (!keys.contains(base)) {
                if (m_collectLegacy && half == Half::Style && section == QLatin1String("details")
                    && GamePageMigration::legacyKeys().contains(keyName))
                    m_legacyGamePage.insert(keyName, nodeToVariant(node));
                if (section != QLatin1String("palette")) {
                    warn(QStringLiteral("%1: unknown setting %2.%3%4").arg(line, section, keyName, suggestion(keyName, keys)));
                    continue;
                }
                const QString hint = suggestion(base, keys);
                if (!hint.isEmpty())
                    warn(QStringLiteral("%1: new custom color palette.%2%3").arg(line, keyName, hint));
            }
            const Half owner = halfOf(section + u'.' + base);
            if (owner != half) {
                warn(QStringLiteral("%1: %2.%3 belongs to a theme's %4, not its %5; put it in themes/%6/")
                         .arg(line, section, keyName, word(owner), word(half), folder(owner)));
                continue;
            }
            merged.insert(section + u'.' + keyName, {nodeToVariant(node), line, name, user, half});
        }
    }
    return true;
}

void Theme::reload()
{
    m_problems.clear();

    const QString colors = readAppearance(Half::Colors);
    const QString style = readAppearance(Half::Style);
    const auto loadable = [this](Half half, const QString &name) {
        if (!halfPath(half, name).isEmpty())
            return name;
        const QString fallback = half == Half::Colors ? kFallbackColors : kFallbackStyle;
        warn(QStringLiteral("%1 \"%2\" not found in %3/themes/%4; using %5")
                 .arg(word(half), name, m_dataDir, folder(half), fallback));
        return fallback;
    };
    const QString loadedColors = loadable(Half::Colors, colors);
    const QString loadedStyle = loadable(Half::Style, style);

    QHash<QString, Entry> merged;
    QStringList colorsChain;
    QStringList styleChain;
    m_legacyGamePage.clear();
    m_collectLegacy = true;
    const bool colorsOk = applyHalfFile(Half::Colors, loadedColors, false, merged, colorsChain);
    const bool styleOk = applyHalfFile(Half::Style, loadedStyle, false, merged, styleChain);
    m_collectLegacy = false;
    if (!(colorsOk && styleOk) && m_loaded) {
        emit problemsChanged();
        return; // keep the current theme while a file is being edited
    }

    m_loadedColors = loadedColors;
    m_loadedStyle = loadedStyle;
    const bool colorsChanged = colors != m_activeColors;
    const bool styleChanged = style != m_activeStyle;
    m_activeColors = colors;
    m_activeStyle = style;
    if (colorsChanged)
        emit activeColorsChanged();
    if (styleChanged)
        emit activeStyleChanged();
    const QString theme = deriveActiveTheme();
    if (theme != m_activeTheme || colorsChanged || styleChanged) {
        m_activeTheme = theme;
        emit activeThemeChanged();
    }

    m_merged = merged;
    applyOverridesOnTop();

    qCInfo(lcTheme).noquote() << "colors:" << colorsChain.join(QStringLiteral(" → extends "))
                              << "style:" << styleChain.join(QStringLiteral(" → extends "));
    m_loaded = true;
    emit availableChanged();
    emit problemsChanged();
    emit reloaded();
    emit sessionChangesChanged();
}

void Theme::applyOverridesOnTop()
{
    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it)
        m_merged.insert(it.key(), Entry{it.value(), QStringLiteral("(unsaved)"), activeName(halfOf(it.key())), true, halfOf(it.key())});

    m_resolved.clear();
    m_resolving.clear();

    for (const Setting &s : ThemeSchema::settings()) {
        const QVariant value = valueOf(s);
        QQmlPropertyMap *map = m_sections.value(toQString(s.section));
        const QString key = camel(toQString(s.key));
        if (map->value(key) != value)
            map->insert(key, value);
    }

    QQmlPropertyMap *palette = m_sections.value(QStringLiteral("palette"));
    for (auto it = m_merged.cbegin(); it != m_merged.cend(); ++it) {
        if (!it.key().startsWith(QLatin1String("palette.")) || findSetting(it.key())
            || it.key().endsWith(QLatin1String("_fallback")))
            continue;
        QString error;
        const QVariant value = convert(it->raw, kColorProto, &error);
        if (!value.isValid())
            warn(QStringLiteral("%1: %2: %3").arg(it->origin, it.key(), error));
        else if (palette->value(camel(it.key().mid(8))) != value)
            palette->insert(camel(it.key().mid(8)), value);
    }
}

QVariant Theme::valueOf(const Setting &s)
{
    const QString key = fullKey(s);
    if (auto it = m_resolved.constFind(key); it != m_resolved.constEnd())
        return *it;
    if (m_resolving.contains(key))
        return {}; // reference loop; the caller falls back
    m_resolving.insert(key);

    QVariant result;
    QString error;

    const auto user = m_merged.constFind(key);
    if (user != m_merged.constEnd()) {
        result = convert(user->raw, s, &error);
        if (!result.isValid())
            warn(QStringLiteral("%1: %2: %3").arg(user->origin, key, error));
    }
    if (!result.isValid()) {
        const auto fallback = m_merged.constFind(key + QLatin1String("_fallback"));
        if (fallback != m_merged.constEnd()) {
            result = convert(fallback->raw, s, &error);
            if (!result.isValid())
                warn(QStringLiteral("%1: %2_fallback: %3").arg(fallback->origin, key, error));
        }
    }
    if (!result.isValid())
        result = convert(defaultRaw(s), s, &error);
    if (!result.isValid() && !s.fallback.empty())
        result = convert(toQString(s.fallback), s, &error);
    if (!result.isValid())
        warn(QStringLiteral("built-in default for %1 is invalid: %2").arg(key, error));

    m_resolving.remove(key);
    m_resolved.insert(key, result);
    return result;
}

QVariant Theme::convert(const QVariant &raw, const Setting &s, QString *error)
{
    const auto fail = [error](const QString &message) {
        *error = message;
        return QVariant();
    };

    if (raw.typeId() == QMetaType::QString && s.type != Type::String) {
        const QString str = raw.toString();
        if (str.startsWith(u'@')) {
            const QString ref = str.mid(1);
            const QString target = ref.contains(u'.') ? ref : QStringLiteral("palette.") + ref;
            QVariant value;
            Type targetType = Type::Color;
            if (const Setting *t = findSetting(target)) {
                value = valueOf(*t);
                targetType = t->type;
            } else if (target.startsWith(QLatin1String("palette.")) && m_merged.contains(target)) {
                if (m_resolving.contains(target))
                    return fail(QStringLiteral("\"%1\" refers back to itself").arg(str));
                m_resolving.insert(target);
                QString ignored;
                value = convert(m_merged.value(target).raw, kColorProto, &ignored);
                m_resolving.remove(target);
            } else {
                return fail(QStringLiteral("unknown setting \"%1\"").arg(str));
            }
            if (!value.isValid())
                return fail(QStringLiteral("\"%1\" has no usable value (or refers back to this setting)").arg(str));

            const bool numeric = (s.type == Type::Int || s.type == Type::Real)
                && (targetType == Type::Int || targetType == Type::Real);
            if (targetType != s.type && !numeric) {
                return fail(QStringLiteral("\"%1\" is a %2, expected a %3")
                                .arg(str, toQString(ThemeSchema::typeName(targetType)),
                                     toQString(ThemeSchema::typeName(s.type))));
            }
            if (!numeric)
                return value;
            return convert(value, s, error);
        }
    }

    switch (s.type) {
    case Type::Color:
        if (raw.typeId() == QMetaType::QColor)
            return raw;
        if (raw.typeId() == QMetaType::QString) {
            if (auto c = parseHex(raw.toString()))
                return *c;
        }
        return fail(QStringLiteral("expected a color like \"#rrggbb\" or \"#rrggbbaa\""));

    case Type::Int:
    case Type::Real: {
        const bool isNumber = raw.typeId() == QMetaType::LongLong || raw.typeId() == QMetaType::Int
            || raw.typeId() == QMetaType::Double;
        if (!isNumber)
            return fail(QStringLiteral("expected a number"));
        const double v = raw.toDouble();
        if (v < s.min || v > s.max)
            return fail(QStringLiteral("must be between %1 and %2").arg(s.min).arg(s.max));
        if (s.type == Type::Int)
            return int(std::lround(v));
        return v;
    }

    case Type::Bool:
        if (raw.typeId() == QMetaType::Bool)
            return raw;
        return fail(QStringLiteral("expected true or false"));

    case Type::String:
        if (raw.typeId() == QMetaType::QString)
            return raw;
        return fail(QStringLiteral("expected text in quotes"));

    case Type::Enum: {
        const QStringList choices = choicesOf(s);
        if (raw.typeId() == QMetaType::QString && choices.contains(raw.toString()))
            return raw;
        return fail(QStringLiteral("expected one of: %1").arg(choices.join(QLatin1String(", "))));
    }

    case Type::StringList: {
        if (raw.typeId() != QMetaType::QVariantList)
            return fail(QStringLiteral("expected a list like [\"a\", \"b\"]"));
        const QStringList choices = choicesOf(s);
        QStringList out;
        for (const QVariant &item : raw.toList()) {
            if (item.typeId() != QMetaType::QString || !choices.contains(item.toString())) {
                return fail(QStringLiteral("\"%1\" is not allowed; use any of: %2")
                                .arg(item.toString(), choices.join(QLatin1String(", "))));
            }
            out.append(item.toString());
        }
        return out;
    }

    case Type::ColorList: {
        if (raw.typeId() != QMetaType::QVariantList)
            return fail(QStringLiteral("expected a list of colors like [\"#c0392b\", \"@accent\"]"));
        QVariantList out;
        for (const QVariant &item : raw.toList()) {
            QString itemError;
            const QVariant c = convert(item, kColorProto, &itemError);
            if (!c.isValid())
                return fail(QStringLiteral("\"%1\": %2").arg(item.toString(), itemError));
            out.append(c);
        }
        return out;
    }
    }
    return fail(QStringLiteral("unsupported type"));
}

void Theme::watch()
{
    QStringList files = {m_dataDir + QStringLiteral("/settings.toml")};
    QStringList dirs = {m_dataDir};
    for (const QString &sub : {QString(), QStringLiteral("colors"), QStringLiteral("styles")}) {
        const QDir dir(m_dataDir + QStringLiteral("/themes") + (sub.isEmpty() ? QString() : u'/' + sub));
        for (const QString &name : dir.entryList({QStringLiteral("*.toml")}, QDir::Files))
            files.append(dir.filePath(name));
        dirs.append(dir.path());
    }

    const QStringList watchedFiles = m_watcher.files();
    for (const QString &file : std::as_const(files)) {
        if (QFile::exists(file) && !watchedFiles.contains(file))
            m_watcher.addPath(file);
    }
    const QStringList watchedDirs = m_watcher.directories();
    for (const QString &dir : std::as_const(dirs)) {
        if (QFileInfo(dir).isDir() && !watchedDirs.contains(dir))
            m_watcher.addPath(dir);
    }
}

QStringList Theme::availableColors() const
{
    return namesIn(Half::Colors);
}

QStringList Theme::availableStyles() const
{
    return namesIn(Half::Style);
}

QStringList Theme::availableThemes() const
{
    QStringList user;
    const QDir themes(m_dataDir + QStringLiteral("/themes"));
    for (const QString &f : themes.entryList({QStringLiteral("*.toml")}, QDir::Files)) {
        const QString name = QFileInfo(f).completeBaseName();
        if (name != QLatin1String("reference") && !kBuiltInThemes.contains(name) && readPair(themes.filePath(f)))
            user.append(name);
    }
    user.sort(Qt::CaseInsensitive);
    return kBuiltInThemes + user;
}

QStringList Theme::builtInColors() const
{
    return kBuiltInColors;
}

QStringList Theme::builtInStyles() const
{
    return kBuiltInStyles;
}

QStringList Theme::builtInThemes() const
{
    return kBuiltInThemes;
}

QVariantMap Theme::themePair(const QString &name) const
{
    const auto pair = pairOf(name);
    if (!pair)
        return {};
    return {{QStringLiteral("colors"), pair->colors}, {QStringLiteral("style"), pair->style}};
}

QVariantMap Theme::preview(const QString &colors, const QString &style)
{
    // The resolver works on the active theme's members; swap them out and back, so nothing of
    // the other pair (its values, its warnings) stays.
    const QHash<QString, Entry> merged = std::exchange(m_merged, {});
    const QHash<QString, QVariant> resolved = std::exchange(m_resolved, {});
    const QStringList problems = m_problems;
    m_resolving.clear();

    QVariantMap out;
    QStringList colorsChain;
    QStringList styleChain;
    const auto loadable = [this](Half half, const QString &name) {
        return halfPath(half, name).isEmpty() ? (half == Half::Colors ? kFallbackColors : kFallbackStyle) : name;
    };
    if (applyHalfFile(Half::Colors, loadable(Half::Colors, colors), false, m_merged, colorsChain)
        && applyHalfFile(Half::Style, loadable(Half::Style, style), false, m_merged, styleChain)) {
        for (const Setting &s : ThemeSchema::settings()) {
            if (s.section == "palette")
                out.insert(camel(toQString(s.key)), valueOf(s));
        }
        out.insert(QStringLiteral("rail"), valueOf(*findSetting(QStringLiteral("rail.background"))));
        out.insert(QStringLiteral("railEnd"), valueOf(*findSetting(QStringLiteral("rail.background_end"))));
        out.insert(QStringLiteral("launch"), valueOf(*findSetting(QStringLiteral("launch_button.background"))));
        out.insert(QStringLiteral("launchEnd"), valueOf(*findSetting(QStringLiteral("launch_button.background_end"))));
        QVariantMap radius;
        for (const char *tier : {"large", "medium", "small", "tiny"})
            radius.insert(QString::fromLatin1(tier), valueOf(*findSetting(QStringLiteral("radius.") + QString::fromLatin1(tier))));
        out.insert(QStringLiteral("radius"), radius);
        out.insert(QStringLiteral("launchRunning"), valueOf(*findSetting(QStringLiteral("launch_button.running_background"))));
        out.insert(QStringLiteral("launchInstall"), valueOf(*findSetting(QStringLiteral("launch_button.install_background"))));
        out.insert(QStringLiteral("railBorderWidth"), valueOf(*findSetting(QStringLiteral("rail.border_width"))));
        out.insert(QStringLiteral("searchRadius"), valueOf(*findSetting(QStringLiteral("search.radius"))));
        out.insert(QStringLiteral("searchBorderWidth"), valueOf(*findSetting(QStringLiteral("search.border_width"))));
        out.insert(QStringLiteral("launchRadius"), valueOf(*findSetting(QStringLiteral("launch_button.radius"))));
        out.insert(QStringLiteral("launchBorderWidth"), valueOf(*findSetting(QStringLiteral("launch_button.border_width"))));
        out.insert(QStringLiteral("launchLabel"), valueOf(*findSetting(QStringLiteral("launch_button.label"))));
        out.insert(QStringLiteral("railContainer"), valueOf(*findSetting(QStringLiteral("rail.container"))));
        out.insert(QStringLiteral("railContainerRadius"), valueOf(*findSetting(QStringLiteral("rail.container_radius"))));
        out.insert(QStringLiteral("railButtonRadius"), valueOf(*findSetting(QStringLiteral("rail.button_radius"))));
        out.insert(QStringLiteral("railWidth"), valueOf(*findSetting(QStringLiteral("rail.width"))));
        out.insert(QStringLiteral("railButtonSize"), valueOf(*findSetting(QStringLiteral("rail.button_size"))));
        out.insert(QStringLiteral("railSelected"), valueOf(*findSetting(QStringLiteral("rail.selected_background"))));
        out.insert(QStringLiteral("railHover"), valueOf(*findSetting(QStringLiteral("rail.hover_background"))));
        out.insert(QStringLiteral("railIcon"), valueOf(*findSetting(QStringLiteral("rail.icon"))));
        out.insert(QStringLiteral("railBorder"), valueOf(*findSetting(QStringLiteral("rail.border"))));
        out.insert(QStringLiteral("listBackground"), valueOf(*findSetting(QStringLiteral("list.background"))));
        out.insert(QStringLiteral("searchHeight"), valueOf(*findSetting(QStringLiteral("search.height"))));
        out.insert(QStringLiteral("launchHeight"), valueOf(*findSetting(QStringLiteral("launch_button.height"))));
        out.insert(QStringLiteral("searchPlaceholder"), valueOf(*findSetting(QStringLiteral("search.placeholder"))));
        out.insert(QStringLiteral("placeholderColor"), valueOf(*findSetting(QStringLiteral("search.placeholder_color"))));
        if (colorsChain.size() > 1)
            out.insert(QStringLiteral("extends"), colorsChain.at(1));
    }

    m_merged = merged;
    m_resolved = resolved;
    m_resolving.clear();
    m_problems = problems;
    return out;
}

QString Theme::styleDescription(const QString &style) const
{
    const auto read = [](const QString &path) {
        QFile file(path);
        if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
            return QString();
        const QByteArray data = file.readAll();
        try {
            const toml::table tbl = toml::parse(std::string_view(data.constData(), size_t(data.size())));
            if (const auto text = tbl["description"].value<std::string>())
                return capDescription(QString::fromStdString(*text));
        } catch (const toml::parse_error &) {
        }
        return QString();
    };
    QString text = read(halfPath(Half::Style, style));
    if (text.isEmpty() && isBuiltInName(Half::Style, style))
        text = read(halfPath(Half::Style, style, true));
    return text;
}

QString Theme::capDescription(const QString &text)
{
    const QString trimmed = text.simplified();
    return trimmed.size() > kMaxDescription ? trimmed.left(kMaxDescription - 1) + QChar(0x2026) : trimmed;
}

QString Theme::styleSummary(const QString &style)
{
    // Same swap as preview(): the resolver works on the active theme's members.
    const QHash<QString, Entry> merged = std::exchange(m_merged, {});
    const QHash<QString, QVariant> resolved = std::exchange(m_resolved, {});
    const QStringList problems = m_problems;
    m_resolving.clear();

    QStringList chain;
    const QString name = halfPath(Half::Style, style).isEmpty() ? kFallbackStyle : style;
    QStringList parts;
    if (applyHalfFile(Half::Style, name, false, m_merged, chain)) {
        QHash<QString, QVariant> set;
        for (const Setting &s : ThemeSchema::settings()) {
            if (m_merged.contains(fullKey(s)))
                set.insert(fullKey(s), valueOf(s));
        }
        const auto tier = [&](const char *which) {
            return valueOf(*findSetting(QStringLiteral("radius.") + QString::fromLatin1(which))).toInt();
        };
        QStringList radii;
        for (const char *which : {"large", "medium", "small", "tiny"})
            radii.append(QString::number(tier(which)));
        parts.append(tr("Radius %1").arg(radii.join(QStringLiteral(" · "))));

        m_merged.clear();
        m_resolved.clear();
        m_resolving.clear();
        int other = 0;
        for (const Setting &s : ThemeSchema::settings()) {
            const QString key = fullKey(s);
            if (!set.contains(key) || key.startsWith(QLatin1String("radius.")))
                continue;
            const QVariant plain = valueOf(s);
            if (set.value(key) == plain)
                continue;
            if (key == QLatin1String("layout.border_width") && set.value(key).toInt() == 0)
                parts.append(tr("no borders"));
            else if (key == QLatin1String("launch_button.label"))
                parts.append(set.value(key).toString());
            else if (key == QLatin1String("rail.container") && !set.value(key).toBool())
                parts.append(tr("no rail panel"));
            else
                ++other;
        }
        if (other > 0)
            parts.append(other == 1 ? tr("+1 other change") : tr("+%1 other changes").arg(other));
    }

    m_merged = merged;
    m_resolved = resolved;
    m_resolving.clear();
    m_problems = problems;
    return parts.join(QStringLiteral(" · "));
}

bool Theme::setStyleDescription(const QString &name, const QString &text)
{
    const QString path = userHalfPath(Half::Style, name);
    if (!QFile::exists(path) || isBuiltInName(Half::Style, name))
        return false;
    const QString capped = text.simplified().left(kMaxDescription);
    return capped.isEmpty() ? TomlPatch::removeValueInFile(path, QString(), QStringLiteral("description"))
                            : TomlPatch::setValueInFile(path, QString(), QStringLiteral("description"), quoted(capped));
}

std::optional<Theme::Half> Theme::halfNamed(const QString &word)
{
    if (word == QLatin1String("colors"))
        return Half::Colors;
    if (word == QLatin1String("style"))
        return Half::Style;
    return std::nullopt;
}

bool Theme::isBuiltInName(Half half, const QString &name) const
{
    return !halfPath(half, name, true).isEmpty();
}

QString Theme::editFilePath(Half half)
{
    const QString name = activeName(half);
    const QString path = userHalfPath(half, name);
    if (QFile::exists(path))
        return path;

    QString content;
    if (isBuiltInName(half, name)) {
        content = QStringLiteral("# Your changes to the built-in %1 %2. What you set here replaces the\n"
                                 "# built-in's value; delete this file to go back to it.\n"
                                 "# Every setting you can use is in themes/%3/reference.toml.\n")
                      .arg(name, word(half), folder(half));
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || (file.write(content.toUtf8()), !file.commit()))
        warn(QStringLiteral("could not create %1").arg(path));
    return path;
}

QString Theme::colorsFilePath()
{
    return editFilePath(Half::Colors);
}

QString Theme::styleFilePath()
{
    return editFilePath(Half::Style);
}

bool Theme::canResetToBuiltIn(const QString &half) const
{
    const auto h = halfNamed(half);
    return h && isBuiltInName(*h, activeName(*h)) && QFile::exists(userHalfPath(*h, activeName(*h)));
}

void Theme::dropHalfState(Half h)
{
    const auto ofHalf = [h](const QString &full) { return halfOf(full) == h; };
    for (auto it = m_overrides.begin(); it != m_overrides.end();)
        it = ofHalf(it.key()) ? m_overrides.erase(it) : std::next(it);
    for (auto it = m_pendingSaves.begin(); it != m_pendingSaves.end();)
        it = ofHalf(*it) ? m_pendingSaves.erase(it) : std::next(it);
    for (auto it = m_sessionStart.begin(); it != m_sessionStart.end();)
        it = ofHalf(it.key()) ? m_sessionStart.erase(it) : std::next(it);
    if (ofHalf(m_lastResetKey))
        m_lastResetKey.clear();
}

bool Theme::resetToBuiltIn(const QString &half)
{
    if (!canResetToBuiltIn(half))
        return false;
    const Half h = *halfNamed(half);

    dropHalfState(h);

    const QString path = userHalfPath(h, activeName(h));
    if (!QFile::remove(path)) {
        warn(QStringLiteral("could not delete %1").arg(path));
        emit problemsChanged();
        return false;
    }
    reload();
    watch();
    return true;
}

QString Theme::activeFile(const QString &section, const QString &key) const
{
    return activeName(halfOf(section + u'.' + key)) + QStringLiteral(".toml");
}

QVariant Theme::rawValue(const QString &section, const QString &key) const
{
    const auto it = m_merged.constFind(section + u'.' + key);
    return it == m_merged.constEnd() ? QVariant() : it->raw;
}

QString Theme::sourceLabel(const QString &section, const QString &key) const
{
    const auto it = m_merged.constFind(section + u'.' + key);
    if (it == m_merged.constEnd() || !it->user)
        return QStringLiteral("default");
    if (it->name == activeName(it->half))
        return QStringLiteral("%1.toml (%2)").arg(it->name, word(it->half));
    return QStringLiteral("inherited from %1").arg(it->name);
}

bool Theme::isCustomized(const QString &section, const QString &key) const
{
    const auto it = m_merged.constFind(section + u'.' + key);
    return it != m_merged.constEnd() && it->user && it->name == activeName(it->half);
}

QVariant Theme::resolvedValue(const QString &section, const QString &key) const
{
    QQmlPropertyMap *map = m_sections.value(section);
    return map ? map->value(camel(key)) : QVariant();
}

QVariantList Theme::paletteColors() const
{
    QQmlPropertyMap *palette = m_sections.value(QStringLiteral("palette"));
    QVariantList out;
    const auto add = [&](const QString &key) {
        out.append(QVariantMap{{QStringLiteral("name"), key}, {QStringLiteral("color"), palette->value(camel(key))}});
    };
    for (const Setting &s : ThemeSchema::settings()) {
        const QString key = toQString(s.key);
        if (s.section == "palette" && s.type == Type::Color && !key.endsWith(QLatin1String("_end")))
            add(key);
    }
    QStringList custom;
    for (auto it = m_merged.cbegin(); it != m_merged.cend(); ++it) {
        if (it.key().startsWith(QLatin1String("palette.")) && !findSetting(it.key())
            && !it.key().endsWith(QLatin1String("_fallback")) && palette->contains(camel(it.key().mid(8))))
            custom.append(it.key().mid(8));
    }
    custom.sort();
    for (const QString &key : std::as_const(custom))
        add(key);
    return out;
}

static QString unquoted(QString text)
{
    if (text.size() >= 2 && text.startsWith(u'"') && text.endsWith(u'"') && text.count(u'"') == 2)
        text = text.mid(1, text.size() - 2);
    return text;
}

static Type typeOf(const QString &full)
{
    const Setting *s = findSetting(full);
    return s ? s->type : Type::Color;
}

void Theme::setValue(const QString &section, const QString &key, const QVariant &value)
{
    const QString full = section + u'.' + key;

    rememberSessionStart(full);
    m_overrides.insert(full, toRawShape(value, typeOf(full)));
    m_pendingSaves.insert(full);
    applyOverridesOnTop();
    emit valueChanged(section, key);
    emit problemsChanged();
    emit sessionChangesChanged();

    scheduleSave(section, key);
}

void Theme::resetValue(const QString &section, const QString &key)
{
    const QString full = section + u'.' + key;
    rememberSessionStart(full);
    m_lastResetKey = full;
    m_lastResetRaw = ownRaw(full);
    m_overrides.remove(full);
    m_pendingSaves.remove(full);

    const QString path = userHalfPath(halfOf(full), activeName(halfOf(full)));
    if (QFile::exists(path)) {
        QString error;
        if (!TomlPatch::removeValueInFile(path, section, key, &error) && !error.isEmpty())
            warn(QStringLiteral("could not update %1: %2").arg(path, error));
    }
    reload();
}

void Theme::undoReset()
{
    const int dot = m_lastResetKey.indexOf(u'.');
    if (dot < 0 || !m_lastResetRaw.isValid())
        return;
    setValue(m_lastResetKey.left(dot), m_lastResetKey.mid(dot + 1), m_lastResetRaw);
    m_lastResetKey.clear();
}

QVariant Theme::ownRaw(const QString &full) const
{
    const auto it = m_merged.constFind(full);
    return it != m_merged.constEnd() && it->user && it->name == activeName(it->half) ? it->raw : QVariant();
}

QString Theme::resolvedText(const QString &full) const
{
    const int dot = full.indexOf(u'.');
    return formatScalar(resolvedValue(full.left(dot), full.mid(dot + 1)), typeOf(full));
}

QString Theme::shownValue(const QString &full, const QVariant &raw, const QString &resolved) const
{
    if (raw.isValid())
        return unquoted(formatScalar(raw, typeOf(full)));
    return QStringLiteral("default (%1)").arg(unquoted(resolved));
}

bool Theme::changedThisSession(const QString &full, const SessionStart &start) const
{
    const QVariant now = ownRaw(full);
    // Compared as written to the file: an editor may hand back a QColor for a "#rrggbb" string.
    if (start.raw.isValid() && now.isValid())
        return formatScalar(start.raw, typeOf(full)) != formatScalar(now, typeOf(full));
    // Added to or removed from the file: only a change if the value in effect differs, so
    // removing a value that equals the default isn't one.
    return resolvedText(full) != start.resolved;
}

void Theme::rememberSessionStart(const QString &full)
{
    if (!m_sessionStart.contains(full))
        m_sessionStart.insert(full, SessionStart{ownRaw(full), resolvedText(full)});
}

void Theme::beginEditSession()
{
    m_sessionStart.clear();
    m_lastResetKey.clear();
    emit reloaded();
    emit sessionChangesChanged();
}

void Theme::revertValue(const QString &section, const QString &key)
{
    const auto it = m_sessionStart.constFind(section + u'.' + key);
    if (it == m_sessionStart.constEnd())
        return;
    if (it->raw.isValid())
        setValue(section, key, it->raw);
    else
        resetValue(section, key);
}

void Theme::revertAll()
{
    const QList<QString> keys = m_sessionStart.keys();
    for (const QString &full : keys) {
        if (!changedThisSession(full, m_sessionStart.value(full)))
            continue;
        const int dot = full.indexOf(u'.');
        revertValue(full.left(dot), full.mid(dot + 1));
    }
}

QString Theme::revertText(const QString &section, const QString &key) const
{
    const QString full = section + u'.' + key;
    const auto it = m_sessionStart.constFind(full);
    if (it == m_sessionStart.constEnd() || !it->raw.isValid() || !changedThisSession(full, *it))
        return {};
    return shownValue(full, it->raw, it->resolved);
}

QVariant Theme::parentRaw(const QString &full)
{
    const Half half = halfOf(full);
    QHash<QString, Entry> parents;
    QStringList chain;
    m_quiet = true;
    applyHalfFile(half, activeName(half), false, parents, chain, true);
    m_quiet = false;
    const auto it = parents.constFind(full);
    return it == parents.constEnd() ? QVariant() : it->raw;
}

QString Theme::describeRaw(const QString &full, const QVariant &raw) const
{
    if (raw.typeId() == QMetaType::QString && raw.toString().startsWith(u'@')) {
        const QString ref = raw.toString().mid(1);
        const QString target = ref.contains(u'.') ? ref : QStringLiteral("palette.") + ref;
        return QStringLiteral("%1, %2").arg(ref, resolvedText(target));
    }
    return shownValue(full, raw, {});
}

QVariantMap Theme::rowMenu(const QString &section, const QString &key)
{
    const QString full = section + u'.' + key;
    const Setting *s = findSetting(full);
    if (!s)
        return {};
    QVariantMap out;
    out.insert(QStringLiteral("revert"), revertText(section, key));
    const QVariant def = defaultRaw(*s);
    if (isCustomized(section, key)) {
        const QVariant parent = parentRaw(full);
        out.insert(QStringLiteral("file"), activeFile(section, key));
        out.insert(QStringLiteral("fallback"), describeRaw(full, parent.isValid() ? parent : def));
    }
    const auto it = m_merged.constFind(full);
    out.insert(QStringLiteral("fromParent"), it != m_merged.constEnd() && it->user && it->name != activeName(it->half));
    out.insert(QStringLiteral("default"), describeRaw(full, def));
    QString defaultTarget;
    if (def.typeId() == QMetaType::QString && def.toString().startsWith(u'@')) {
        defaultTarget = def.toString().mid(1);
        if (!defaultTarget.contains(u'.'))
            defaultTarget.prepend(QLatin1String("palette."));
    }
    out.insert(QStringLiteral("defaultTarget"), defaultTarget);
    out.insert(QStringLiteral("defaultValue"),
               defaultTarget.isEmpty() ? shownValue(full, def, {}) : unquoted(resolvedText(defaultTarget)));
    out.insert(QStringLiteral("current"), shownValue(full, resolvedValue(section, key), {}));
    return out;
}

void Theme::useDefault(const QString &section, const QString &key)
{
    if (const Setting *s = findSetting(section + u'.' + key))
        setValue(section, key, defaultRaw(*s));
}

void Theme::useOwnValue(const QString &section, const QString &key)
{
    setValue(section, key, resolvedValue(section, key));
}

QVariantList Theme::sessionChanges() const
{
    QVariantList out;
    for (const Setting &s : ThemeSchema::settings()) {
        const QString full = toQString(s.section) + u'.' + toQString(s.key);
        const auto it = m_sessionStart.constFind(full);
        if (it == m_sessionStart.constEnd() || !changedThisSession(full, *it))
            continue;
        out.append(QVariantMap{
            {QStringLiteral("section"), toQString(s.section)},
            {QStringLiteral("key"), toQString(s.key)},
            {QStringLiteral("half"), ThemeSchema::isColorSetting(s) ? QStringLiteral("colors") : QStringLiteral("style")},
            {QStringLiteral("before"), shownValue(full, it->raw, it->resolved)},
            {QStringLiteral("after"), shownValue(full, ownRaw(full), resolvedText(full))},
        });
    }
    return out;
}

void Theme::scheduleSave(const QString &, const QString &)
{
    m_saveTimer.start();
}

void Theme::flushPendingSaves()
{
    if (m_pendingSaves.isEmpty())
        return;

    const QSet<QString> keys = m_pendingSaves;
    m_pendingSaves.clear();

    for (const QString &full : keys) {
        const int dot = full.indexOf(u'.');
        const QString section = full.left(dot);
        const QString key = full.mid(dot + 1);
        const Setting *s = findSetting(full);
        const Type type = s ? s->type : Type::Color;
        const QString formatted = formatScalar(m_overrides.value(full), type);

        const Half half = halfOf(full);
        const QString path = userHalfPath(half, activeName(half));
        QString error;
        if (!QFile::exists(path)) {
            QFile created(path);
            if (!created.open(QIODevice::WriteOnly))
                error = QStringLiteral("could not create %1").arg(path);
        }
        if (error.isEmpty() && TomlPatch::setValueInFile(path, section, key, formatted, &error)) {
            m_overrides.remove(full);
        } else {
            warn(QStringLiteral("could not save %1: %2").arg(full, error));
            m_pendingSaves.insert(full); // retry on the next edit
        }
    }
    emit problemsChanged();
}

void Theme::setAppearance(const QString &colors, const QString &style)
{
    if (colors == m_activeColors && style == m_activeStyle)
        return;

    flushPendingSaves();
    m_saveTimer.stop();
    m_overrides.clear();
    m_pendingSaves.clear();
    m_sessionStart.clear();
    m_lastResetKey.clear();

    const QString path = m_dataDir + QStringLiteral("/settings.toml");
    for (const auto &[key, value] : {std::pair(QStringLiteral("colors"), colors), std::pair(QStringLiteral("style"), style)}) {
        QString error;
        if (!TomlPatch::setValueInFile(path, QStringLiteral("appearance"), key, quoted(value), &error)) {
            warn(QStringLiteral("could not switch theme: %1").arg(error));
            return;
        }
    }
    reload();
    watch();
}

void Theme::setColors(const QString &name)
{
    if (!name.isEmpty())
        setAppearance(name, m_activeStyle);
}

void Theme::setStyle(const QString &name)
{
    if (!name.isEmpty())
        setAppearance(m_activeColors, name);
}

void Theme::setTheme(const QString &name)
{
    const auto pair = pairOf(name);
    if (!pair)
        return;
    setAppearance(pair->colors.isEmpty() ? m_activeColors : pair->colors,
                  pair->style.isEmpty() ? m_activeStyle : pair->style);
}

bool Theme::createHalf(Half half, const QString &name, const QString &basedOn, const QString &description)
{
    if (!nameError(word(half), name).isEmpty())
        return false;

    const QString path = userHalfPath(half, name);
    QString content = QStringLiteral("extends = %1\n").arg(quoted(basedOn.isEmpty() ? activeName(half) : basedOn));
    const QString capped = description.simplified().left(kMaxDescription);
    if (half == Half::Style && !capped.isEmpty())
        content += QStringLiteral("description = %1\n").arg(quoted(capped));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || (file.write(content.toUtf8()), !file.commit())) {
        warn(QStringLiteral("could not create %1").arg(path));
        return false;
    }
    if (half == Half::Colors)
        setAppearance(name, m_activeStyle);
    else
        setAppearance(m_activeColors, name);
    return true;
}

bool Theme::createColors(const QString &name, const QString &basedOn)
{
    return createHalf(Half::Colors, name, basedOn);
}

bool Theme::createStyle(const QString &name, const QString &basedOn, const QString &description)
{
    return createHalf(Half::Style, name, basedOn, description);
}

QString Theme::nameError(const QString &kind, const QString &name) const
{
    if (name.trimmed().isEmpty())
        return tr("Give it a name");
    if (name.contains(u'/') || name.contains(u'\\'))
        return tr("A name can't contain / or \\");
    if (name == QLatin1String("reference"))
        return tr("\"reference\" is the file listing every setting");
    const bool theme = kind == QLatin1String("theme");
    const std::optional<Half> half = halfNamed(kind);
    if (!theme && !half)
        return tr("Not a kind of theme file");
    const QStringList builtIns = theme ? kBuiltInThemes : (*half == Half::Colors ? kBuiltInColors : kBuiltInStyles);
    if (builtIns.contains(name))
        return tr("%1 is built in").arg(name);
    if (!theme && namesIn(*half).contains(name))
        return tr("There is already a %1 called %2").arg(kind == QLatin1String("colors") ? tr("colors") : tr("style"), name);
    return {};
}

bool Theme::themeExists(const QString &name) const
{
    return pairOf(name).has_value();
}

bool Theme::saveTheme(const QString &name, bool replace)
{
    if (!nameError(QStringLiteral("theme"), name).isEmpty() || (themeExists(name) && !replace))
        return false;

    const QString path = userThemePath(name);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || (file.write(QStringLiteral("colors = %1\nstyle = %2\n").arg(quoted(m_activeColors), quoted(m_activeStyle)).toUtf8()),
            !file.commit())) {
        warn(QStringLiteral("could not create %1").arg(path));
        return false;
    }
    reload();
    watch();
    return true;
}

QVariantMap Theme::usedBy(const QString &kind, const QString &name) const
{
    const std::optional<Half> half = halfNamed(kind);
    if (!half || name.isEmpty())
        return {};

    QStringList themes;
    for (const QString &theme : availableThemes()) {
        const auto pair = pairOf(theme);
        if (pair && (*half == Half::Colors ? pair->colors : pair->style) == name)
            themes.append(theme);
    }

    QStringList files;
    const QDir dir(m_dataDir + QStringLiteral("/themes/") + folder(*half));
    for (const QString &f : dir.entryList({QStringLiteral("*.toml")}, QDir::Files)) {
        const QString other = QFileInfo(f).completeBaseName();
        if (other == name || other == QLatin1String("reference"))
            continue;
        try {
            const toml::table tbl = toml::parse_file(dir.filePath(f).toStdString());
            if (tbl["extends"].value<std::string>() == name.toStdString())
                files.append(other);
        } catch (const toml::parse_error &) {
        }
    }
    files.sort(Qt::CaseInsensitive);
    return {{QStringLiteral("themes"), themes}, {QStringLiteral("files"), files}};
}

bool Theme::deleteUserFile(const QString &kind, const QString &name, bool permanent)
{
    m_needsPermanent = false;
    const std::optional<Half> half = halfNamed(kind);
    const bool theme = kind == QLatin1String("theme");
    if ((!theme && !half) || name.isEmpty() || name.contains(u'/') || name.contains(u'\\')
        || name == QLatin1String("reference"))
        return false;
    const QStringList builtIns = theme ? kBuiltInThemes : (*half == Half::Colors ? kBuiltInColors : kBuiltInStyles);
    const QString path = theme ? userThemePath(name) : userHalfPath(*half, name);
    if (builtIns.contains(name) || !QFile::exists(path))
        return false;

    const Uninstall::Removal removal = Uninstall::remove(path, permanent, m_trash);
    if (!removal.error.isEmpty()) {
        m_needsPermanent = removal.needsPermanent;
        if (!removal.needsPermanent) {
            warn(QStringLiteral("could not delete %1: %2").arg(path, removal.error));
            emit problemsChanged();
        }
        return false;
    }

    // The file is gone before the switch, so a failed trash never leaves settings.toml changed.
    if (!theme && name == activeName(*half)) {
        dropHalfState(*half);
        if (*half == Half::Colors)
            setAppearance(kFallbackColors, m_activeStyle);
        else
            setAppearance(m_activeColors, kFallbackStyle);
    } else {
        reload();
        watch();
    }
    return true;
}

bool Theme::renameUserFile(const QString &kind, const QString &name, const QString &newName)
{
    const std::optional<Half> half = halfNamed(kind);
    const bool theme = kind == QLatin1String("theme");
    if ((!theme && !half) || name.isEmpty() || name.contains(u'/') || name.contains(u'\\')
        || name == QLatin1String("reference"))
        return false;
    const QStringList builtIns = theme ? kBuiltInThemes : (*half == Half::Colors ? kBuiltInColors : kBuiltInStyles);
    const QString from = theme ? userThemePath(name) : userHalfPath(*half, name);
    const QString to = theme ? userThemePath(newName) : userHalfPath(*half, newName);
    if (builtIns.contains(name) || !QFile::exists(from) || !nameError(kind, newName).isEmpty() || QFile::exists(to))
        return false;

    // Edits waiting for the timer belong in the file under its old name.
    flushPendingSaves();
    if (!QFile::rename(from, to)) {
        warn(QStringLiteral("could not rename %1 to %2").arg(from, to));
        emit problemsChanged();
        return false;
    }

    if (!theme) {
        const auto repoint = [this, &newName](const QString &path, const QString &section, const QString &key) {
            QString error;
            if (!TomlPatch::setValueInFile(path, section, key, quoted(newName), &error))
                warn(QStringLiteral("could not update %1: %2").arg(path, error));
        };

        const QDir halfDir(m_dataDir + QStringLiteral("/themes/") + folder(*half));
        for (const QString &f : halfDir.entryList({QStringLiteral("*.toml")}, QDir::Files)) {
            const QString path = halfDir.filePath(f);
            try {
                const toml::table tbl = toml::parse_file(path.toStdString());
                if (tbl["extends"].value<std::string>() == name.toStdString())
                    repoint(path, QString(), QStringLiteral("extends"));
            } catch (const toml::parse_error &) {
            }
        }

        const QDir themeDir(m_dataDir + QStringLiteral("/themes"));
        for (const QString &f : themeDir.entryList({QStringLiteral("*.toml")}, QDir::Files)) {
            const QString path = themeDir.filePath(f);
            const auto pair = readPair(path);
            if (pair && (*half == Half::Colors ? pair->colors : pair->style) == name)
                repoint(path, QString(), word(*half));
        }

        if (readAppearance(*half) == name)
            repoint(m_dataDir + QStringLiteral("/settings.toml"), QStringLiteral("appearance"), word(*half));
    }

    reload();
    watch();
    return true;
}

bool Theme::renameColors(const QString &name, const QString &newName)
{
    return renameUserFile(QStringLiteral("colors"), name, newName);
}

bool Theme::renameStyle(const QString &name, const QString &newName)
{
    return renameUserFile(QStringLiteral("style"), name, newName);
}

bool Theme::renameTheme(const QString &name, const QString &newName)
{
    return renameUserFile(QStringLiteral("theme"), name, newName);
}

bool Theme::deleteColors(const QString &name, bool permanent)
{
    return deleteUserFile(QStringLiteral("colors"), name, permanent);
}

bool Theme::deleteStyle(const QString &name, bool permanent)
{
    return deleteUserFile(QStringLiteral("style"), name, permanent);
}

bool Theme::deleteTheme(const QString &name, bool permanent)
{
    return deleteUserFile(QStringLiteral("theme"), name, permanent);
}
