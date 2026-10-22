#include "playhistory.h"

#include "tomlpatch.h"
#include "tomlsection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <climits>
#include <vector>

namespace PlayHistory {
namespace {
const QString kFolder = QStringLiteral("/library/history/");

QString where(const QString &displayPath, const toml::node &node)
{
    return QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
}

QDateTime toDateTime(const toml::date_time &v)
{
    // Without an offset, a hand-written time is this machine's local time.
    const QTimeZone zone = v.offset ? QTimeZone::fromSecondsAheadOfUtc(v.offset->minutes * 60)
                                    : QTimeZone(QTimeZone::LocalTime);
    return QDateTime(QDate(v.date.year, v.date.month, v.date.day),
                     QTime(v.time.hour, v.time.minute, v.time.second), zone);
}

QString formatted(const QDateTime &time)
{
    const QDateTime whole = time.addMSecs(-time.time().msec());
    return whole.toOffsetFromUtc(whole.offsetFromUtc()).toString(Qt::ISODate);
}

bool readSession(const toml::table &entry, const QString &displayPath, QStringList *warnings, Session *out)
{
    const toml::date_time *end = nullptr;
    if (const auto *node = entry.get(std::string_view("end")))
        end = node->is_date_time() ? &node->as_date_time()->get() : nullptr;
    const auto *minutes = entry.get_as<int64_t>(std::string_view("minutes"));
    if (!end || !minutes || minutes->get() < 0 || minutes->get() > INT_MAX) {
        warnings->append(QStringLiteral("%1: skipped; a session needs end = <date and time with its offset> and "
                                        "minutes = <a number>")
                             .arg(where(displayPath, entry)));
        return false;
    }
    out->end = toDateTime(*end);
    out->minutes = int(minutes->get());
    out->merged = false;
    for (auto &&[k, node] : entry) {
        const QString key = toQString(k.str());
        if (key == QLatin1String("end") || key == QLatin1String("minutes"))
            continue;
        if (key == QLatin1String("merged") && node.is_boolean())
            out->merged = node.as_boolean()->get();
        else
            warnings->append(QStringLiteral("%1: %2 is not a session key (or has the wrong type)")
                                 .arg(where(displayPath, node), key));
    }
    return true;
}

QByteArray sessionText(const Session &session)
{
    QByteArray text = "[[session]]\nend = " + formatted(session.end).toUtf8() + "\nminutes = "
        + QByteArray::number(session.minutes) + "\n";
    if (session.merged)
        text += "merged = true\n";
    return text;
}

QString writeText(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text) < 0 || !file.commit())
        return QStringLiteral("Could not save %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return {};
}

bool readText(const QString &path, QByteArray *text, QString *error)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        *text = file.readAll();
        return true;
    }
    if (!QFileInfo::exists(path)) {
        text->clear();
        return true;
    }
    *error = QStringLiteral("Could not read %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return false;
}

QString syntaxError(const QString &path, const QByteArray &text)
{
    const Parsed<File> parsed = parse(text, QFileInfo(path).fileName());
    if (parsed.ok)
        return {};
    return QStringLiteral("%1 has an error, so play history wasn't saved: %2")
        .arg(QFileInfo(path).fileName(), parsed.warnings.value(0));
}
}

QString path(const QString &dataDir, const QString &id)
{
    return dataDir + kFolder + id + QStringLiteral(".toml");
}

QString seenPath(const QString &dataDir)
{
    return dataDir + kFolder + QStringLiteral("steam-seen.toml");
}

