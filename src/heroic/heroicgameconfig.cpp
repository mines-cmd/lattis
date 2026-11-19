#include "heroicgameconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace HeroicGameConfig {
namespace {
using Edit = QPair<QString, QJsonValue>;

const QString kUnit = QStringLiteral("  ");

QJsonObject wineObject(const HeroicWine &wine)
{
    QJsonObject object{{QStringLiteral("bin"), wine.bin}, {QStringLiteral("name"), wine.name},
                       {QStringLiteral("type"), wine.type}};
    if (!wine.wineserver.isEmpty())
        object.insert(QStringLiteral("wineserver"), wine.wineserver);
    return object;
}

QList<Edit> editList(const HeroicGameEdits &e)
{
    QList<Edit> out;
    if (e.launcherArgs)
        out.append({QStringLiteral("launcherArgs"), *e.launcherArgs});
    if (e.showMangohud)
        out.append({QStringLiteral("showMangohud"), *e.showMangohud});
    if (e.useGameMode)
        out.append({QStringLiteral("useGameMode"), *e.useGameMode});
    if (e.environment)
        out.append({QStringLiteral("enviromentOptions"), *e.environment});
    if (e.wrappers)
        out.append({QStringLiteral("wrapperOptions"), *e.wrappers});
    if (e.wine)
        out.append({QStringLiteral("wineVersion"), wineObject(*e.wine)});
    if (e.winePrefix)
        out.append({QStringLiteral("winePrefix"), *e.winePrefix});
    return out;
}

QList<Edit> editList(const QJsonObject &object)
{
    QList<Edit> out;
    for (auto it = object.begin(); it != object.end(); ++it)
        out.append({it.key(), it.value()});
    return out;
}

// Heroic's own layout: two spaces, one member per line, empty containers as {} and [].
QString serialize(const QJsonValue &value, const QString &indent)
{
    const QString inner = indent + kUnit;
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.isEmpty())
            return QStringLiteral("{}");
        QStringList lines;
        for (auto it = object.begin(); it != object.end(); ++it)
            lines.append(inner + serialize(it.key(), {}) + QStringLiteral(": ") + serialize(it.value(), inner));
        return QStringLiteral("{\n") + lines.join(QStringLiteral(",\n")) + u'\n' + indent + u'}';
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (array.isEmpty())
            return QStringLiteral("[]");
        QStringList lines;
        for (const QJsonValue &item : array)
            lines.append(inner + serialize(item, inner));
        return QStringLiteral("[\n") + lines.join(QStringLiteral(",\n")) + u'\n' + indent + u']';
    }
    const QByteArray json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

struct Member
{
    QString key;
    qsizetype keyBegin = 0;
    qsizetype valBegin = 0;
    qsizetype valEnd = 0;
};

qsizetype skipSpace(const QString &t, qsizetype pos)
{
    while (pos < t.size() && t[pos].isSpace())
        ++pos;
    return pos;
}

// `pos` is at the opening quote; returns the position after the closing one.
qsizetype skipString(const QString &t, qsizetype pos)
{
    ++pos;
    while (pos < t.size() && t[pos] != u'"')
        pos += t[pos] == u'\\' ? 2 : 1;
    return pos + 1;
}

qsizetype skipValue(const QString &t, qsizetype pos)
{
    if (t[pos] == u'"')
        return skipString(t, pos);
    if (t[pos] == u'{' || t[pos] == u'[') {
        int depth = 0;
        while (pos < t.size()) {
            const QChar c = t[pos];
            if (c == u'"') {
                pos = skipString(t, pos);
                continue;
            }
            if (c == u'{' || c == u'[')
                ++depth;
            else if (c == u'}' || c == u']')
                --depth;
            ++pos;
            if (depth == 0)
                break;
        }
        return pos;
    }
    while (pos < t.size() && !t[pos].isSpace() && t[pos] != u',' && t[pos] != u'}' && t[pos] != u']')
        ++pos;
    return pos;
}

// `t` holds valid JSON (checked by the caller), and `open` is the position of an object's `{`.
QList<Member> members(const QString &t, qsizetype open)
{
    QList<Member> out;
    qsizetype pos = skipSpace(t, open + 1);
    while (pos < t.size() && t[pos] == u'"') {
        Member m;
        m.keyBegin = pos;
        const qsizetype end = skipString(t, pos);
        m.key = t.mid(pos + 1, end - pos - 2);
        pos = skipSpace(t, skipSpace(t, end) + 1);
        m.valBegin = pos;
        m.valEnd = skipValue(t, pos);
        out.append(m);
        pos = skipSpace(t, m.valEnd);
        if (pos < t.size() && t[pos] == u',')
            pos = skipSpace(t, pos + 1);
    }
    return out;
}

QString memberIndent(const QString &t, const QList<Member> &list, const QString &indent)
{
    const qsizetype lineStart = t.lastIndexOf(u'\n', list.last().keyBegin);
    if (lineStart < 0)
        return indent + kUnit;
    const QString prefix = t.mid(lineStart + 1, list.last().keyBegin - lineStart - 1);
    return prefix.trimmed().isEmpty() ? prefix : indent + kUnit;
}

