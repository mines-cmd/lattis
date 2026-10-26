#include "steammedia.h"

#include "vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace SteamMedia {
namespace {
struct Streams
{
    QString videoInit;
    QStringList videoChunks;
    QString audioInit;
    QStringList audioChunks;
};

QString manifestAttribute(const QByteArray &mpd, const QByteArray &name)
{
    const QRegularExpression re(QStringLiteral("\\b%1=\"([^\"]*)\"").arg(QString::fromLatin1(name)));
    return re.match(QString::fromUtf8(mpd)).captured(1);
}

QStringList chunks(const QDir &dir, int stream)
{
    return dir.entryList({u"chunk-stream%1-*.m4s"_s.arg(stream)}, QDir::Files, QDir::Name);
}

Streams streamsIn(const QString &videoDir)
{
    const QDir dir(videoDir);
    Streams s;
    const QStringList video = chunks(dir, 0);
    if (video.isEmpty() || !dir.exists(u"init-stream0.m4s"_s))
        return s;
    s.videoInit = dir.filePath(u"init-stream0.m4s"_s);
    for (const QString &name : video)
        s.videoChunks.append(dir.filePath(name));
    const QStringList audio = chunks(dir, 1);
    if (!audio.isEmpty() && dir.exists(u"init-stream1.m4s"_s)) {
        s.audioInit = dir.filePath(u"init-stream1.m4s"_s);
        for (const QString &name : audio)
            s.audioChunks.append(dir.filePath(name));
    }
    return s;
}

bool concatenate(const QString &init, const QStringList &chunkFiles, const QString &outPath)
{
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    for (const QString &path : QStringList{init} + chunkFiles) {
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly))
            return false;
        while (!in.atEnd()) {
            if (out.write(in.read(1 << 20)) < 0)
                return false;
        }
    }
    return out.flush();
}

// The clip's video folder is the one bg_* folder under video/ that has a manifest.
QString videoDirOf(const QString &clipFolder)
{
    const QDir video(clipFolder + u"/video"_s);
    const QStringList dirs = video.entryList({u"bg_*"_s}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : dirs) {
        if (QFileInfo::exists(video.filePath(name) + u"/session.mpd"_s))
            return video.filePath(name);
    }
    return {};
}

// Settings → Game Recording can move the recordings folder; clips made before the move stay in userdata.
QStringList recordingRoots(const QString &userDir)
{
    QStringList roots = {userDir + u"/gamerecordings"_s};
    QFile file(userDir + u"/config/localconfig.vdf"_s);
    if (!file.open(QIODevice::ReadOnly))
        return roots;
    const Vdf::Document doc = Vdf::parse(file.readAll());
    const Vdf::Node *recording = doc.root.find({u"UserLocalConfigStore"_s, u"GameRecording"_s});
    const QString moved = recording ? recording->valueOf(u"BackgroundRecordPath") : QString();
    if (!moved.isEmpty() && QFileInfo(moved).canonicalFilePath() != QFileInfo(roots.first()).canonicalFilePath())
        roots.append(moved);
    return roots;
}

void appendClips(const QDir &clips, const QString &prefix, QList<Clip> &out)
{
    for (const QString &name : clips.entryList({prefix + u'*'}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        Clip clip;
        clip.id = name;
        clip.folder = clips.filePath(name);
        clip.videoDir = videoDirOf(clip.folder);
        if (clip.videoDir.isEmpty())
            continue;
        QFile manifest(clip.videoDir + u"/session.mpd"_s);
        if (!manifest.open(QIODevice::ReadOnly))
            continue;
        clip.durationMs = parseDurationMs(manifestAttribute(manifest.readAll(), "mediaPresentationDuration"));
        if (clip.durationMs <= 0 || streamsIn(clip.videoDir).videoInit.isEmpty())
            continue;
        const QString thumbnail = clip.folder + u"/thumbnail.jpg"_s;
        if (QFileInfo::exists(thumbnail))
            clip.thumbnail = thumbnail;
        clip.time = QDateTime::fromString(name.mid(prefix.size()), u"yyyyMMdd_HHmmss"_s);
        out.append(clip);
    }
}
}

