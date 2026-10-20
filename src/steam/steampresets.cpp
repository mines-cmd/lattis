#include "steampresets.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <QRegularExpression>

#include <algorithm>

#include <toml++/toml.hpp>

namespace {
const QString kCommand = QStringLiteral("%command%");

using LaunchOptions::isAssignment;
using LaunchOptions::split;
using LaunchOptions::Word;

bool isSpace(QChar c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
}

QString quoteWord(const QString &word)
{
    static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_\\-+=.,/:%@^]+$"));
    if (safe.match(word).hasMatch())
        return word;
    QString escaped = word;
    escaped.replace(u'\\', QStringLiteral("\\\\"))
        .replace(u'"', QStringLiteral("\\\""))
        .replace(u'$', QStringLiteral("\\$"))
        .replace(u'`', QStringLiteral("\\`"));
    return u'"' + escaped + u'"';
}

QString joinWords(const QStringList &words)
{
    QStringList quoted;
    for (const QString &w : words)
        quoted.append(quoteWord(w));
    return quoted.join(u' ');
}

struct Layout
{
    QList<Word> words;
    qsizetype command = -1;
    qsizetype envEnd = 0;   // the leading assignments are words[0, envEnd)

    explicit Layout(const QString &options)
        : words(split(options))
    {
        for (qsizetype i = 0; i < words.size(); ++i) {
            if (words.at(i).text == kCommand) {
                command = i;
                break;
            }
        }
        if (command >= 0) {
            while (envEnd < command && isAssignment(words.at(envEnd).text))
                ++envEnd;
        }
    }

    qsizetype find(const QStringList &sequence, qsizetype from, qsizetype to) const
    {
        for (qsizetype i = from; i + sequence.size() <= to; ++i) {
            bool match = true;
            for (qsizetype j = 0; j < sequence.size() && match; ++j)
                match = words.at(i + j).text == sequence.at(j);
            if (match)
                return i;
        }
        return -1;
    }

    qsizetype wrapperAt(const SteamPreset &preset) const
    {
        return command < 0 ? -1 : find(preset.wrapper, envEnd, command);
    }

    qsizetype argsAt(const SteamPreset &preset) const
    {
        return find(preset.args, command + 1, words.size());
    }

    qsizetype envAt(const QString &key, const QString &value) const
    {
        const QString assignment = key + u'=' + value;
        for (qsizetype i = 0; i < envEnd; ++i) {
            if (words.at(i).text == assignment)
                return i;
        }
        return -1;
    }
};

struct Edit
{
    qsizetype begin;
    qsizetype end;
    QString text;
    int order; // among edits at the same place: the lower one ends up first
};

QString apply(QString s, QList<Edit> edits)
{
    std::sort(edits.begin(), edits.end(), [](const Edit &a, const Edit &b) {
        return a.begin != b.begin ? a.begin > b.begin : a.order > b.order;
    });
    for (const Edit &e : edits)
        s.replace(e.begin, e.end - e.begin, e.text);
    return s;
}

void removeWords(QString &s, qsizetype begin, qsizetype end, bool spaceBefore)
{
    if (spaceBefore && begin > 0 && isSpace(s.at(begin - 1))) {
        while (begin > 0 && isSpace(s.at(begin - 1)))
            --begin;
    } else if (end < s.size() && isSpace(s.at(end))) {
        while (end < s.size() && isSpace(s.at(end)))
            ++end;
    } else {
        while (begin > 0 && isSpace(s.at(begin - 1)))
            --begin;
    }
    s.remove(begin, end - begin);
}

bool isValidId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9][a-z0-9_-]*$"));
    return re.match(id).hasMatch();
}

bool isValidEnvName(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    return re.match(name).hasMatch();
}
}

QString SteamPreset::describe() const
{
    return LaunchOptions::turnOn({}, *this);
}