struct Op
{
    qsizetype begin;
    qsizetype end;
    QString text;
};

// `object` is one object's text (from its `{` to its `}`) and `indent` the indentation of the line it ends on.
QString patchObject(const QString &object, const QList<Edit> &edits, const QString &indent)
{
    const QList<Member> list = members(object, 0);
    if (list.isEmpty()) {
        QJsonObject full;
        for (const Edit &edit : edits)
            full.insert(edit.first, edit.second);
        return serialize(full, indent);
    }
    const QString inner = memberIndent(object, list, indent);
    QList<Op> ops;
    QString added;
    for (const Edit &edit : edits) {
        const Member *found = nullptr;
        for (const Member &m : list)
            if (m.key == edit.first)
                found = &m;
        if (!found) {
            added += QStringLiteral(",\n") + inner + serialize(edit.first, {}) + QStringLiteral(": ")
                + serialize(edit.second, inner);
            continue;
        }
        const QString current = object.mid(found->valBegin, found->valEnd - found->valBegin);
        // Keys Lattis doesn't edit inside an object (wineVersion's wineserver) stay.
        const QString text = edit.second.isObject() && current.startsWith(u'{')
            ? patchObject(current, editList(edit.second.toObject()), inner)
            : serialize(edit.second, inner);
        ops.append({found->valBegin, found->valEnd, text});
    }
    if (!added.isEmpty())
        ops.append({list.last().valEnd, list.last().valEnd, added});
    QString out = object;
    std::sort(ops.begin(), ops.end(), [](const Op &a, const Op &b) { return a.begin > b.begin; });
    for (const Op &op : ops)
        out.replace(op.begin, op.end - op.begin, op.text);
    return out;
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject defaultSettings(const QString &root)
{
    return readObject(root + QStringLiteral("/config.json")).value(QStringLiteral("defaultSettings")).toObject();
}

QJsonObject fullEntry(const QString &root, const QList<Edit> &edits)
{
    QJsonObject entry = defaultSettings(root);
    for (const Edit &edit : edits) {
        const QJsonValue current = entry.value(edit.first);
        if (edit.second.isObject() && current.isObject()) {
            QJsonObject merged = current.toObject();
            const QJsonObject changes = edit.second.toObject();
            for (auto it = changes.begin(); it != changes.end(); ++it)
                merged.insert(it.key(), it.value());
            entry.insert(edit.first, merged);
        } else {
            entry.insert(edit.first, edit.second);
        }
    }
    return entry;
}

QString wholeFile(const QString &appName, const QJsonObject &entry)
{
    return QStringLiteral("{\n  ") + serialize(appName, {}) + QStringLiteral(": ") + serialize(entry, kUnit)
        + QStringLiteral(",\n  \"version\": \"v0\",\n  \"explicit\": true\n}");
}

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}
}

QString path(const QString &root, const QString &appName)
{
    return root + QStringLiteral("/GamesConfig/") + appName + QStringLiteral(".json");
}

bool read(const QString &root, const QString &appName, HeroicGameSettings &out, QString *error)
{
    QJsonObject entry;
    QFile file(path(root, appName));
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        if (!bytes.trimmed().isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(bytes);
            if (!doc.isObject())
                return fail(error, QStringLiteral("%1: not a JSON object").arg(file.fileName()));
            entry = doc.object().value(appName).toObject();
        }
    }
    const QJsonObject defaults = defaultSettings(root);
    const auto value = [&](const char *key) {
        const QString k = QLatin1String(key);
        return entry.contains(k) ? entry.value(k) : defaults.value(k);
    };

    HeroicGameSettings settings;
    settings.launcherArgs = value("launcherArgs").toString();
    settings.showMangohud = value("showMangohud").toBool();
    settings.useGameMode = value("useGameMode").toBool();
    settings.environment = value("enviromentOptions").toArray();
    settings.wrappers = value("wrapperOptions").toArray();
    const QJsonObject wine = value("wineVersion").toObject();
    settings.wine = {wine.value(QStringLiteral("bin")).toString(), wine.value(QStringLiteral("name")).toString(),
                     wine.value(QStringLiteral("type")).toString(), wine.value(QStringLiteral("wineserver")).toString()};
    out = settings;
    return true;
}