Parsed<File> parse(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<File> out;
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(
            QStringLiteral("%1:%2: %3").arg(displayPath).arg(e.source().begin.line).arg(toQString(e.description())));
        return out;
    }
    out.ok = true;

    for (auto &&[k, node] : table) {
        const QString key = toQString(k.str());
        if (key == QLatin1String("deleted_minutes")) {
            const auto *n = node.as_integer();
            if (n && n->get() >= 0 && n->get() <= INT_MAX)
                out.value.deletedMinutes = int(n->get());
            else
                out.warnings.append(QStringLiteral("%1: deleted_minutes must be a whole number of minutes")
                                        .arg(where(displayPath, node)));
        } else if (key == QLatin1String("session")) {
            const toml::array *entries = node.as_array();
            if (!entries || !node.is_array_of_tables()) {
                out.warnings.append(QStringLiteral("%1: session must be [[session]] blocks").arg(where(displayPath, node)));
                continue;
            }
            for (const toml::node &entry : *entries) {
                Session session;
                if (entry.is_table() && readSession(*entry.as_table(), displayPath, &out.warnings, &session))
                    out.value.sessions.append(session);
            }
        } else {
            out.warnings.append(QStringLiteral("%1: unknown key %2").arg(where(displayPath, node), key));
        }
    }
    std::stable_sort(out.value.sessions.begin(), out.value.sessions.end(),
                     [](const Session &a, const Session &b) { return a.end < b.end; });
    return out;
}

Parsed<File> read(const QString &dataDir, const QString &id)
{
    const QString file = path(dataDir, id);
    QByteArray text;
    QString error;
    if (!readText(file, &text, &error)) {
        Parsed<File> out;
        out.warnings.append(error);
        return out;
    }
    if (text.isEmpty()) {
        Parsed<File> out;
        out.ok = true;
        return out;
    }
    return parse(text, QStringLiteral("library/history/%1.toml").arg(id));
}

QString append(const QString &dataDir, const QString &id, const Session &session)
{
    const QString file = path(dataDir, id);
    QByteArray text;
    QString error;
    if (!readText(file, &text, &error))
        return error;
    if (!text.trimmed().isEmpty()) {
        if (const QString problem = syntaxError(file, text); !problem.isEmpty())
            return problem;
        if (!text.endsWith('\n'))
            text += '\n';
        text += '\n';
    } else {
        text.clear();
    }
    return writeText(file, text + sessionText(session));
}

QString remove(const QString &dataDir, const QString &id, const QDateTime &end, Session *removed)
{
    const QString file = path(dataDir, id);
    QByteArray bytes;
    QString error;
    if (!readText(file, &bytes, &error))
        return error;
    if (const QString problem = syntaxError(file, bytes); !problem.isEmpty())
        return problem;

    const std::string src(bytes.constData(), size_t(bytes.size()));
    const toml::table table = toml::parse(src);
    const toml::array *sessions = table.get_as<toml::array>(std::string_view("session"));
    size_t line = 0;
    if (sessions) {
        for (const toml::node &entry : *sessions) {
            Session candidate;
            QStringList ignored;
            if (entry.is_table() && readSession(*entry.as_table(), {}, &ignored, &candidate)
                && candidate.end.toSecsSinceEpoch() == end.toSecsSinceEpoch()) {
                line = entry.source().begin.line;
                if (removed)
                    *removed = candidate;
                break;
            }
        }
    }
    if (line == 0)
        return QStringLiteral("There's no session that ended at %1").arg(formatted(end));

    std::vector<size_t> lineStarts{0};
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '\n')
            lineStarts.push_back(i + 1);
    }
    size_t next = SIZE_MAX;
    const auto consider = [&](const toml::node &node) {
        const size_t begin = node.source().begin.line;
        if (begin > line)
            next = std::min(next, begin);
    };
    for (auto &&[k, node] : table) {
        if (const toml::array *entries = node.as_array(); entries && node.is_array_of_tables()) {
            for (const toml::node &entry : *entries)
                consider(entry);
        } else if (const toml::table *t = node.as_table(); t && !t->is_inline()) {
            consider(node);
        }
    }
    const size_t from = lineStarts.at(line - 1);
    const size_t to = next == SIZE_MAX || next > lineStarts.size() ? src.size() : lineStarts.at(next - 1);
    std::string out = src;
    out.erase(from, to - from);
    if (to == src.size()) {
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        if (!out.empty())
            out += '\n';
    }

    if (QByteArray(out.data(), qsizetype(out.size())).trimmed().isEmpty()) {
        if (!QFile::remove(file))
            return QStringLiteral("Could not remove %1").arg(QFileInfo(file).fileName());
        return {};
    }
    return writeText(file, QByteArray(out.data(), qsizetype(out.size())));
}