bool SteamPreset::protonOnly() const
{
    return !env.isEmpty() && wrapper.isEmpty() && args.isEmpty()
        && std::all_of(env.cbegin(), env.cend(), [](const auto &pair) { return pair.first.startsWith(QLatin1String("PROTON_")); });
}

const SteamPreset *SteamSettings::preset(const QString &id) const
{
    for (const SteamPreset &p : presets) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

QStringList SteamSettings::presetIds() const
{
    QStringList ids;
    for (const SteamPreset &p : presets)
        ids.append(p.id);
    return ids;
}

QList<SteamPreset> builtInSteamPresets()
{
    SteamPreset mangohud;
    mangohud.id = QStringLiteral("mangohud");
    mangohud.name = QStringLiteral("MangoHud");
    mangohud.note = QStringLiteral("Shows frame rate, frame times and temperatures. Needs MangoHud installed.");
    mangohud.wrapper = {QStringLiteral("mangohud")};
    mangohud.builtIn = true;

    SteamPreset gamemode;
    gamemode.id = QStringLiteral("gamemode");
    gamemode.name = QStringLiteral("GameMode");
    gamemode.note = QStringLiteral("Asks Feral GameMode to tune the system while the game runs. Needs GameMode installed.");
    gamemode.wrapper = {QStringLiteral("gamemoderun")};
    gamemode.builtIn = true;

    // Valve's Proton (Experimental, as of Proton 10) has no Wayland driver and ignores it.
    SteamPreset wayland;
    wayland.id = QStringLiteral("wayland");
    wayland.name = QStringLiteral("Wayland");
    wayland.note = QStringLiteral("Runs the game as a Wayland window instead of through Xwayland. Needs GE-Proton.");
    wayland.env = {{QStringLiteral("PROTON_ENABLE_WAYLAND"), QStringLiteral("1")}};
    wayland.builtIn = true;

    return {mangohud, gamemode, wayland};
}

Parsed<SteamSettings> parseSteamSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "steam");
    Parsed<SteamSettings> out{section.ok, {}, section.warnings};
    out.value.presets = builtInSteamPresets();
    if (!section.table)
        return out;
    const toml::table *table = section.table;

    const auto wordList = [&](const toml::node &node, const QString &name, QStringList *words) {
        if (const auto *s = node.as_string()) {
            *words = LaunchOptions::words(toQString(s->get()));
            return true;
        }
        const auto *array = node.as_array();
        if (!array || !array->is_homogeneous(toml::node_type::string) || array->empty()) {
            out.warnings.append(QStringLiteral("%1: %2 must be a list of words, like [\"gamescope\", \"-f\", \"--\"]")
                                    .arg(section.where(node), name));
            return false;
        }
        words->clear();
        for (const toml::node &w : *array)
            words->append(toQString(w.as_string()->get()));
        return true;
    };

    // The presets first: default_presets refers to them.
    if (const toml::node *node = table->get("presets")) {
        const toml::table *presets = node->as_table();
        if (!presets)
            out.warnings.append(QStringLiteral("%1: presets must be [steam.presets.<id>] sections").arg(section.where(*node)));
        static const toml::table kNone;
        for (auto &&[k, presetNode] : presets ? *presets : kNone) {
            const QString id = toQString(k.str());
            const toml::table *def = presetNode.as_table();
            if (!def) {
                out.warnings.append(QStringLiteral("%1: preset %2 must be a [steam.presets.%2] section")
                                        .arg(section.where(presetNode), id));
                continue;
            }
            if (!isValidId(id)) {
                out.warnings.append(QStringLiteral("%1: preset id %2 must be lowercase letters, digits, - and _")
                                        .arg(section.where(presetNode), id));
                continue;
            }
            SteamPreset *existing = nullptr;
            for (SteamPreset &p : out.value.presets) {
                if (p.id == id)
                    existing = &p;
            }
            SteamPreset preset = existing ? *existing : SteamPreset();
            preset.id = id;
            const QStringList known = {QStringLiteral("name"), QStringLiteral("env"), QStringLiteral("wrapper"),
                                       QStringLiteral("args")};
            for (auto &&[fk, value] : *def) {
                const QString key = toQString(fk.str());
                if (key == u"name") {
                    const auto *s = value.as_string();
                    if (s && !s->get().empty())
                        preset.name = toQString(s->get());
                    else
                        out.warnings.append(QStringLiteral("%1: name must be text").arg(section.where(value)));
                } else if (key == u"env") {
                    const toml::table *env = value.as_table();
                    if (!env) {
                        out.warnings.append(
                            QStringLiteral("%1: env must be a table of variables, like { DXVK_HDR = \"1\" }")
                                .arg(section.where(value)));
                        continue;
                    }
                    preset.env.clear();
                    for (auto &&[ek, ev] : *env) {
                        const QString name = toQString(ek.str());
                        const auto *s = ev.as_string();
                        if (!isValidEnvName(name))
                            out.warnings.append(QStringLiteral("%1: %2 isn't a variable name").arg(section.where(ev), name));
                        else if (!s)
                            out.warnings.append(QStringLiteral("%1: %2 must be text, like \"1\"").arg(section.where(ev), name));
                        else
                            preset.env.append({name, toQString(s->get())});
                    }
                } else if (key == u"wrapper") {
                    wordList(value, key, &preset.wrapper);
                } else if (key == u"args") {
                    wordList(value, key, &preset.args);
                } else {
                    out.warnings.append(
                        QStringLiteral("%1: unknown key %2%3").arg(section.where(value), key, suggestion(key, known)));
                }
            }
            if (preset.isEmpty()) {
                out.warnings.append(QStringLiteral("%1: preset %2 adds nothing: give it env, wrapper or args")
                                        .arg(section.where(presetNode), id));
                continue;
            }
            if (preset.name.isEmpty())
                preset.name = id;
            if (existing)
                *existing = preset;
            else
                out.value.presets.append(preset);
        }
    }

    const QStringList known = {QStringLiteral("enabled"), QStringLiteral("path"), QStringLiteral("user"),
                               QStringLiteral("default_presets"), QStringLiteral("default_proton"),
                               QStringLiteral("presets")};
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        if (key == u"presets") {
            continue;
        } else if (key == u"enabled") {
            if (const auto *b = node.as_boolean())
                out.value.enabled = b->get();
            else
                out.warnings.append(QStringLiteral("%1: enabled must be true or false").arg(section.where(node)));
        } else if (key == u"path") {
            if (const auto *s = node.as_string())
                out.value.path = toQString(s->get());
            else
                out.warnings.append(QStringLiteral("%1: path must be a folder, like \"~/.local/share/Steam\"")
                                        .arg(section.where(node)));
        } else if (key == u"user") {
            if (const auto *i = node.as_integer(); i && i->get() > 0)
                out.value.user = QString::number(i->get());
            else if (const auto *s = node.as_string(); s && !s->get().empty())
                out.value.user = toQString(s->get());
            else
                out.warnings.append(QStringLiteral("%1: user must be an account id, the number of a folder in "
                                                   "Steam's userdata")
                                        .arg(section.where(node)));
        } else if (key == u"default_proton") {
            if (const auto *s = node.as_string())
                out.value.defaultProton = toQString(s->get());
            else
                out.warnings.append(QStringLiteral("%1: default_proton must be a compatibility tool's internal name, "
                                                   "like \"proton_experimental\"")
                                        .arg(section.where(node)));
        } else if (key == u"default_presets") {
            const auto *array = node.as_array();
            if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
                out.warnings.append(QStringLiteral("%1: default_presets must be a list of preset ids, like "
                                                   "[\"mangohud\"]")
                                        .arg(section.where(node)));
                continue;
            }
            const QStringList ids = out.value.presetIds();
            for (const toml::node &item : *array) {
                const QString id = toQString(item.as_string()->get());
                if (!ids.contains(id))
                    out.warnings.append(
                        QStringLiteral("%1: unknown preset %2%3").arg(section.where(item), id, suggestion(id, ids)));
                else if (!out.value.defaultPresets.contains(id))
                    out.value.defaultPresets.append(id);
            }
        } else {
            out.warnings.append(QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        }
    }
    return out;
}