qint64 parseDurationMs(const QString &isoDuration)
{
    static const QRegularExpression re(
        QStringLiteral("^PT(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+(?:\\.\\d+)?)S)?$"));
    const QRegularExpressionMatch m = re.match(isoDuration);
    if (!m.hasMatch() || isoDuration == QLatin1String("PT"))
        return 0;
    const double seconds = m.captured(1).toDouble() * 3600 + m.captured(2).toDouble() * 60 + m.captured(3).toDouble();
    return qRound64(seconds * 1000);
}

QString lengthText(qint64 durationMs)
{
    const qint64 total = (durationMs + 500) / 1000;
    const qint64 h = total / 3600;
    const qint64 m = total / 60 % 60;
    const qint64 s = total % 60;
    return h > 0 ? u"%1:%2:%3"_s.arg(h).arg(m, 2, 10, u'0').arg(s, 2, 10, u'0')
                 : u"%1:%2"_s.arg(m).arg(s, 2, 10, u'0');
}

QList<Clip> listClips(const SteamInstall &install, qint64 appId)
{
    const QString user = install.userDataDir();
    if (user.isEmpty() || appId <= 0)
        return {};
    const QString prefix = u"clip_%1_"_s.arg(appId);
    QList<Clip> out;
    for (const QString &root : recordingRoots(user))
        appendClips(QDir(root + u"/clips"_s), prefix, out);
    std::sort(out.begin(), out.end(), [](const Clip &a, const Clip &b) {
        return a.time != b.time ? a.time > b.time : a.id > b.id;
    });
    return out;
}

QList<Screenshot> listScreenshots(const SteamInstall &install, qint64 appId)
{
    const QString user = install.userDataDir();
    if (user.isEmpty() || appId <= 0)
        return {};
    const QDir dir(u"%1/760/remote/%2/screenshots"_s.arg(user).arg(appId));
    QList<Screenshot> out;
    for (const QFileInfo &file : dir.entryInfoList({u"*.jpg"_s, u"*.png"_s}, QDir::Files)) {
        Screenshot shot;
        shot.id = file.fileName();
        shot.path = file.filePath();
        shot.time = QDateTime::fromString(file.completeBaseName().section(u'_', 0, 0), u"yyyyMMddHHmmss"_s);
        if (!shot.time.isValid())
            shot.time = file.lastModified();
        out.append(shot);
    }
    std::sort(out.begin(), out.end(), [](const Screenshot &a, const Screenshot &b) {
        return a.time != b.time ? a.time > b.time : a.id > b.id;
    });
    return out;
}

QString remuxClip(const Clip &clip,const QString &ffmpeg, const QString &outPath)
{
    if (ffmpeg.isEmpty())
        return QObject::tr("ffmpeg isn't installed");
    const Streams s = streamsIn(clip.videoDir);
    if (s.videoInit.isEmpty())
        return QObject::tr("The clip has no video");

    // Handing ffmpeg session.mpd reads only the first chunk, so each stream is joined into one file first.
    const QString video = outPath + u".video.mp4"_s;
    const QString audio = outPath + u".audio.mp4"_s;
    const auto cleanUp = [&] {
        QFile::remove(video);
        QFile::remove(audio);
    };
    if (!concatenate(s.videoInit, s.videoChunks, video)) {
        cleanUp();
        return QObject::tr("Couldn't read the clip");
    }
    QStringList args = {u"-y"_s, u"-loglevel"_s, u"error"_s, u"-i"_s, video};
    if (!s.audioInit.isEmpty()) {
        if (!concatenate(s.audioInit, s.audioChunks, audio)) {
            cleanUp();
            return QObject::tr("Couldn't read the clip");
        }
        args << u"-i"_s << audio;
    }
    args << u"-c"_s << u"copy"_s << u"-f"_s << u"mp4"_s << outPath;

    QProcess process;
    process.start(ffmpeg, args);
    const bool finished = process.waitForFinished(60000);
    cleanUp();
    if (!finished) {
        process.kill();
        process.waitForFinished();
        QFile::remove(outPath);
        return QObject::tr("ffmpeg took too long");
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QFile::remove(outPath);
        const QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed().section(u'\n', -1);
        return detail.isEmpty() ? QObject::tr("ffmpeg failed") : detail;
    }
    return {};
}
}
