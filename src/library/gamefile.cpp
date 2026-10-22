#include "gamefile.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <QTimeZone>

#include <toml++/toml.hpp>

#include <cmath>

namespace GameFields {
namespace {
using enum Type;

constexpr Field kFields[] = {
    {"title", String, 0, 0, "", "The name shown in the list. Required, unless a source index (steam.toml) has it"},
    {"platform", String, 0, 0, "", "A platform id from platforms.toml. Required, unless a source index has it"},
    {"rom", String, 0, 0, "", "The ROM of a game added from outside the ROM folders: an absolute path, ~/…, or relative to the data dir"},
    {"description", String, 0, 0, "", "A paragraph or two about the game"},
    {"year", Integer, 1, 9999, "", "Release year"},
    {"rating", Float, 0, 5, "", "Your rating, 0 to 5"},
    {"rating_source", Choice, 0, 0, "", "Set to igdb when the rating came from an IGDB lookup; setting the rating yourself removes it", "igdb"},
    {"tags", StringList, 0, 0, "[]", "Tag names; colors come from tags.toml"},
    {"favorite", Bool, 0, 0, "false", "Starred"},
    {"status", Choice, 0, 0, "", "How far you are: backlog, playing, finished (the story), completed (100%) or dropped",
     "not_started playing finished completed dropped"},
    {"logo_scale", Float, 0.2, 4, "1", "The logo's size on the game's page, times the theme's (hero.logo_max_width and logo_max_height)"},
    {"logo_position", Choice, 0, 0, "", "Where the logo goes on the game's page; the theme's hero.logo_position when not set",
     "bottom_left bottom_center bottom_right center_left center center_right top_left top_center top_right"},
    {"logo_offset_x", Float, -10, 10, "0", "How far the logo is moved right from its logo_position, as a fraction of the hero's height; negative moves it left"},
    {"logo_offset_y", Float, -10, 10, "0", "How far the logo is moved down from its logo_position, as a fraction of the hero's height; negative moves it up"},
    {"logo_scrim", Choice, 0, 0, "", "What darkens the art behind the logo on the game's page: a gradient, a shadow that follows the logo, or none; the theme's hero.scrim and hero.scrim_style when not set",
     "gradient shadow none"},
    {"logo_scrim_side", Choice, 0, 0, "", "Where a gradient goes (auto = on the logo's top or bottom, or everywhere for a centered logo; full = an even dim over the whole page); the theme's hero.scrim_side when not set",
     "auto top bottom left right full"},
    {"logo_scrim_strength", Float, 0, 2, "1", "How strong the gradient or shadow is, times the theme's color; 0 shows nothing"},
    {"igdb", Integer, 0, 2147483647, "", "The game's IGDB id, set when a lookup matches it; 0 = not on IGDB, so bulk lookups skip it"},
    {"steamgriddb", Integer, 0, 2147483647, "", "The game's SteamGridDB id, set when its art is looked up; 0 = not on SteamGridDB"},
    {"play_minutes", Integer, 0, 2147483647, "0", "Total play time in minutes"},
    {"last_played", DateTime, 0, 0, "", "When the last session ended, with its UTC offset"},
    {"added", Date, 0, 0, "", "When the game was added to the library"},
    {"hidden", Bool, 0, 0, "false", "Hide the game from the list; its source won't bring it back"},
    {"emulator", String, 0, 0, "", "Pin the game to an emulator id; launching falls back to the platform's emulators if it isn't installed"},
    {"args", StringList, 0, 0, "[]", "Extra emulator arguments for this game, after the emulator's args"},
    {"launch_options", String, 0, 0, "", "Steam, Proton and Linux games: the launch options, as in Steam's Properties (`mangohud %command%`); a Steam or Proton game's are copied from Steam when they change there"},
    {"proton", String, 0, 0, "", "Steam games: the compatibility tool's internal name (`proton_experimental`, `GE-Proton10-4`); empty = Steam's default"},
    {"skip_steam_art", StringList, 0, 0, "[]", "Steam and Proton games: kinds of art (hero, logo, icon, cover) not to copy from Steam; set when you remove one"},
    {"exe", String, 0, 0, "", "Proton and Linux games: the program to run (`~` for the home folder); a Proton game's is kept in step with the shortcut's target"},
    {"start_dir", String, 0, 0, "", "Proton and Linux games: the folder it starts in (Steam's Start in); the exe's folder when not set"},
    {"install_dir", String, 0, 0, "", "Proton and Linux games: the game's folder, which Open folder opens and Uninstall can delete"},
    {"save_folder", String, 0, 0, "", "The folder Open save folder opens (`~` for the home folder); when not set, or not found, it's looked for: the ROM's folder for an emulator that saves next to it, the emulator's save folder, or the Proton prefix's user folder"},
    {"shortcut", Integer, 1, 4294967295.0, "", "Proton games: the Steam shortcut's app ID; it names the prefix and the grid art. Don't change it by hand"},
    {"installed", Bool, 0, 0, "true", "Proton and Linux games: false after Uninstall removed its files (or a Proton game's shortcut) and kept it in the library, until it's located again"},
};

QDate toQDate(const toml::date &d)
{
    return QDate(d.year, d.month, d.day);
}

QTime toQTime(const toml::time &t)
{
    return QTime(t.hour, t.minute, t.second, int(t.nanosecond / 1'000'000));
}

QVariant fromNode(const toml::node &node, Type type)
{
    switch (type) {
    case String:
    case Choice:
        if (const auto *s = node.as_string())
            return toQString(s->get());
        break;
    case Integer:
        if (const auto *i = node.as_integer())
            return qint64(i->get());
        break;
    case Float:
        if (const auto *f = node.as_floating_point())
            return f->get();
        if (const auto *i = node.as_integer())
            return double(i->get());
        break;
    case Bool:
        if (const auto *b = node.as_boolean())
            return b->get();
        break;
    case StringList:
        if (const auto *array = node.as_array()) {
            QStringList list;
            for (const toml::node &item : *array) {
                const auto *s = item.as_string();
                if (!s)
                    return {};
                list.append(toQString(s->get()));
            }
            return list;
        }
        break;
    case DateTime:
        if (const auto *dt = node.as_date_time()) {
            const toml::date_time &v = dt->get();
            // Without an offset, a hand-written time is this machine's local time.
            const QTimeZone zone = v.offset ? QTimeZone::fromSecondsAheadOfUtc(v.offset->minutes * 60)
                                            : QTimeZone(QTimeZone::LocalTime);
            return QDateTime(toQDate(v.date), toQTime(v.time), zone);
        }
        if (const auto *d = node.as_date())
            return toQDate(d->get()).startOfDay();
        break;
    case Date:
        if (const auto *d = node.as_date())
            return toQDate(d->get());
        break;
    }
    return {};
}

QString expected(const Field &field)
{
    switch (field.type) {
    case String: return QStringLiteral("text in quotes");
    case Integer:
        if (field.max == 2147483647)
            return QStringLiteral("a whole number, %1 or more").arg(qint64(field.min));
        return QStringLiteral("a whole number from %1 to %2").arg(qint64(field.min)).arg(qint64(field.max));
    case Float: return QStringLiteral("a number from %1 to %2").arg(field.min).arg(field.max);
    case Bool: return QStringLiteral("true or false");
    case StringList: return QStringLiteral("a list of names in quotes, like [\"RPG\", \"Co-op\"]");
    case DateTime: return QStringLiteral("a date and time, like 2026-09-19T21:04:00-03:00");
    case Date: return QStringLiteral("a date, like 2026-01-12");
    case Choice: {
        QStringList quotedChoices;
        for (const QString &c : choices(field))
            quotedChoices.append(u'"' + c + u'"');
        return QStringLiteral("one of %1").arg(quotedChoices.join(QLatin1String(", ")));
    }
    }
    return {};
}

bool isIntegral(const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    case QMetaType::Double: {
        const double d = value.toDouble();
        return std::isfinite(d) && d == std::floor(d);
    }
    default:
        return false;
    }
}

QString quoted(const QString &s)
{
    QString out;
    out.reserve(s.size() + 2);
    out += u'"';
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '"': out += QLatin1String("\\\""); break;
        case '\\': out += QLatin1String("\\\\"); break;
        case '\n': out += QLatin1String("\\n"); break;
        case '\t': out += QLatin1String("\\t"); break;
        case '\r': out += QLatin1String("\\r"); break;
        default:
            if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                out += QStringLiteral("\\u%1").arg(uint(c.unicode()), 4, 16, u'0');
            else
                out += c;
        }
    }
    out += u'"';
    return out;
}
}

