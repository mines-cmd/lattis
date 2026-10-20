#include "apppaths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

namespace AppPaths {
QString dataDir()
{
    const QString overridden = qEnvironmentVariable("LATTIS_DATA");
    if (!overridden.isEmpty())
        return QDir(overridden).absolutePath();
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString stateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::StateLocation);
}

QString dataDirKey(const QString &dataDir)
{
    const QByteArray path = QDir(dataDir).absolutePath().toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(path, QCryptographicHash::Sha1).toHex().left(12));
}
}
