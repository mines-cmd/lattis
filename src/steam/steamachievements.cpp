#include "steamachievements.h"

#include "binaryvdf.h"
#include "vdf.h"

#include <QFile>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace SteamAchievements {
namespace {
constexpr int kAchievementsStatType = 4;

bool readFile(const QString &path, QByteArray &out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    out = file.readAll();
    return true;
}

// Steam writes the same value as a string or as a number depending on the game.
qint64 numberOf(const BinaryVdf::Node *node)
{
    if (!node)
        return 0;
    return node->type == BinaryVdf::String ? node->toString().toLongLong() : node->toInt32();
}

bool isAchievementStat(const BinaryVdf::Node &stat)
{
    const BinaryVdf::Node *type = stat.child("type");
    if (!type)
        return false;
    return type->type == BinaryVdf::String ? type->toString().compare(u"ACHIEVEMENTS"_s, Qt::CaseInsensitive) == 0
                                           : type->toInt32() == kAchievementsStatType;
}

// A map of language → text (plus a `token`), or a plain string.
QString localized(const BinaryVdf::Node *node, const QString &language)
{
    if (!node)
        return {};
    if (node->type == BinaryVdf::String)
        return node->toString();
    for (const QString &wanted : {language, u"english"_s}) {
        if (const BinaryVdf::Node *text = node->child(wanted.toUtf8()); text && !text->toString().isEmpty())
            return text->toString();
    }
    for (const BinaryVdf::Node &text : node->children) {
        if (text.key != "token" && !text.toString().isEmpty())
            return text.toString();
    }
    return {};
}

const BinaryVdf::Node *firstChild(const BinaryVdf::Document &doc)
{
    return doc.ok && !doc.root.children.isEmpty() ? &doc.root.children.first() : nullptr;
}
}

QList<Achievement> readAchievements(const SteamInstall &install, qint64 appId, const QString &language)
{
    if (install.root.isEmpty() || install.userId.isEmpty() || appId <= 0)
        return {};
    const QString stats = install.root + u"/appcache/stats/"_s;
    QByteArray bytes;
    if (!readFile(stats + u"UserGameStatsSchema_%1.bin"_s.arg(appId), bytes))
        return {};
    const BinaryVdf::Document schema = BinaryVdf::parse(bytes);
    const BinaryVdf::Node *schemaStats = firstChild(schema) ? firstChild(schema)->child("stats") : nullptr;
    if (!schemaStats)
        return {};

    BinaryVdf::Document progress;
    if (readFile(stats + u"UserGameStats_%1_%2.bin"_s.arg(install.userId).arg(appId), bytes))
        progress = BinaryVdf::parse(bytes);
    const BinaryVdf::Node *cache = progress.ok ? progress.root.child("cache") : nullptr;

    struct Entry
    {
        Achievement achievement;
        int order = 0;
    };
    QList<Entry> entries;
    // The bit numbers restart in every stat, and so does the progress: each stat has its own `data`.
    for (const BinaryVdf::Node &stat : schemaStats->children) {
        const BinaryVdf::Node *bits = stat.child("bits");
        if (!isAchievementStat(stat) || !bits)
            continue;
        const BinaryVdf::Node *statProgress = cache ? cache->child(stat.key) : nullptr;
        const quint32 unlocked = quint32(statProgress ? numberOf(statProgress->child("data")) : 0);
        const BinaryVdf::Node *times = statProgress ? statProgress->child("AchievementTimes") : nullptr;

        for (const BinaryVdf::Node &bit : bits->children) {
            bool ok = false;
            const int index = bit.key.toInt(&ok);
            const BinaryVdf::Node *name = bit.child("name");
            if (!ok || index < 0 || index > 31 || !name)
                continue;
            Entry e;
            Achievement &a = e.achievement;
            a.apiName = name->toString();
            const BinaryVdf::Node *display = bit.child("display");
            if (!display)
                continue;
            a.name = localized(display->child("name"), language);
            a.description = localized(display->child("desc"), language);
            a.hidden = numberOf(display->child("hidden")) != 0;
            a.unlocked = (unlocked >> index) & 1;
            if (a.unlocked && times) {
                if (const qint64 when = numberOf(times->child(bit.key)); when > 0)
                    a.unlockTime = QDateTime::fromSecsSinceEpoch(when);
            }
            const BinaryVdf::Node *icon = display->child(a.unlocked ? "icon" : "icon_gray");
            if (icon && !icon->toString().isEmpty()) {
                a.iconUrl = u"https://cdn.fastly.steamstatic.com/steamcommunity/public/images/apps/%1/%2"_s.arg(appId)
                                .arg(icon->toString());
            }
            e.order = int(entries.size());
            entries.append(e);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        if (a.achievement.unlocked != b.achievement.unlocked)
            return a.achievement.unlocked;
        if (a.achievement.unlocked && a.achievement.unlockTime != b.achievement.unlockTime)
            return a.achievement.unlockTime > b.achievement.unlockTime;
        if (!a.achievement.unlocked && a.achievement.hidden != b.achievement.hidden)
            return b.achievement.hidden;
        return a.order < b.order;
    });
    QList<Achievement> out;
    out.reserve(entries.size());
    for (const Entry &e : std::as_const(entries))
        out.append(e.achievement);
    return out;
}

QString steamLanguage(const SteamInstall &install, const QString &home)
{
    const QString registry = install.kind == SteamInstall::Kind::Flatpak
                                 ? home + u"/.var/app/com.valvesoftware.Steam/.steam/registry.vdf"_s
                                 : home + u"/.steam/registry.vdf"_s;
    QByteArray bytes;
    if (!readFile(registry, bytes))
        return u"english"_s;
    const Vdf::Document doc = Vdf::parse(bytes);
    const Vdf::Node *steam = doc.ok ? doc.root.find({u"Registry"_s, u"HKCU"_s, u"Software"_s, u"Valve"_s, u"Steam"_s})
                                    : nullptr;
    const QString language = steam ? steam->valueOf(u"language") : QString();
    return language.isEmpty() ? u"english"_s : language;
}
}