std::span<const Field> all()
{
    return kFields;
}

const Field *find(const QString &key)
{
    for (const Field &f : kFields) {
        if (key == QLatin1String(f.key))
            return &f;
    }
    return nullptr;
}

QStringList keys()
{
    QStringList out;
    for (const Field &f : kFields)
        out.append(QLatin1String(f.key));
    return out;
}

QString typeName(Type type)
{
    switch (type) {
    case String: return QStringLiteral("string");
    case Integer: return QStringLiteral("integer");
    case Float: return QStringLiteral("float");
    case Bool: return QStringLiteral("bool");
    case StringList: return QStringLiteral("list of strings");
    case DateTime: return QStringLiteral("date-time");
    case Date: return QStringLiteral("date");
    case Choice: return QStringLiteral("string");
    }
    return {};
}

QStringList choices(const Field &field)
{
    return field.choices ? QString::fromLatin1(field.choices).split(u' ') : QStringList();
}

QVariant validate(const QString &key, const QVariant &value, QString *error)
{
    const Field *field = find(key);
    if (!field) {
        *error = QStringLiteral("unknown key %1").arg(key);
        return {};
    }
    const auto wrong = [&] {
        *error = QStringLiteral("%1 must be %2").arg(key, expected(*field));
        return QVariant();
    };

    switch (field->type) {
    case String:
        return value.typeId() == QMetaType::QString ? value : wrong();
    case Integer: {
        if (!isIntegral(value))
            return wrong();
        const qint64 n = value.toLongLong();
        if (n < field->min || n > field->max)
            return wrong();
        return int(n) == n ? QVariant(int(n)) : QVariant(n);
    }
    case Float: {
        if (!isIntegral(value) && value.typeId() != QMetaType::Double && value.typeId() != QMetaType::Float)
            return wrong();
        const double d = value.toDouble();
        if (!std::isfinite(d) || d < field->min || d > field->max)
            return wrong();
        return d;
    }
    case Bool:
        return value.typeId() == QMetaType::Bool ? value : wrong();
    case StringList: {
        if (value.typeId() == QMetaType::QStringList)
            return value;
        if (value.typeId() != QMetaType::QVariantList)
            return wrong();
        QStringList list;
        for (const QVariant &item : value.toList()) {
            if (item.typeId() != QMetaType::QString)
                return wrong();
            list.append(item.toString());
        }
        return list;
    }
    case DateTime:
        return value.typeId() == QMetaType::QDateTime && value.toDateTime().isValid() ? value : wrong();
    case Date:
        return value.typeId() == QMetaType::QDate && value.toDate().isValid() ? value : wrong();
    case Choice:
        return value.typeId() == QMetaType::QString && choices(*field).contains(value.toString()) ? value : wrong();
    }
    return wrong();
}

