#include "modmanagers.h"
#include "gamefile.h"
#include "libraryfiles.h"
#include "suggestion.h"
#include "tomlsection.h"

#include <toml++/toml.hpp>

namespace ModManagers {
namespace {
struct Key
{
    const char *name;
    bool list;
};

constexpr Key kKeys[] = {
    {"name", false}, {"exe", false},    {"start_dir", false}, {"launch_options", false},
    {"games", true}, {"prefix", false}, {"proton", false},    {"shortcut", false},
    {"in_steam", false},
};

const Key *find(const QString &key)
{
    for (const Key &k : kKeys) {
        if (key == QLatin1String(k.name))
            return &k;
    }
    return nullptr;
}

QString quoted(const QString &text)
{
    return tomlString(text);
}
}

QStringList keys()
{
    QStringList out;
    for (const Key &k : kKeys)
        out.append(QLatin1String(k.name));
    return out;
}

QVariant validate(const QString &key, const QVariant &value, QString *error)
{
    const Key *k = find(key);
    if (!k) {
        *error = QStringLiteral("unknown key %1%2").arg(key, suggestion(key, keys()));
        return {};
    }
    if (key == QLatin1String("in_steam")) {
        if (value.typeId() != QMetaType::Bool) {
            *error = QStringLiteral("in_steam must be true or false");
            return {};
        }
        return value.toBool();
    }
    if (key == QLatin1String("shortcut")) {
        bool ok = false;
        const qlonglong n = value.toLongLong(&ok);
        if (!ok || n < 1 || n > 4294967295LL) {
            *error = QStringLiteral("shortcut must be a whole number from 1 to 4294967295");
            return {};
        }
        return n;
    }
    if (k->list) {
        if (value.typeId() != QMetaType::QStringList) {
            *error = QStringLiteral("%1 must be a list of names in quotes").arg(key);
            return {};
        }
        return value.toStringList();
    }
    if (value.typeId() != QMetaType::QString) {
        *error = QStringLiteral("%1 must be text in quotes").arg(key);
        return {};
    }
    const QString text = value.toString();
    if (key == QLatin1String("prefix") && text != QLatin1String("game") && text != QLatin1String("own")) {
        *error = QStringLiteral("prefix must be one of \"game\", \"own\"");
        return {};
    }
    return text;
}

QString format(const QString &key, const QVariant &value)
{
    if (key == QLatin1String("shortcut"))
        return QString::number(value.toLongLong());
    if (key == QLatin1String("in_steam"))
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (find(key)->list) {
        QStringList items;
        for (const QString &item : value.toStringList())
            items.append(quoted(item));
        return QStringLiteral("[") + items.join(QLatin1String(", ")) + u']';
    }
    return quoted(value.toString());
}

void apply(ModManager &m, const QString &key, const QVariant &value)
{
    if (key == QLatin1String("name"))
        m.name = value.toString();
    else if (key == QLatin1String("exe"))
        m.exe = value.toString();
    else if (key == QLatin1String("start_dir"))
        m.startDir = value.toString();
    else if (key == QLatin1String("launch_options"))
        m.launchOptions = value.toString();
    else if (key == QLatin1String("games"))
        m.games = value.toStringList();
    else if (key == QLatin1String("prefix"))
        m.prefix = value.toString();
    else if (key == QLatin1String("proton"))
        m.proton = value.toString();
    else if (key == QLatin1String("shortcut"))
        m.shortcut = quint32(value.toLongLong());
    else if (key == QLatin1String("in_steam"))
        m.inSteam = value.toBool();
}

Parsed<ModManager> parse(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<ModManager> out;
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(QStringLiteral("%1:%2: %3")
                                .arg(displayPath)
                                .arg(e.source().begin.line)
                                .arg(toQString(e.description())));
        return out;
    }
    out.ok = true;

