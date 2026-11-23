#include "offereddefaults.h"
#include "tomlstring.h"

#include <toml++/toml.hpp>

#include <algorithm>

namespace OfferedDefaults {
namespace {

// Frozen: the default platforms and emulators of the versions before `offered` existed. A
// default added later goes only in resources/platforms.toml.
const char *const kBootstrap[] = {
    "switch", "switch/ryubing", "3ds", "3ds/azahar", "wiiu", "wiiu/cemu", "wii", "wii/dolphin",
    "gamecube", "gamecube/dolphin", "n64", "n64/rmg", "nds", "nds/melonds", "gba", "gba/mgba",
    "ps4", "ps4/shadps4", "ps3", "ps3/rpcs3", "ps2", "ps2/pcsx2", "ps1", "ps1/duckstation",
    "psp", "psp/ppsspp",
};

const QString kComment = QStringLiteral(
    "# ids Lattis has offered as defaults; it adds a default that isn't here once, and delete one from here\n"
    "# to have it offered again.\n");

struct DefaultPlatform
{
    QString id;
    QString name;
    QStringList emulators;
    QList<std::pair<QString, QString>> values;
};


QString formatted(const toml::node &node)
{
    if (const auto *s = node.as_string())
        return tomlQuote(QString::fromStdString(s->get()));
    if (const auto *a = node.as_array()) {
        QStringList items;
        for (const toml::node &item : *a)
            items.append(formatted(item));
        return u'[' + items.join(QStringLiteral(", ")) + u']';
    }
    if (const auto *i = node.as_integer())
        return QString::number(i->get());
    if (const auto *b = node.as_boolean())
        return b->get() ? QStringLiteral("true") : QStringLiteral("false");
    if (const auto *f = node.as_floating_point())
        return QString::number(f->get());
    return QStringLiteral("\"\"");
}

QStringList stringList(const toml::node *node, bool *ok = nullptr)
{
    QStringList out;
    const auto *array = node ? node->as_array() : nullptr;
    if (ok)
        *ok = array != nullptr;
    if (!array)
        return out;
    for (const toml::node &item : *array) {
        const auto *s = item.as_string();
        if (!s) {
            if (ok)
                *ok = false;
            continue;
        }
        out.append(QString::fromStdString(s->get()));
    }
    return out;
}

TomlPatch::Result failure(const QString &error)
{
    return {false, {}, error};
}

QString offeredValue(const QStringList &items)
{
    QString out = QStringLiteral("[\n");
    bool open = false;
    for (const QString &item : items) {
        if (!item.contains(u'/')) {
            if (open)
                out += u'\n';
            out += QStringLiteral("    ");
            open = true;
        } else if (!open) {
            out += QStringLiteral("    ");
            open = true;
        } else {
            out += u' ';
        }
        out += tomlQuote(item) + u',';
    }
    return out + (open ? QStringLiteral("\n]") : QStringLiteral("]"));
}

QStringList emulatorsOf(const QString &text, const QString &id)
{
    toml::table root;
    try {
        root = toml::parse(text.toStdString());
    } catch (const toml::parse_error &) {
        return {};
    }
    const auto *platforms = root["platform"].as_array();
    if (!platforms)
        return {};
    for (const toml::node &entry : *platforms) {
        const auto *t = entry.as_table();
        const auto *key = t ? t->get("id") : nullptr;
        if (key && key->is_string() && QString::fromStdString(key->as_string()->get()) == id)
            return stringList(t->get("emulators"));
    }
    return {};
}

} // namespace

QStringList bootstrap()
{
    QStringList out;
    for (const char *item : kBootstrap)
        out.append(QString::fromLatin1(item));
    return out;
}

TomlPatch::Result apply(const QString &text, const QString &defaults, const QStringList &bootstrap,
                        QStringList *added)
{
    toml::table defaultsRoot;
    toml::table root;
    try {
        defaultsRoot = toml::parse(defaults.toStdString());
    } catch (const toml::parse_error &e) {
        return failure(QStringLiteral("the default platforms: ") + QString::fromUtf8(e.description().data()));
    }
    try {
        root = toml::parse(text.toStdString());
    } catch (const toml::parse_error &e) {
        return failure(QString::fromUtf8(e.description().data()));
    }

    QList<DefaultPlatform> platforms;
    if (const auto *array = defaultsRoot["platform"].as_array()) {
        for (const toml::node &entry : *array) {
            const auto *t = entry.as_table();
            const auto *id = t ? t->get("id") : nullptr;
            if (!id || !id->is_string())
                continue;
            DefaultPlatform p;
            p.id = QString::fromStdString(id->as_string()->get());
            if (const auto *name = t->get("name"); name && name->is_string())
                p.name = QString::fromStdString(name->as_string()->get());
            p.emulators = stringList(t->get("emulators"));
            p.values = {{QStringLiteral("id"), formatted(*id)}};
            for (auto &&[k, node] : *t) {
                if (k.str() != "id")
                    p.values.append({QString::fromUtf8(k.str().data(), qsizetype(k.str().size())), formatted(node)});
            }
            // `id` first, then `name`, then the rest as the table lists them.
            const auto nameAt = std::find_if(p.values.begin(), p.values.end(),
                                             [](const auto &v) { return v.first == QLatin1String("name"); });
            if (nameAt != p.values.end() && nameAt != p.values.begin() + 1)
                std::rotate(p.values.begin() + 1, nameAt, nameAt + 1);
            platforms.append(p);
        }
    }

    const toml::node *offeredNode = root.get("offered");
    bool offeredOk = true;
    QStringList offered = offeredNode ? stringList(offeredNode, &offeredOk) : bootstrap;
    if (!offeredOk)
        return failure(QStringLiteral("offered must be a list of text in quotes, like [\"switch\", \"switch/ryubing\"]"));
    const QStringList previous = offered;

    QString current = text;
    QStringList ids = TomlPatch::blockValues(current, QStringLiteral("platform"), QStringLiteral("id"));
    QStringList considered;
    QStringList log;
    for (qsizetype i = 0; i < platforms.size(); ++i) {
        const DefaultPlatform &p = platforms.at(i);
        bool placed = false;
        if (!offered.contains(p.id)) {
            considered.append(p.id);
            if (!ids.contains(p.id)) {
                TomlPatch::Result r = TomlPatch::appendBlock(current, QStringLiteral("platform"), p.values);
                if (!r.ok)
                    return r;
                const QStringList all = TomlPatch::blockValues(r.text, QStringLiteral("platform"), QStringLiteral("id"));
                qsizetype after = -1;
                for (qsizetype before = i - 1; before >= 0 && after < 0; --before)
                    after = all.indexOf(platforms.at(before).id);
                const QString target = all.value(after + 1);
                if (!target.isEmpty() && target != p.id) {
                    r = TomlPatch::moveBlock(r.text, {QStringLiteral("platform"), p.id}, target);
                    if (!r.ok)
                        return r;
                }
                current = r.text;
                ids = TomlPatch::blockValues(current, QStringLiteral("platform"), QStringLiteral("id"));
                placed = true;
                log.append(p.name.isEmpty() ? p.id : p.name);
            }
        }
        for (const QString &emulator : p.emulators) {
            const QString pair = p.id + u'/' + emulator;
            if (offered.contains(pair))
                continue;
            considered.append(pair);
            if (placed || !ids.contains(p.id))
                continue;
            QStringList list = emulatorsOf(current, p.id);
            if (list.contains(emulator))
                continue;
            // Where the defaults have it: before the first default listed after it, so a new first
            // choice (Eden on Switch) also comes first in an existing library.
            const qsizetype rank = p.emulators.indexOf(emulator);
            qsizetype at = list.size();
            for (qsizetype j = 0; j < list.size(); ++j) {
                if (p.emulators.indexOf(list.at(j)) > rank) {
                    at = j;
                    break;
                }
            }
            list.insert(at, emulator);
            QStringList quotedItems;
            for (const QString &item : list)
                quotedItems.append(tomlQuote(item));
            const TomlPatch::Result r = TomlPatch::setBlockValue(
                current, {QStringLiteral("platform"), p.id}, QStringLiteral("emulators"),
                u'[' + quotedItems.join(QStringLiteral(", ")) + u']');
            if (!r.ok)
                return r;
            current = r.text;
            log.append(pair);
        }
        // Counts as looked at from here on, so a default is never offered twice in one run.
        offered += considered;
        considered.clear();
    }

    if (offeredNode && offered == previous)
        return {true, current, {}};
    TomlPatch::Result r = TomlPatch::setValue(current, QString(), QStringLiteral("offered"), offeredValue(offered));
    if (!r.ok)
        return r;
    if (!offeredNode) {
        const qsizetype at = r.text.indexOf(QStringLiteral("offered = "));
        if (at >= 0)
            r.text.insert(at, kComment);
    }
    if (added)
        *added = log;
    return r;
}
} // namespace OfferedDefaults