bool isCleared(const QString &key, const QVariant &value)
{
    const Field *field = find(key);
    if (!field)
        return false;
    if (!value.isValid())
        return true;
    switch (field->type) {
    case String:
    case Choice:
        return value.typeId() == QMetaType::QString && value.toString().isEmpty();
    case StringList:
        return (value.typeId() == QMetaType::QStringList || value.typeId() == QMetaType::QVariantList)
            && value.toList().isEmpty();
    case Bool:
        return value.typeId() == QMetaType::Bool && value.toBool() == (qstrcmp(field->defaultText, "true") == 0);
    default:
        return false;
    }
}

void apply(GameFile &game, const QString &key, const QVariant &value)
{
    if (key == QLatin1String("title"))
        game.title = value.toString();
    else if (key == QLatin1String("platform"))
        game.platform = value.toString();
    else if (key == QLatin1String("rom"))
        game.rom = value.toString();
    else if (key == QLatin1String("description"))
        game.description = value.toString();
    else if (key == QLatin1String("year"))
        game.year = value.toInt();
    else if (key == QLatin1String("rating"))
        game.rating = value.toDouble();
    else if (key == QLatin1String("rating_source"))
        game.ratingSource = value.toString();
    else if (key == QLatin1String("tags"))
        game.tags = value.toStringList();
    else if (key == QLatin1String("favorite"))
        game.favorite = value.toBool();
    else if (key == QLatin1String("status"))
        game.status = value.toString();
    else if (key == QLatin1String("logo_scale"))
        game.logoScale = value.toDouble();
    else if (key == QLatin1String("logo_position"))
        game.logoPosition = value.toString();
    else if (key == QLatin1String("logo_scrim"))
        game.logoScrim = value.toString();
    else if (key == QLatin1String("logo_scrim_side"))
        game.logoScrimSide = value.toString();
    else if (key == QLatin1String("logo_scrim_strength"))
        game.logoScrimStrength = value.toDouble();
    else if (key == QLatin1String("logo_offset_x"))
        game.logoOffsetX = value.toDouble();
    else if (key == QLatin1String("logo_offset_y"))
        game.logoOffsetY = value.toDouble();
    else if (key == QLatin1String("igdb"))
        game.igdb = value.toInt();
    else if (key == QLatin1String("steamgriddb"))
        game.steamgriddb = value.toInt();
    else if (key == QLatin1String("play_minutes"))
        game.playMinutes = value.toInt();
    else if (key == QLatin1String("last_played"))
        game.lastPlayed = value.toDateTime();
    else if (key == QLatin1String("added"))
        game.added = value.toDate();
    else if (key == QLatin1String("hidden"))
        game.hidden = value.toBool();
    else if (key == QLatin1String("emulator"))
        game.emulator = value.toString();
    else if (key == QLatin1String("args"))
        game.args = value.toStringList();
    else if (key == QLatin1String("launch_options"))
        game.launchOptions = value.toString();
    else if (key == QLatin1String("proton"))
        game.proton = value.toString();
    else if (key == QLatin1String("skip_steam_art"))
        game.skipSteamArt = value.toStringList();
    else if (key == QLatin1String("exe"))
        game.exe = value.toString();
    else if (key == QLatin1String("start_dir"))
        game.startDir = value.toString();
    else if (key == QLatin1String("install_dir"))
        game.installDir = value.toString();
    else if (key == QLatin1String("save_folder"))
        game.saveFolder = value.toString();
    else if (key == QLatin1String("shortcut"))
        game.shortcut = value.toLongLong();
    else if (key == QLatin1String("installed"))
        game.installed = value.toBool();
}

