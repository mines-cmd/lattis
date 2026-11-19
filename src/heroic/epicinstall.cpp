#include "epicinstall.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStorageInfo>

namespace {
std::optional<EpicInstallInfo> fromObject(const QJsonObject &root)
{
    const QJsonObject manifest = root.value(QLatin1String("manifest")).toObject();
    const QJsonValue download = manifest.value(QLatin1String("download_size"));
    const QJsonValue disk = manifest.value(QLatin1String("disk_size"));
    if (!download.isDouble() && !disk.isDouble())
        return std::nullopt;

    EpicInstallInfo info;
    if (download.isDouble())
        info.downloadSize = qint64(download.toDouble());
    if (disk.isDouble())
        info.diskSize = qint64(disk.toDouble());
    info.platform = root.value(QLatin1String("install")).toObject().value(QLatin1String("platform")).toString();
    info.hasLinux = root.value(QLatin1String("game")).toObject().value(QLatin1String("platform_versions"))
                        .toObject().contains(QLatin1String("Linux"));
    return info;
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
}

std::optional<EpicInstallInfo> EpicInstall::parseInfo(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return std::nullopt;
    return fromObject(doc.object());
}

std::optional<EpicInstallInfo> EpicInstall::cachedInfo(const QString &heroicRoot, const QString &appName)
{
    if (heroicRoot.isEmpty() || appName.isEmpty())
        return std::nullopt;
    const QJsonObject all = readObject(heroicRoot + QStringLiteral("/store_cache/legendary_install_info.json"));
    return fromObject(all.value(appName).toObject());
}

QString EpicInstall::gameFolder(const QString &heroicRoot, const QString &appName)
{
    if (heroicRoot.isEmpty() || appName.isEmpty())
        return {};
    const QJsonObject game = readObject(heroicRoot + QStringLiteral("/legendaryConfig/legendary/metadata/")
                                        + appName + QStringLiteral(".json"));
    return game.value(QLatin1String("metadata")).toObject().value(QLatin1String("customAttributes")).toObject()
        .value(QLatin1String("FolderName")).toObject().value(QLatin1String("value")).toString().trimmed();
}

QString EpicInstall::defaultFolder(const QString &heroicRoot)
{
    const QString configured = readObject(heroicRoot + QStringLiteral("/config.json"))
                                   .value(QLatin1String("defaultSettings")).toObject()
                                   .value(QLatin1String("defaultInstallPath")).toString().trimmed();
    return configured.isEmpty() ? QDir::homePath() + QStringLiteral("/Games/Heroic") : expandHome(configured);
}

QString EpicInstall::platformFor(const EpicInstallInfo &info)
{
    return info.hasLinux ? QStringLiteral("Linux") : QStringLiteral("Windows");
}

QStringList EpicInstall::installArgs(const QString &folder, const QString &platform)
{
    return {QStringLiteral("--base-path"), folder, QStringLiteral("--platform"), platform,
            QStringLiteral("--skip-sdl")};
}

qint64 EpicInstall::freeSpace(const QString &folder)
{
    QString path = QDir::cleanPath(expandHome(folder));
    if (path.isEmpty() || !QDir::isAbsolutePath(path))
        return -1;
    while (!QFileInfo::exists(path)) {
        const QString parent = QFileInfo(path).absolutePath();
        if (parent == path)
            return -1;
        path = parent;
    }
    const QStorageInfo storage(path);
    return storage.isValid() && storage.isReady() ? storage.bytesAvailable() : -1;
}

QString EpicInstall::expandHome(const QString &folder)
{
    if (folder == u"~")
        return QDir::homePath();
    if (folder.startsWith(u"~/"))
        return QDir::homePath() + folder.mid(1);
    return folder;
}