    for (auto &&[k, node] : table) {
        const QString key = toQString(k.str());
        const QString where = QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
        const Key *known = find(key);
        if (!known) {
            out.warnings.append(QStringLiteral("%1: unknown key %2%3").arg(where, key, suggestion(key, keys())));
            continue;
        }
        QVariant value;
        if (known->list) {
            if (const auto *array = node.as_array()) {
                QStringList list;
                bool allText = true;
                for (const toml::node &item : *array) {
                    const auto *s = item.as_string();
                    allText = allText && s;
                    if (s)
                        list.append(toQString(s->get()));
                }
                if (allText)
                    value = list;
            }
        } else if (key == QLatin1String("shortcut")) {
            if (const auto *i = node.as_integer())
                value = qlonglong(i->get());
        } else if (key == QLatin1String("in_steam")) {
            if (const auto *b = node.as_boolean())
                value = b->get();
        } else if (const auto *s = node.as_string()) {
            value = toQString(s->get());
        }
        QString error;
        if (value.isValid())
            value = validate(key, value, &error);
        else
            error = key == QLatin1String("shortcut")
                ? QStringLiteral("shortcut must be a whole number from 1 to 4294967295")
            : key == QLatin1String("in_steam") ? QStringLiteral("in_steam must be true or false")
                : known->list ? QStringLiteral("%1 must be a list of names in quotes, like [\"steam-489830\"]").arg(key)
                              : QStringLiteral("%1 must be text in quotes").arg(key);
        if (!value.isValid()) {
            out.warnings.append(QStringLiteral("%1: %2").arg(where, error));
            continue;
        }
        apply(out.value, key, value);
    }
    if (out.value.name.isEmpty())
        out.warnings.append(QStringLiteral("%1: name is missing; using the file name").arg(displayPath));
    return out;
}

QString formatNew(const ModManager &m)
{
    QString text = QStringLiteral(
        "# A mod manager. It's opened from the pages of the games in `games` and from Settings → Mod managers.\n");
    const auto add = [&](const char *key, const QVariant &value) {
        text += QLatin1String(key) + QStringLiteral(" = ") + format(QLatin1String(key), value) + u'\n';
    };
    add("name", m.name);
    add("exe", m.exe);
    if (!m.startDir.isEmpty())
        add("start_dir", m.startDir);
    if (!m.launchOptions.isEmpty())
        add("launch_options", m.launchOptions);
    add("games", m.games);
    if (m.isProton()) {
        if (!m.prefix.isEmpty())
            add("prefix", m.prefix);
        if (!m.proton.isEmpty())
            add("proton", m.proton);
        if (m.shortcut > 0)
            add("shortcut", qlonglong(m.shortcut));
        if (!m.inSteam)
            add("in_steam", false);
    }
    return text;
}

namespace {
const QString kCompatEnv = QStringLiteral("STEAM_COMPAT_DATA_PATH=");
const QString kCommand = QStringLiteral("%command%");
}

QString withPrefix(const QString &launchOptions, const QString &compatDataPath)
{
    if (compatDataPath.isEmpty())
        return launchOptions;
    QString escaped = compatDataPath;
    escaped.replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\""));
    const QString env = kCompatEnv + u'"' + escaped + u'"';
    const QString rest = launchOptions.trimmed();
    if (rest.contains(kCommand))
        return env + u' ' + rest;
    return rest.isEmpty() ? env + u' ' + kCommand : env + u' ' + kCommand + u' ' + rest;
}

QString withoutPrefix(const QString &launchOptions)
{
    QString text = launchOptions.trimmed();
    if (!text.startsWith(kCompatEnv))
        return launchOptions;
    qsizetype i = kCompatEnv.size();
    if (i < text.size() && text[i] == u'"') {
        for (++i; i < text.size() && text[i] != u'"'; ++i) {
            if (text[i] == u'\\')
                ++i;
        }
        ++i;
    } else {
        while (i < text.size() && !text[i].isSpace())
            ++i;
    }
    text = text.mid(std::min(i, text.size())).trimmed();
    if (text == kCommand)
        return {};
    if (text.startsWith(kCommand + u' '))
        return text.mid(kCommand.size() + 1).trimmed();
    return text;
}

bool linkable(const QString &platform)
{
    return platform == kSteamPlatform || runsProgram(platform);
}

QList<LinkedGame> linkedGames(const ModManager &manager, const std::function<const Game *(const QString &)> &find)
{
    QList<LinkedGame> out;
    for (const QString &id : manager.games) {
        const Game *game = find(id);
        LinkedGame linked{id, game ? game->title : id, true};
        linked.missing = !game || game->hidden || !game->installed || !linkable(game->platform);
        out.append(linked);
    }
    return out;
}
}