bool write(const QString &root, const QString &appName, const HeroicGameEdits &edits, QString *error)
{
    const QString filePath = path(root, appName);
    const QList<Edit> list = editList(edits);

    QString text;
    {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.readAll();
            if (!bytes.trimmed().isEmpty()) {
                if (!QJsonDocument::fromJson(bytes).isObject())
                    return fail(error, QStringLiteral("%1: not a JSON object").arg(filePath));
                text = QString::fromUtf8(bytes);
            }
        }
    }

    const qsizetype open = text.isEmpty() ? -1 : skipSpace(text, 0);
    const QList<Member> top = open < 0 ? QList<Member>{} : members(text, open);
    if (top.isEmpty()) {
        text = wholeFile(appName, fullEntry(root, list));
    } else {
        const Member *found = nullptr;
        for (const Member &m : top)
            if (m.key == appName)
                found = &m;
        const QString indent = memberIndent(text, top, {});
        if (!found) {
            text.insert(open + 1, u'\n' + indent + serialize(appName, {}) + QStringLiteral(": ")
                                      + serialize(fullEntry(root, list), indent) + u',');
        } else {
            const QString current = text.mid(found->valBegin, found->valEnd - found->valBegin);
            const bool empty = !current.startsWith(u'{') || members(current, 0).isEmpty();
            text.replace(found->valBegin, found->valEnd - found->valBegin,
                         empty ? serialize(fullEntry(root, list), indent) : patchObject(current, list, indent));
        }
    }

    if (!QDir().mkpath(QFileInfo(filePath).absolutePath()))
        return fail(error, QStringLiteral("%1: can't create the folder").arg(filePath));
    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit())
        return fail(error, QStringLiteral("%1: can't write the file").arg(filePath));
    return true;
}

QJsonObject editsToJson(const HeroicGameEdits &e)
{
    QJsonObject o;
    if (e.launcherArgs)
        o.insert(QStringLiteral("launcherArgs"), *e.launcherArgs);
    if (e.showMangohud)
        o.insert(QStringLiteral("showMangohud"), *e.showMangohud);
    if (e.useGameMode)
        o.insert(QStringLiteral("useGameMode"), *e.useGameMode);
    if (e.environment)
        o.insert(QStringLiteral("environment"), *e.environment);
    if (e.wrappers)
        o.insert(QStringLiteral("wrappers"), *e.wrappers);
    if (e.wine) {
        QJsonObject wine = wineObject(*e.wine);
        wine.insert(QStringLiteral("wineserver"), e.wine->wineserver);
        o.insert(QStringLiteral("wine"), wine);
    }
    if (e.winePrefix)
        o.insert(QStringLiteral("winePrefix"), *e.winePrefix);
    return o;
}

HeroicGameEdits editsFromJson(const QJsonObject &o)
{
    HeroicGameEdits e;
    if (o.contains(QStringLiteral("launcherArgs")))
        e.launcherArgs = o.value(QStringLiteral("launcherArgs")).toString();
    if (o.contains(QStringLiteral("showMangohud")))
        e.showMangohud = o.value(QStringLiteral("showMangohud")).toBool();
    if (o.contains(QStringLiteral("useGameMode")))
        e.useGameMode = o.value(QStringLiteral("useGameMode")).toBool();
    if (o.contains(QStringLiteral("environment")))
        e.environment = o.value(QStringLiteral("environment")).toArray();
    if (o.contains(QStringLiteral("wrappers")))
        e.wrappers = o.value(QStringLiteral("wrappers")).toArray();
    if (const QJsonObject wine = o.value(QStringLiteral("wine")).toObject(); !wine.isEmpty()) {
        e.wine = HeroicWine{wine.value(QStringLiteral("bin")).toString(), wine.value(QStringLiteral("name")).toString(),
                            wine.value(QStringLiteral("type")).toString(),
                            wine.value(QStringLiteral("wineserver")).toString()};
    }
    if (o.contains(QStringLiteral("winePrefix")))
        e.winePrefix = o.value(QStringLiteral("winePrefix")).toString();
    return e;
}

void merge(HeroicGameEdits &into, const HeroicGameEdits &later)
{
    if (later.launcherArgs)
        into.launcherArgs = later.launcherArgs;
    if (later.showMangohud)
        into.showMangohud = later.showMangohud;
    if (later.useGameMode)
        into.useGameMode = later.useGameMode;
    if (later.environment)
        into.environment = later.environment;
    if (later.wrappers)
        into.wrappers = later.wrappers;
    if (later.wine)
        into.wine = later.wine;
    if (later.winePrefix)
        into.winePrefix = later.winePrefix;
}

void apply(HeroicGameSettings &s, const HeroicGameEdits &e)
{
    if (e.launcherArgs)
        s.launcherArgs = *e.launcherArgs;
    if (e.showMangohud)
        s.showMangohud = *e.showMangohud;
    if (e.useGameMode)
        s.useGameMode = *e.useGameMode;
    if (e.environment)
        s.environment = *e.environment;
    if (e.wrappers)
        s.wrappers = *e.wrappers;
    if (e.wine)
        s.wine = *e.wine;
}

bool isEmpty(const HeroicGameEdits &e)
{
    return !(e.launcherArgs || e.showMangohud || e.useGameMode || e.environment || e.wrappers || e.wine || e.winePrefix);
}

QString defaultPrefixDir(const QString &root)
{
    return defaultSettings(root).value(QStringLiteral("defaultWinePrefixDir")).toString().trimmed();
}

QString sharedPrefix(const QString &root)
{
    QString prefix = defaultSettings(root).value(QStringLiteral("winePrefix")).toString().trimmed();
    if (prefix.startsWith(u'~'))
        prefix.replace(0, 1, QDir::homePath());
    return prefix;
}
}