namespace LaunchOptions {
bool isOn(const QString &options, const SteamPreset &preset)
{
    if (preset.isEmpty())
        return false;
    const Layout layout(options);
    if ((!preset.env.isEmpty() || !preset.wrapper.isEmpty()) && layout.command < 0)
        return false;
    for (const auto &[key, value] : preset.env) {
        if (layout.envAt(key, value) < 0)
            return false;
    }
    if (!preset.wrapper.isEmpty() && layout.wrapperAt(preset) < 0)
        return false;
    return preset.args.isEmpty() || layout.argsAt(preset) >= 0;
}

QString turnOn(const QString &options, const SteamPreset &preset)
{
    if (preset.isEmpty() || isOn(options, preset))
        return options;
    QString s = options;
    if (Layout(s).command < 0)
        s = s.trimmed().isEmpty() ? kCommand : kCommand + u' ' + s;

    const Layout layout(s);
    const Word &command = layout.words.at(layout.command);
    QList<Edit> edits;
    QStringList added;
    for (const auto &[key, value] : preset.env) {
        if (layout.envAt(key, value) >= 0)
            continue;
        const QString assignment = key + u'=' + quoteWord(value);
        bool replaced = false;
        for (qsizetype i = 0; i < layout.envEnd && !replaced; ++i) {
            const Word &w = layout.words.at(i);
            if (w.text.startsWith(key + u'=')) {
                edits.append({w.begin, w.end, assignment, 0});
                replaced = true;
            }
        }
        if (!replaced)
            added.append(assignment);
    }
    if (!added.isEmpty())
        edits.append({0, 0, added.join(u' ') + u' ', 0});
    if (!preset.wrapper.isEmpty() && layout.wrapperAt(preset) < 0)
        edits.append({command.begin, command.begin, joinWords(preset.wrapper) + u' ', 1});
    if (!preset.args.isEmpty() && layout.argsAt(preset) < 0)
        edits.append({command.end, command.end, u' ' + joinWords(preset.args), 2});
    return apply(s, edits);
}

QString turnOff(const QString &options, const SteamPreset &preset)
{
    const Layout layout(options);
    struct Range
    {
        qsizetype begin;
        qsizetype end;
        bool spaceBefore;
    };
    QList<Range> ranges;
    for (const auto &[key, value] : preset.env) {
        const qsizetype i = layout.envAt(key, value);
        if (i >= 0)
            ranges.append({layout.words.at(i).begin, layout.words.at(i).end, false});
    }
    const auto addSequence = [&](qsizetype at, qsizetype size, bool spaceBefore) {
        if (at >= 0 && size > 0)
            ranges.append({layout.words.at(at).begin, layout.words.at(at + size - 1).end, spaceBefore});
    };
    if (!preset.wrapper.isEmpty())
        addSequence(layout.wrapperAt(preset), preset.wrapper.size(), false);
    if (!preset.args.isEmpty())
        addSequence(layout.argsAt(preset), preset.args.size(), true);
    if (ranges.isEmpty())
        return options;

    std::sort(ranges.begin(), ranges.end(), [](const Range &a, const Range &b) { return a.begin > b.begin; });
    QString s = options;
    for (const Range &r : ranges)
        removeWords(s, r.begin, r.end, r.spaceBefore);

    const Layout left(s);
    if (left.command == 0)
        removeWords(s, left.words.at(0).begin, left.words.at(0).end, false);
    return s;
}
}