QString setDeletedMinutes(const QString &dataDir, const QString &id, int minutes)
{
    const QString file = path(dataDir, id);
    QByteArray bytes;
    QString error;
    if (!readText(file, &bytes, &error))
        return error;
    const QString key = QStringLiteral("deleted_minutes");
    const TomlPatch::Result r = minutes > 0
        ? TomlPatch::setValue(QString::fromUtf8(bytes), {}, key, QString::number(minutes))
        : TomlPatch::removeValue(QString::fromUtf8(bytes), {}, key);
    if (!r.ok)
        return QStringLiteral("%1 has an error, so play history wasn't saved: %2")
            .arg(QFileInfo(file).fileName(), r.error);
    if (r.text.trimmed().isEmpty()) {
        if (QFileInfo::exists(file) && !QFile::remove(file))
            return QStringLiteral("Could not remove %1").arg(QFileInfo(file).fileName());
        return {};
    }
    return writeText(file, r.text.toUtf8());
}

QString removeFile(const QString &dataDir, const QString &id)
{
    const QString file = path(dataDir, id);
    if (QFileInfo::exists(file) && !QFile::remove(file))
        return QStringLiteral("Could not remove %1").arg(QFileInfo(file).fileName());
    return {};
}

Parsed<SeenTotals> readSeen(const QString &dataDir)
{
    Parsed<SeenTotals> out;
    const QString file = seenPath(dataDir);
    QByteArray bytes;
    QString error;
    if (!readText(file, &bytes, &error)) {
        out.warnings.append(error);
        return out;
    }
    const QString display = QStringLiteral("library/history/steam-seen.toml");
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(
            QStringLiteral("%1:%2: %3").arg(display).arg(e.source().begin.line).arg(toQString(e.description())));
        return out;
    }
    out.ok = true;
    for (auto &&[user, node] : table) {
        const toml::table *games = node.as_table();
        if (!games) {
            out.warnings.append(QStringLiteral("%1: %2 must be a [user] section").arg(where(display, node), toQString(user.str())));
            continue;
        }
        QMap<QString, int> &totals = out.value[toQString(user.str())];
        for (auto &&[id, total] : *games) {
            const auto *n = total.as_integer();
            if (n && n->get() >= 0 && n->get() <= INT_MAX)
                totals.insert(toQString(id.str()), int(n->get()));
            else
                out.warnings.append(QStringLiteral("%1: %2 must be a number of minutes").arg(where(display, total), toQString(id.str())));
        }
    }
    return out;
}

QString writeSeen(const QString &dataDir, const SeenTotals &seen)
{
    QByteArray text = "# The play time Lattis last saw in Steam, to tell what is new.\n";
    for (auto user = seen.cbegin(); user != seen.cend(); ++user) {
        text += "\n[\"" + user.key().toUtf8() + "\"]\n";
        for (auto game = user->cbegin(); game != user->cend(); ++game)
            text += "\"" + game.key().toUtf8() + "\" = " + QByteArray::number(game.value()) + "\n";
    }
    return writeText(seenPath(dataDir), text);
}

SteamDiff diffSteam(const QMap<QString, int> &seen, const QList<SteamPlayed> &current, bool merged)
{
    SteamDiff out;
    out.seen = seen;
    for (const SteamPlayed &game : current) {
        const int total = qMax(game.minutes, 0);
        const auto before = seen.constFind(game.id);
        if (before == seen.cend()) {
            out.seen.insert(game.id, total);
            out.changed = true;
            continue;
        }
        if (total == *before)
            continue;
        out.seen.insert(game.id, total);
        out.changed = true;
        if (total > *before && game.lastPlayed > 0)
            out.sessions.append({game.id, {QDateTime::fromSecsSinceEpoch(game.lastPlayed), total - *before, merged}});
    }
    return out;
}

