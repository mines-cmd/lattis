#include "heroicscan.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>

namespace {
HeroicStamp stampOf(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return {};
    return {info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch(), info.size()};
}

bool readObject(const QString &path, QJsonObject *object, QString *warning)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *warning = QStringLiteral("%1: could not read: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        *warning = QStringLiteral("%1: %2").arg(path, error.errorString());
        return false;
    }
    if (!doc.isObject()) {
        *warning = QStringLiteral("%1: not a JSON object").arg(path);
        return false;
    }
    *object = doc.object();
    return true;
}

QString titleOf(const QJsonObject &game)
{
    return game.value(QStringLiteral("title")).toString().trimmed();
}

void readLibrary(const QString &path, HeroicState *state)
{
    state->libraryStamp = stampOf(path);
    if (state->libraryStamp.size < 0)
        return;
    QJsonObject root;
    if (!readObject(path, &root, &state->libraryWarning))
        return;
    QSet<QString> seen;
    const QJsonArray entries = root.value(QStringLiteral("library")).toArray();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        const QString appName = entry.value(QStringLiteral("app_name")).toString();
        if (appName.isEmpty())
            continue;
        state->libraryKnown = true;
        const QJsonObject install = entry.value(QStringLiteral("install")).toObject();
        if (install.value(QStringLiteral("is_dlc")).toBool() || seen.contains(appName))
            continue;
        seen.insert(appName);
        HeroicGame game;
        game.appName = appName;
        game.title = titleOf(entry);
        game.installed = entry.value(QStringLiteral("is_installed")).toBool();
        game.installPath = install.value(QStringLiteral("install_path")).toString();
        game.artCover = entry.value(QStringLiteral("art_cover")).toString();
        game.artSquare = entry.value(QStringLiteral("art_square")).toString();
        game.artLogo = entry.value(QStringLiteral("art_logo")).toString();
        state->library.append(game);
    }
}

void readInstalled(const QString &path, HeroicState *state)
{
    if (!QFileInfo::exists(path)) {
        state->installedOk = true;
        return;
    }
    QJsonObject root;
    QString warning;
    if (!readObject(path, &root, &warning)) {
        state->warnings.append(warning);
        return;
    }
    state->installedOk = true;
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        if (it.key().isEmpty() || entry.isEmpty() || entry.value(QStringLiteral("is_dlc")).toBool())
            continue;
        HeroicGame game;
        game.appName = it.key();
        game.title = titleOf(entry);
        game.installed = true;
        game.installPath = entry.value(QStringLiteral("install_path")).toString();
        game.sizeBytes = entry.value(QStringLiteral("install_size")).toInteger();
        state->installed.append(game);
    }
}

void readPlayTime(const QString &path, HeroicState *state)
{
    if (!QFileInfo::exists(path)) {
        state->playTimeOk = true;
        return;
    }
    QJsonObject root;
    QString warning;
    if (!readObject(path, &root, &warning)) {
        state->warnings.append(warning);
        return;
    }
    state->playTimeOk = true;
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        if (entry.isEmpty())
            continue;
        HeroicPlayTime time;
        time.minutes = int(entry.value(QStringLiteral("totalPlayed")).toDouble());
        const QDateTime last = QDateTime::fromString(entry.value(QStringLiteral("lastPlayed")).toString(),
                                                     Qt::ISODateWithMs);
        if (last.isValid())
            time.lastPlayed = last.toSecsSinceEpoch();
        state->playTime.insert(it.key(), time);
    }
}
}

QList<HeroicGame> HeroicState::games() const
{
    QHash<QString, const HeroicGame *> onDisk;
    for (const HeroicGame &game : installed)
        onDisk.insert(game.appName, &game);

    QList<HeroicGame> out;
    out.reserve(library.size() + installed.size());
    for (HeroicGame game : library) {
        // The cache only changes on a refresh, so it's the fallback when installed.json can't be read.
        if (installedOk) {
            const HeroicGame *here = onDisk.take(game.appName);
            game.installed = here != nullptr;
            game.installPath = here ? here->installPath : QString();
            game.sizeBytes = here ? here->sizeBytes : 0;
            if (here && game.title.isEmpty())
                game.title = here->title;
        }
        out.append(game);
    }
    for (const HeroicGame &game : installed) {
        if (onDisk.contains(game.appName))
            out.append(game);
    }
    for (HeroicGame &game : out) {
        if (game.title.isEmpty())
            game.title = game.appName;
    }
    return out;
}

QStringList HeroicState::watchPaths() const
{
    if (root.isEmpty())
        return {};
    return {HeroicScan::installedPath(root), HeroicScan::playTimePath(root)};
}

namespace HeroicScan {
QString libraryPath(const QString &root)
{
    return root + QStringLiteral("/store_cache/legendary_library.json");
}

QString installedPath(const QString &root)
{
    return root + QStringLiteral("/legendaryConfig/legendary/installed.json");
}

QString playTimePath(const QString &root)
{
    return root + QStringLiteral("/store/timestamp.json");
}

HeroicState scan(const QString &root, const HeroicState &previous)
{
    HeroicState state;
    if (root.isEmpty())
        return state;
    state.root = root;

    // Megabytes of JSON on a large library, and every rescan comes here.
    const QString library = libraryPath(root);
    if (previous.root == root && previous.libraryStamp == stampOf(library)) {
        state.libraryStamp = previous.libraryStamp;
        state.library = previous.library;
        state.libraryKnown = previous.libraryKnown;
        state.libraryWarning = previous.libraryWarning;
    } else {
        readLibrary(library, &state);
    }
    if (!state.libraryWarning.isEmpty())
        state.warnings.append(state.libraryWarning);

    readInstalled(installedPath(root), &state);
    readPlayTime(playTimePath(root), &state);
    return state;
}

std::optional<qint64> lastPlayed(const QString &root, const QString &appName)
{
    HeroicState state;
    readPlayTime(playTimePath(root), &state);
    if (!state.playTimeOk)
        return std::nullopt;
    return state.playTime.value(appName).lastPlayed;
}

QString cachedImage(const QString &root, const QString &url)
{
    if (url.isEmpty())
        return {};
    // Heroic names each copy after the address it asked its image service for, size included.
    const QByteArray asked = url.toUtf8() + "?h=400&resize=1&w=300";
    const QByteArray name = QCryptographicHash::hash(asked, QCryptographicHash::Sha256).toHex();
    const QString path = root + QStringLiteral("/images-cache/") + QString::fromLatin1(name);
    return QFileInfo(path).isFile() ? path : QString();
}
}
