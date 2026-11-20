#include "romdb.h"

#include "romscan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRomDb, "launcher.romdb")

namespace {
const QSet<QString> kCrcPlatforms = {QStringLiteral("gba"),     QStringLiteral("n64"), QStringLiteral("nes"),
                                     QStringLiteral("snes"),    QStringLiteral("genesis"), QStringLiteral("gb"),
                                     QStringLiteral("gbc"),     QStringLiteral("sms"), QStringLiteral("gamegear")};

// What a platform's database is searched by.
QString keyOf(const QString &platform, const QString &fileTitleId, const RomInfo &info)
{
    if (platform == QLatin1String("switch")) {
        const QString id = (!fileTitleId.isEmpty() ? fileTitleId : info.titleId).toUpper();
        return id.endsWith(QLatin1String("000")) ? id : QString();
    }
    if (kCrcPlatforms.contains(platform))
        return info.crc32.toUpper();
    return info.serial.toUpper();
}

// What stands for "not a game" in a database: arcade.tsv names a BIOS or device zip this.
constexpr QLatin1String kNotAGame("-");
}

RomDb::RomDb(RomDbOptions options) : m_options(std::move(options)) {}

QString RomDb::fileOf(const QString &platform)
{
    if (platform == QLatin1String("wii") || platform == QLatin1String("gamecube"))
        return QStringLiteral("wii");
    static const QSet<QString> kPlatforms = {QStringLiteral("switch"), QStringLiteral("3ds"), QStringLiteral("nds"),
                                             QStringLiteral("ps1"), QStringLiteral("ps2"), QStringLiteral("arcade")};
    return kPlatforms.contains(platform) || kCrcPlatforms.contains(platform) ? platform : QString();
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

bool RomDb::isNotAGame(const QString &platform, const QString &path)
{
    if (platform != QLatin1String("arcade"))
        return false;
    return database(platform).value(QFileInfo(path).completeBaseName().toUpper()) == kNotAGame;
}

void RomDb::name(const QString &platform, const QString &fileTitleId, RomInfo *info)
{
    if (!info->title.isEmpty())
        return;
    // A file with a header the list leaves out can match either way.
    for (const QString &crc : {keyOf(platform, fileTitleId, *info), info->crc32WithHeader.toUpper()}) {
        if (crc.isEmpty())
            continue;
        const QString found = database(platform).value(crc);
        if (found == kNotAGame)
            continue;
        if (const QString title = RomName::cleanName(found, platform); !title.isEmpty()) {
            info->title = title;
            info->fromDatabase = true;
            return;
        }
    }
    if (!info->fallbackTitle.isEmpty())
        info->title = info->fallbackTitle;
}