QDate dayOf(const Session &session, const QTimeZone &zone)
{
    return (session.merged ? session.end : session.start()).toTimeZone(zone).date();
}

namespace {
QDate weekStart(const QDate &date, Qt::DayOfWeek firstDay)
{
    return date.addDays(-((date.dayOfWeek() - int(firstDay) + 7) % 7));
}

QVariantMap sessionMap(const Session &s)
{
    return {{QStringLiteral("end"), formatted(s.end)},
            {QStringLiteral("endMs"), s.end.toMSecsSinceEpoch()},
            {QStringLiteral("startMs"), s.start().toMSecsSinceEpoch()},
            {QStringLiteral("minutes"), s.minutes},
            {QStringLiteral("merged"), s.merged}};
}
}

QVariantMap summary(const File &file, int total, const QDate &today, Qt::DayOfWeek firstDay, const QTimeZone &zone)
{
    int sum = file.deletedMinutes;
    int week = 0;
    int month = 0;
    const QDate thisWeek = weekStart(today, firstDay);
    QVariantList sessions;
    for (auto it = file.sessions.crbegin(); it != file.sessions.crend(); ++it) {
        sum += it->minutes;
        const QDate day = dayOf(*it, zone);
        if (day >= thisWeek && day <= today)
            week += it->minutes;
        if (day.year() == today.year() && day.month() == today.month())
            month += it->minutes;
        sessions.append(sessionMap(*it));
    }
    return {{QStringLiteral("total"), total},
            {QStringLiteral("before"), qMax(0, total - sum)},
            {QStringLiteral("week"), week},
            {QStringLiteral("month"), month},
            {QStringLiteral("count"), int(file.sessions.size())},
            {QStringLiteral("sessions"), sessions}};
}

PageStats pageStats(const File &file, const QDate &today, Qt::DayOfWeek firstDay, const QTimeZone &zone)
{
    PageStats out;
    out.sessions = int(file.sessions.size());
    const QDate thisWeek = weekStart(today, firstDay);
    const QDate twoWeeks = today.addDays(-13);
    for (const Session &s : file.sessions) {
        const QDateTime start = s.merged ? s.end : s.start();
        if (!out.first.isValid() || start < out.first)
            out.first = start;
        const QDate day = dayOf(s, zone);
        if (day > today)
            continue;
        if (day >= thisWeek)
            out.week += s.minutes;
        if (day >= twoWeeks)
            out.twoWeeks += s.minutes;
        if (day.year() == today.year()) {
            out.year += s.minutes;
            if (day.month() == today.month())
                out.month += s.minutes;
        }
    }
    return out;
}

void load(const QString &dataDir, QList<StatsGame> *games)
{
    for (StatsGame &game : *games) {
        const Parsed<File> file = read(dataDir, game.id);
        game.sessions = file.value.sessions;
    }
}

