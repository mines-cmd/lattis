#include "romdb.h"

#include "romscan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRomDb, "launcher.romdb")

namespace {
// What a platform's database is searched by.
QString keyOf(const QString &platform, const QString &fileTitleId, const RomInfo &info)
{
    if (platform == QLatin1String("switch")) {
        const QString id = (!fileTitleId.isEmpty() ? fileTitleId : info.titleId).toUpper();
        return id.endsWith(QLatin1String("000")) ? id : QString();
    }
    if (platform == QLatin1String("gba") || platform == QLatin1String("n64"))
        return info.crc32.toUpper();
    return info.serial.toUpper();
}
}

RomDb::RomDb(RomDbOptions options) : m_options(std::move(options)) {}

QString RomDb::fileOf(const QString &platform)
{
    if (platform == QLatin1String("wii") || platform == QLatin1String("gamecube"))
        return QStringLiteral("wii");
    static const QSet<QString> kPlatforms = {QStringLiteral("switch"), QStringLiteral("3ds"), QStringLiteral("nds"),
                                             QStringLiteral("ps1"), QStringLiteral("ps2"), QStringLiteral("gba"),
                                             QStringLiteral("n64")};
    return kPlatforms.contains(platform) ? platform : QString();
}

bool RomDb::covers(const QString &platform)
{
    return !fileOf(platform).isEmpty();
}

QHash<QString, QString> RomDb::parse(const QByteArray &bytes)
{
    QHash<QString, QString> out;
    for (const QByteArray &line : bytes.split('\n')) {
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        const qsizetype tab = line.indexOf('\t');
        if (tab <= 0)
            continue;
        const QString name = QString::fromUtf8(line.mid(tab + 1)).trimmed();
        if (!name.isEmpty())
            out.insert(QString::fromUtf8(line.left(tab)).toUpper(), name);
    }
    return out;
}

const QHash<QString, QString> &RomDb::database(const QString &platform)
{
    const QString file = fileOf(platform);
    const auto loaded = m_loaded.constFind(file);
    if (loaded != m_loaded.cend())
        return *loaded;
    static const QHash<QString, QString> kNone;
    if (m_options.dir.isEmpty() || file.isEmpty())
        return kNone;

    const QString path = QDir(m_options.dir).filePath(file + QStringLiteral(".tsv"));
    if (!QFileInfo::exists(path) && m_options.download && !m_tried.contains(file)) {
        m_tried.insert(file);
        QDir().mkpath(m_options.dir);
        if (!m_options.download(platform, path))
            qCDebug(lcRomDb).noquote() << "no name database for" << platform;
    }
    QFile in(path);
    const QByteArray bytes = in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
    return m_loaded.insert(file, parse(bytes)).value();
}

void RomDb::name(const QString &platform, const QString &fileTitleId, RomInfo *info)
{
    if (!info->title.isEmpty())
        return;
    if (const QString key = keyOf(platform, fileTitleId, *info); !key.isEmpty()) {
        const QString found = database(platform).value(key);
        if (const QString title = RomName::cleanName(found, platform); !title.isEmpty()) {
            info->title = title;
            info->fromDatabase = true;
            return;
        }
    }
    if (!info->fallbackTitle.isEmpty())
        info->title = info->fallbackTitle;
}
