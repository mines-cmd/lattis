#include "heroicsaves.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace HeroicSaves {
namespace {
// Heroic writes absolute paths, but a hand-edited file may use "~".
QString absolute(const QString &path)
{
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

QJsonObject readObject(const QString &path, QStringList &warnings)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (!doc.isObject()) {
        warnings.append(QStringLiteral("%1: not a JSON object").arg(path));
        return {};
    }
    return doc.object();
}

QString wineType(const QJsonObject &settings)
{
    return settings.value(QLatin1String("wineVersion")).toObject().value(QLatin1String("type")).toString();
}
}

HeroicSavesInfo read(const QString &root, const QString &appName, const QString &systemUser)
{
    HeroicSavesInfo info;

    const QJsonObject own = readObject(root + QStringLiteral("/GamesConfig/") + appName + QStringLiteral(".json"),
                                       info.warnings)
                                .value(appName)
                                .toObject();
    const QJsonObject defaults = readObject(root + QStringLiteral("/config.json"), info.warnings)
                                     .value(QLatin1String("defaultSettings"))
                                     .toObject();

    QString prefix = own.value(QLatin1String("winePrefix")).toString();
    if (prefix.isEmpty())
        prefix = defaults.value(QLatin1String("winePrefix")).toString();
    if (!prefix.isEmpty())
        info.prefix = QDir::cleanPath(absolute(prefix));

    QString type = wineType(own);
    if (type.isEmpty())
        type = wineType(defaults);
    info.wineUser = type == QLatin1String("proton") ? QStringLiteral("steamuser") : systemUser;

    const QString metadataPath = root + QStringLiteral("/legendaryConfig/legendary/metadata/") + appName
        + QStringLiteral(".json");
    info.cloudSaveFolder = readObject(metadataPath, info.warnings)
                               .value(QLatin1String("metadata"))
                               .toObject()
                               .value(QLatin1String("customAttributes"))
                               .toObject()
                               .value(QLatin1String("CloudSaveFolder"))
                               .toObject()
                               .value(QLatin1String("value"))
                               .toString()
                               .trimmed();
    return info;
}

QString expandCloudSaveFolder(const QString &folder, const QString &prefix, const QString &wineUser,
                              const QString &installDir)
{
    if (folder.isEmpty())
        return {};
    const QString user = prefix.isEmpty() ? QString() : prefix + QStringLiteral("/drive_c/users/") + wineUser;

    QString result;
    bool resolved = true;
    static const QRegularExpression variable(QStringLiteral("\\{([^{}]*)\\}"));
    qsizetype last = 0;
    for (auto it = variable.globalMatch(folder); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        result += folder.mid(last, match.capturedStart() - last);
        last = match.capturedEnd();

        const QString name = match.captured(1).toLower();
        QString value;
        if (name == QLatin1String("appdata") && !user.isEmpty())
            value = user + QStringLiteral("/AppData/Local");
        else if (name == QLatin1String("userdir") && !user.isEmpty())
            value = user + QStringLiteral("/Documents");
        else if (name == QLatin1String("usersavedgames") && !user.isEmpty())
            value = user + QStringLiteral("/Saved Games");
        else if (name == QLatin1String("installdir"))
            value = installDir;
        else if (name == QLatin1String("epicid"))
            value = QStringLiteral("*");
        if (value.isEmpty())
            resolved = false;
        result += value;
    }
    result += folder.mid(last);

    if (!resolved)
        return {};
    return QDir::cleanPath(result.replace(u'\\', u'/'));
}
}