namespace {
int monthKey(const QDate &d)
{
    return d.year() * 12 + d.month() - 1;
}

QList<GameMinutes> rankGames(const QHash<QString, int> &perGame, const QHash<QString, QString> &titles, int limit)
{
    QList<GameMinutes> list;
    for (auto it = perGame.cbegin(); it != perGame.cend(); ++it)
        list.append({it.key(), it.value()});
    std::sort(list.begin(), list.end(), [&](const GameMinutes &a, const GameMinutes &b) {
        if (a.minutes != b.minutes)
            return a.minutes > b.minutes;
        const int byTitle = QString::compare(titles.value(a.id), titles.value(b.id), Qt::CaseInsensitive);
        return byTitle != 0 ? byTitle < 0 : a.id < b.id;
    });
    if (list.size() > limit)
        list.resize(limit);
    return list;
}

QHash<QString, QString> titlesOf(const QList<StatsGame> &games)
{
    QHash<QString, QString> titles;
    for (const StatsGame &game : games)
        titles.insert(game.id, game.title);
    return titles;
}

QList<HistoryMonth> historyMonthsOf(const QMap<int, int> &months)
{
    QList<HistoryMonth> list;
    for (auto it = months.cbegin(); it != months.cend(); ++it)
        list.append({it.key() / 12, it.key() % 12 + 1, it.value()});
    return list;
}

PeriodStats periodStats(const QList<StatsGame> &games, const QDate &from, const QDate &to, const QTimeZone &zone)
{
    PeriodStats p;
    QHash<QString, int> perGame;
    QHash<QString, int> perPlatform;
    QSet<qint64> days;
    int plainMinutes = 0;
    int plainSessions = 0;

    for (const StatsGame &game : games) {
        for (const Session &s : game.sessions) {
            const QDate day = dayOf(s, zone);
            if (day < from || day > to)
                continue;
            days.insert(day.toJulianDay());
            perGame[game.id] += s.minutes;
            perPlatform[game.platform] += s.minutes;
            p.minutes += s.minutes;
            p.sessions++;
            if (!s.merged) {
                plainMinutes += s.minutes;
                plainSessions++;
                if (s.minutes > p.longest.minutes || (s.minutes == p.longest.minutes && s.end < p.longest.end))
                    p.longest = {game.id, s.minutes, s.end};
            }
        }
    }

    p.games = int(perGame.size());
    p.activeDays = int(days.size());
    if (plainSessions > 0)
        p.average = {qRound(double(plainMinutes) / plainSessions), plainSessions};
    else if (p.sessions > 0)
        p.average = {qRound(double(p.minutes) / p.sessions), p.sessions};

    for (auto it = perPlatform.cbegin(); it != perPlatform.cend(); ++it)
        p.platforms.append({it.key(), it.value()});
    std::sort(p.platforms.begin(), p.platforms.end(), [](const PlatformMinutes &a, const PlatformMinutes &b) {
        return a.minutes != b.minutes ? a.minutes > b.minutes : a.platform < b.platform;
    });
    p.top = rankGames(perGame, titlesOf(games), 5);

    QList<qint64> sorted(days.cbegin(), days.cend());
    std::sort(sorted.begin(), sorted.end());
    int run = 0;
    for (int i = 0; i < sorted.size(); ++i) {
        run = i > 0 && sorted.at(i) == sorted.at(i - 1) + 1 ? run + 1 : 1;
        if (run > p.streak.days) {
            p.streak.days = run;
            p.streak.first = QDate::fromJulianDay(sorted.at(i) - run + 1);
        }
    }
    return p;
}
}