QString format(const QString &key, const QVariant &value)
{
    const Field *field = find(key);
    if (!field)
        return {};
    switch (field->type) {
    case String:
    case Choice:
        return quoted(value.toString());
    case Integer:
        return QString::number(value.toLongLong());
    case Float: {
        QString text = QString::number(value.toDouble(), 'g', 15);
        if (!text.contains(u'.') && !text.contains(u'e'))
            text += QLatin1String(".0");
        return text;
    }
    case Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case StringList: {
        QStringList items;
        for (const QString &s : value.toStringList())
            items.append(quoted(s));
        return u'[' + items.join(QLatin1String(", ")) + u']';
    }
    case DateTime: {
        const QDateTime dt = value.toDateTime();
        return dt.toOffsetFromUtc(dt.offsetFromUtc()).toString(Qt::ISODate);
    }
    case Date:
        return value.toDate().toString(Qt::ISODate);
    }
    return {};
}
}

ParsedGameFile parseGameFile(const QByteArray &bytes, const QString &displayPath)
{
    ParsedGameFile out;
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(QStringLiteral("%1:%2: %3")
                                .arg(displayPath)
                                .arg(e.source().begin.line)
                                .arg(QString::fromUtf8(e.description().data(), qsizetype(e.description().size()))));
        return out;
    }
    out.ok = true;

    for (auto &&[k, node] : table) {
        const QString key = toQString(k.str());
        const QString where = QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
        const GameFields::Field *field = GameFields::find(key);
        if (!field) {
            const QString what = node.is_table() && !node.as_table()->is_inline()
                ? QStringLiteral("unknown section [%1]; game files have no sections").arg(key)
                : QStringLiteral("unknown key %1").arg(key);
            out.warnings.append(QStringLiteral("%1: %2%3").arg(where, what, suggestion(key, GameFields::keys())));
            continue;
        }

        // logo_scrim was a Bool before it became a choice.
        if (key == QLatin1String("logo_scrim") && node.is_boolean()) {
            if (!node.as_boolean()->get())
                out.game.logoScrim = QStringLiteral("none");
            continue;
        }

        QString error;
        QVariant value = GameFields::fromNode(node, field->type);
        if (value.isValid())
            value = GameFields::validate(key, value, &error);
        else
            error = QStringLiteral("%1 must be %2").arg(key, GameFields::expected(*field));
        if (!value.isValid()) {
            out.warnings.append(QStringLiteral("%1: %2").arg(where, error));
            continue;
        }
        GameFields::apply(out.game, key, value);
        if (key == QLatin1String("rom"))
            out.game.romLine = int(node.source().begin.line);
    }
    return out;
}

QString tomlString(const QString &text)
{
    return GameFields::quoted(text);
}