MonthStats monthStats(const QList<StatsGame> &games, int year, int month, const QDate &today,
                      Qt::DayOfWeek firstDay, const QTimeZone &zone)
{
    MonthStats m;
    m.year = year;
    m.month = month;
    const QDate first(year, month, 1);
    const QDate last = first.addMonths(1).addDays(-1);
    static_cast<PeriodStats &>(m) = periodStats(games, first, last, zone);
    m.days.resize(first.daysInMonth());

    const int picked = monthKey(first);
    const int previous = picked - 1;
    const int currentKey = monthKey(today);
    const bool current = picked == currentKey;
    const QDate previousFirst = first.addMonths(-1);
    if (current)
        m.comparedThrough = qMin(today.day(), previousFirst.daysInMonth());

    QHash<int, QHash<QString, int>> dayGames;
    QMap<int, int> months;

    for (const StatsGame &game : games) {
        for (const Session &s : game.sessions) {
            const QDate day = dayOf(s, zone);
            const int key = monthKey(day);
            months[key] += s.minutes;

            if (key == previous) {
                m.previousHasHistory = true;
                if (!current || day.day() <= m.comparedThrough)
                    m.previousMinutes += s.minutes;
            }
            if (key != picked)
                continue;

            DayStats &d = m.days[day.day() - 1];
            d.minutes += s.minutes;
            d.sessions++;
            if (s.merged)
                d.mergedMinutes += s.minutes;
            dayGames[day.day()][game.id] += s.minutes;
        }
    }

    m.hasHistory = !months.isEmpty();
    if (m.hasHistory) {
        m.firstYear = months.firstKey() / 12;
        m.firstMonth = months.firstKey() % 12 + 1;
    }
    m.historyMonths = historyMonthsOf(months);

    // The picked month's own year, so a clicked bar never moves under the cursor.
    for (int i = 0; i < 12; ++i) {
        const int key = year * 12 + i;
        m.strip.append({year, i + 1, months.value(key), months.contains(key), key <= currentKey});
    }

    for (auto it = dayGames.cbegin(); it != dayGames.cend(); ++it) {
        QList<GameMinutes> list;
        for (auto g = it->cbegin(); g != it->cend(); ++g)
            list.append({g.key(), g.value()});
        std::sort(list.begin(), list.end(), [](const GameMinutes &a, const GameMinutes &b) {
            return a.minutes != b.minutes ? a.minutes > b.minutes : a.id < b.id;
        });
        m.days[it.key() - 1].games = list;
    }

    int number = 0;
    for (QDate start = weekStart(first, firstDay); start <= last; start = start.addDays(7)) {
        WeekStats w;
        w.number = ++number;
        w.firstDay = qMax(start, first).day();
        w.lastDay = qMin(start.addDays(6), last).day();
        for (int day = w.firstDay; day <= w.lastDay; ++day) {
            w.minutes += m.days.at(day - 1).minutes;
            w.mergedMinutes += m.days.at(day - 1).mergedMinutes;
        }
        m.weeks.append(w);
    }
    return m;
}

YearStats yearStats(const QList<StatsGame> &games, int year, const QDate &today, Qt::DayOfWeek firstDay,
                    const QTimeZone &zone)
{
    YearStats y;
    y.year = year;
    static_cast<PeriodStats &>(y) = periodStats(games, QDate(year, 1, 1), QDate(year, 12, 31), zone);
    y.columns.resize(12);
    y.weekdays.fill(0, 7);

    const int currentKey = monthKey(today);
    const bool current = year == today.year();
    if (current) {
        // 29 February has no counterpart in most previous years.
        const int daysThere = QDate(year - 1, today.month(), 1).daysInMonth();
        y.comparedThrough = QDate(year - 1, today.month(), qMin(today.day(), daysThere));
    }

    QHash<QString, int> monthGames[12];
    QMap<int, int> months;

    for (const StatsGame &game : games) {
        for (const Session &s : game.sessions) {
            const QDate day = dayOf(s, zone);
            months[monthKey(day)] += s.minutes;

            if (day.year() == year - 1) {
                y.previousHasHistory = true;
                if (!current || day <= y.comparedThrough)
                    y.previousMinutes += s.minutes;
            }
            if (day.year() != year)
                continue;

            MonthColumn &c = y.columns[day.month() - 1];
            c.minutes += s.minutes;
            if (s.merged)
                c.mergedMinutes += s.minutes;
            monthGames[day.month() - 1][game.id] += s.minutes;
            y.weekdays[(day.dayOfWeek() - int(firstDay) + 7) % 7] += s.minutes;
        }
    }

    y.hasHistory = !months.isEmpty();
    const int firstKey = y.hasHistory ? months.firstKey() : INT_MAX;
    if (y.hasHistory) {
        y.firstYear = firstKey / 12;
        y.firstMonth = firstKey % 12 + 1;
    }
    y.historyMonths = historyMonthsOf(months);

    const QHash<QString, QString> titles = titlesOf(games);
    for (int i = 0; i < 12; ++i) {
        MonthColumn &c = y.columns[i];
        const int key = year * 12 + i;
        c.month = i + 1;
        c.games = int(monthGames[i].size());
        c.top = rankGames(monthGames[i], titles, 3);
        c.pickable = key >= firstKey && key <= currentKey;
    }

    if (y.hasHistory) {
        for (int yr = y.firstYear; yr <= today.year(); ++yr) {
            int minutes = 0;
            for (int i = 0; i < 12; ++i)
                minutes += months.value(yr * 12 + i);
            y.years.append({yr, minutes});
        }
    }
    return y;
}

namespace {
struct MapBuilder
{
    const QHash<QString, QVariantMap> &gameInfo;
    const QHash<QString, QVariantMap> &platformInfo;

    static QVariantMap withInfo(QVariantMap entry, const QVariantMap &info)
    {
        for (auto it = info.cbegin(); it != info.cend(); ++it)
            entry.insert(it.key(), it.value());
        return entry;
    }

    QVariantMap game(const GameMinutes &g) const
    {
        return withInfo({{QStringLiteral("id"), g.id}, {QStringLiteral("minutes"), g.minutes}}, gameInfo.value(g.id));
    }

    QVariantList games(const QList<GameMinutes> &list) const
    {
        QVariantList out;
        for (const GameMinutes &g : list)
            out.append(game(g));
        return out;
    }

    QVariantList platforms(const QList<PlatformMinutes> &list) const
    {
        QVariantList out;
        for (const PlatformMinutes &p : list) {
            out.append(withInfo({{QStringLiteral("id"), p.platform}, {QStringLiteral("minutes"), p.minutes}},
                                platformInfo.value(p.platform)));
        }
        return out;
    }

    QVariantMap longest(const PeriodStats::Longest &l) const
    {
        if (l.minutes <= 0)
            return {};
        QVariantMap out = game({l.id, l.minutes});
        out.insert(QStringLiteral("end"), formatted(l.end));
        out.insert(QStringLiteral("endMs"), l.end.toMSecsSinceEpoch());
        return out;
    }

    static QVariantList historyMonths(const QList<HistoryMonth> &list)
    {
        QVariantList out;
        for (const HistoryMonth &h : list) {
            out.append(QVariantMap{{QStringLiteral("year"), h.year},
                                   {QStringLiteral("month"), h.month},
                                   {QStringLiteral("minutes"), h.minutes}});
        }
        return out;
    }

    QVariantMap period(const PeriodStats &p) const
    {
        return {{QStringLiteral("platforms"), platforms(p.platforms)},
                {QStringLiteral("top"), games(p.top)},
                {QStringLiteral("minutes"), p.minutes},
                {QStringLiteral("sessions"), p.sessions},
                {QStringLiteral("games"), p.games},
                {QStringLiteral("activeDays"), p.activeDays},
                {QStringLiteral("longest"), longest(p.longest)},
                {QStringLiteral("streakDays"), p.streak.days},
                {QStringLiteral("averageMinutes"), p.average.minutes},
                {QStringLiteral("averageSessions"), p.average.sessions}};
    }
};
}

QVariantMap toVariantMap(const MonthStats &m, const QHash<QString, QVariantMap> &gameInfo,
                         const QHash<QString, QVariantMap> &platformInfo)
{
    const MapBuilder b{gameInfo, platformInfo};

    QVariantList days;
    for (int i = 0; i < m.days.size(); ++i) {
        const DayStats &d = m.days.at(i);
        days.append(QVariantMap{{QStringLiteral("day"), i + 1},
                                {QStringLiteral("minutes"), d.minutes},
                                {QStringLiteral("mergedMinutes"), d.mergedMinutes},
                                {QStringLiteral("sessions"), d.sessions},
                                {QStringLiteral("games"), b.games(d.games)}});
    }
    QVariantList weeks;
    for (const WeekStats &w : m.weeks) {
        weeks.append(QVariantMap{{QStringLiteral("number"), w.number},
                                 {QStringLiteral("firstDay"), w.firstDay},
                                 {QStringLiteral("lastDay"), w.lastDay},
                                 {QStringLiteral("minutes"), w.minutes},
                                 {QStringLiteral("mergedMinutes"), w.mergedMinutes}});
    }
    QVariantList strip;
    for (const StripMonth &s : m.strip) {
        strip.append(QVariantMap{{QStringLiteral("year"), s.year},
                                 {QStringLiteral("month"), s.month},
                                 {QStringLiteral("minutes"), s.minutes},
                                 {QStringLiteral("hasHistory"), s.hasHistory},
                                 {QStringLiteral("pickable"), s.pickable}});
    }

    QVariantMap out = b.period(m);
    out.insert({{QStringLiteral("year"), m.year},
                {QStringLiteral("month"), m.month},
                {QStringLiteral("hasHistory"), m.hasHistory},
                {QStringLiteral("firstYear"), m.firstYear},
                {QStringLiteral("firstMonth"), m.firstMonth},
                {QStringLiteral("days"), days},
                {QStringLiteral("weeks"), weeks},
                {QStringLiteral("previousMinutes"), m.previousMinutes},
                {QStringLiteral("previousHasHistory"), m.previousHasHistory},
                {QStringLiteral("comparedThrough"), m.comparedThrough},
                {QStringLiteral("streakFirstDay"), m.streak.first.isValid() ? m.streak.first.day() : 0},
                {QStringLiteral("strip"), strip},
                {QStringLiteral("historyMonths"), MapBuilder::historyMonths(m.historyMonths)}});
    return out;
}

QVariantMap toVariantMap(const YearStats &y, const QHash<QString, QVariantMap> &gameInfo,
                         const QHash<QString, QVariantMap> &platformInfo)
{
    const MapBuilder b{gameInfo, platformInfo};

    QVariantList columns;
    for (const MonthColumn &c : y.columns) {
        columns.append(QVariantMap{{QStringLiteral("month"), c.month},
                                   {QStringLiteral("minutes"), c.minutes},
                                   {QStringLiteral("mergedMinutes"), c.mergedMinutes},
                                   {QStringLiteral("games"), c.games},
                                   {QStringLiteral("top"), b.games(c.top)},
                                   {QStringLiteral("pickable"), c.pickable}});
    }
    QVariantList weekdays;
    for (int minutes : y.weekdays)
        weekdays.append(minutes);
    QVariantList years;
    for (const StripYear &s : y.years)
        years.append(QVariantMap{{QStringLiteral("year"), s.year}, {QStringLiteral("minutes"), s.minutes}});

    const auto part = [](const QDate &d, int (QDate::*get)() const) { return d.isValid() ? (d.*get)() : 0; };
    QVariantMap out = b.period(y);
    out.insert({{QStringLiteral("year"), y.year},
                {QStringLiteral("hasHistory"), y.hasHistory},
                {QStringLiteral("firstYear"), y.firstYear},
                {QStringLiteral("firstMonth"), y.firstMonth},
                {QStringLiteral("columns"), columns},
                {QStringLiteral("weekdays"), weekdays},
                {QStringLiteral("previousMinutes"), y.previousMinutes},
                {QStringLiteral("previousHasHistory"), y.previousHasHistory},
                {QStringLiteral("comparedThroughMonth"), part(y.comparedThrough, &QDate::month)},
                {QStringLiteral("comparedThroughDay"), part(y.comparedThrough, &QDate::day)},
                {QStringLiteral("streakFirstMonth"), part(y.streak.first, &QDate::month)},
                {QStringLiteral("streakFirstDay"), part(y.streak.first, &QDate::day)},
                {QStringLiteral("years"), years},
                {QStringLiteral("historyMonths"), MapBuilder::historyMonths(y.historyMonths)}});
    return out;
}
}
