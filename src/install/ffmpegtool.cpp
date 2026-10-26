#include "ffmpegtool.h"

#include "releasesource.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QtConcurrent>

#include <archive.h>
#include <archive_entry.h>

#include <memory>

Q_LOGGING_CATEGORY(lcFfmpeg, "launcher.ffmpeg")

FfmpegTool::Setup FfmpegTool::s_setup;

namespace {
constexpr qint64 FileLimit = qint64(2) * 1024 * 1024 * 1024;
constexpr qint64 TotalLimit = qint64(1) * 1024 * 1024 * 1024;
const QString ReleaseProject = QStringLiteral("github:BtbN/FFmpeg-Builds");

QString tr(const char *text)
{
    return QCoreApplication::translate("FfmpegTool", text);
}

struct ReadCloser
{
    void operator()(struct archive *a) const { archive_read_free(a); }
};

enum class Part { Skip, Binary, Library, License };

// The archive is `<top folder>/bin/ffmpeg`, `<top folder>/lib/*.so.*` (what ffmpeg links, found through its RPATH),
// and the licence; the rest (ffplay, ffprobe, headers, docs) isn't kept.
Part partOf(const QStringList &parts)
{
    if (parts.size() == 3 && parts[1] == QLatin1String("bin") && parts[2] == QLatin1String("ffmpeg"))
        return Part::Binary;
    if (parts.size() == 3 && parts[1] == QLatin1String("lib") && parts[2].contains(QLatin1String(".so.")))
        return Part::Library;
    if (parts.size() == 2 && parts[1] == QLatin1String("LICENSE.txt"))
        return Part::License;
    return Part::Skip;
}

QString releaseFile(const QString &toolsDir)
{
    return QDir(toolsDir).filePath(QStringLiteral("ffmpeg/bin/ffmpeg"));
}
}

namespace FfmpegInstall {
QString assetName(const QString &cpu)
{
    QString arch;
    if (cpu == QLatin1String("x86_64"))
        arch = QStringLiteral("linux64");
    else if (cpu == QLatin1String("arm64") || cpu == QLatin1String("aarch64"))
        arch = QStringLiteral("linuxarm64");
    else
        return {};
    // The versioned names (n8.1, n9.0) sit in the same release; this one matches a single asset.
    return QStringLiteral("ffmpeg-master-latest-%1-lgpl-shared.tar.xz").arg(arch);
}

Extracted extract(const QString &archivePath, const QString &folder, const std::atomic_bool *cancel)
{
    std::unique_ptr<struct archive, ReadCloser> a(archive_read_new());
    archive_read_support_filter_all(a.get());
    archive_read_support_format_tar(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(archivePath).constData(), 64 * 1024) != ARCHIVE_OK)
        return {false, tr("Couldn't open the download: %1").arg(QString::fromLocal8Bit(archive_error_string(a.get())))};

    QDir().mkpath(folder + QStringLiteral("/bin"));
    QDir().mkpath(folder + QStringLiteral("/lib"));
    qint64 total = 0;
    bool binary = false;
    struct archive_entry *entry = nullptr;
    int status = ARCHIVE_OK;
    while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK || status == ARCHIVE_WARN) {
        if (cancel && cancel->load())
            return {true, {}};
        const QString path = QString::fromUtf8(archive_entry_pathname(entry));
        const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
        if (path.startsWith(u'/') || parts.contains(QStringLiteral("..")))
            return {false, tr("The download holds a path that leads outside its folder, so it was not unpacked")};
        const Part part = partOf(parts);
        if (part == Part::Skip)
            continue;

        const QString name = parts.last();
        const QString target = folder + u'/' + (part == Part::License ? QString() : parts[1] + u'/') + name;
        if (archive_entry_hardlink(entry))
            return {false, tr("%1 in the download is a link, not a file, so it was not unpacked").arg(name)};
        if (archive_entry_filetype(entry) == AE_IFLNK) {
            // The libraries' version links (libavutil.so.61 -> libavutil.so.61.20.100), kept beside their targets.
            const QString link = QString::fromUtf8(archive_entry_symlink(entry));
            if (part != Part::Library || link.isEmpty() || link.contains(u'/'))
                return {false, tr("%1 in the download is a link, not a file, so it was not unpacked").arg(name)};
            if (!QFile::link(link, target))
                return {false, tr("Couldn't write %1").arg(target)};
            continue;
        }
        if (archive_entry_filetype(entry) != AE_IFREG)
            return {false, tr("%1 in the download is not a file, so it was not unpacked").arg(name)};
        if (archive_entry_size(entry) > FileLimit)
            return {false, tr("%1 in the download is too large").arg(name)};

        QFile out(target);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return {false, tr("Couldn't write %1: %2").arg(out.fileName(), out.errorString())};
        char block[64 * 1024];
        for (;;) {
            if (cancel && cancel->load())
                return {true, {}};
            const la_ssize_t n = archive_read_data(a.get(), block, sizeof block);
            if (n == 0)
                break;
            total += qMax<la_ssize_t>(n, 0);
            if (n < 0 || total > TotalLimit || out.write(block, n) != n) {
                const QString error = n < 0 ? QString::fromLocal8Bit(archive_error_string(a.get()))
                                            : total > TotalLimit ? tr("it is too large") : out.errorString();
                return {false, tr("Couldn't unpack %1: %2").arg(name, error)};
            }
        }
        if (!out.flush())
            return {false, tr("Couldn't write %1: %2").arg(name, out.errorString())};
        out.setPermissions(part == Part::License ? QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                                                        | QFileDevice::ReadOther
                                                  : QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                                        | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                                        | QFileDevice::ReadOther | QFileDevice::ExeOther);
        binary = binary || part == Part::Binary;
    }
    if (status != ARCHIVE_EOF)
        return {false, tr("Couldn't read the download: %1").arg(QString::fromLocal8Bit(archive_error_string(a.get())))};
    if (!binary)
        return {false, tr("The download has no bin/ffmpeg")};
    return {};
}
}

FfmpegTool *FfmpegTool::instance()
{
    static FfmpegTool *tool = [] {
        auto *t = new FfmpegTool(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(t, QJSEngine::CppOwnership);
        return t;
    }();
    return tool;
}

QString FfmpegTool::ownPath(const QString &toolsDir)
{
    const QString own = releaseFile(toolsDir);
    return QFileInfo(own).isFile() && QFileInfo(own).isExecutable() ? own : QString();
}

QString FfmpegTool::resolve(const QString &toolsDir)
{
    // LATTIS_FFMPEG=none shows the missing state in screenshots, where PATH can't be changed.
    if (qEnvironmentVariable("LATTIS_FFMPEG") == u"none")
        return {};
    const QString own = ownPath(toolsDir);
    return own.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("ffmpeg")) : own;
}

FfmpegTool::FfmpegTool(const Setup &setup, QObject *parent)
    : QObject(parent)
    , m_setup(setup)
{
    connect(&m_extract, &QFutureWatcher<FfmpegInstall::Extracted>::finished, this, [this] { extracted(m_serial); });
    if (auto *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &FfmpegTool::cancel);
    refresh();
}

FfmpegTool::~FfmpegTool()
{
    abandon();
}

QString FfmpegTool::state() const
{
    switch (m_state) {
    case State::Idle: return QStringLiteral("idle");
    case State::Checking: return QStringLiteral("checking");
    case State::Downloading: return QStringLiteral("downloading");
    case State::Installing: return QStringLiteral("installing");
    case State::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("idle");
}

QNetworkAccessManager *FfmpegTool::network()
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    return m_network;
}

QString FfmpegTool::ownFolder() const
{
    return QDir(m_setup.toolsDir).filePath(QStringLiteral("ffmpeg"));
}

QString FfmpegTool::partialFolder() const
{
    return QDir(m_setup.toolsDir).filePath(QStringLiteral(".ffmpeg.partial"));
}

void FfmpegTool::setState(State state)
{
    m_state = state;
    emit changed();
}

void FfmpegTool::refresh()
{
    m_path = resolve(m_setup.toolsDir);
    m_ownCopy = !ownPath(m_setup.toolsDir).isEmpty();
    emit changed();
}

void FfmpegTool::install()
{
    if (m_state != State::Idle && m_state != State::Failed)
        return;
    const QString asset = m_assetName.isEmpty() ? FfmpegInstall::assetName(QSysInfo::currentCpuArchitecture()) : m_assetName;
    m_error.clear();
    m_progress = -1;
    m_size = 0;
    const quint64 serial = ++m_serial;
    if (asset.isEmpty()) {
        fail(tr("Lattis can't download ffmpeg for this computer's processor (%1)").arg(QSysInfo::currentCpuArchitecture()));
        return;
    }
    const auto source = ReleaseSources::parse(ReleaseProject);
    QNetworkRequest request(m_releaseUrl.isEmpty() ? source->apiUrl() : m_releaseUrl);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QCoreApplication::applicationName() + u'/' + QCoreApplication::applicationVersion());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    setState(State::Checking);
    QNetworkReply *reply = network()->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        gotRelease(serial, reply->readAll(), reply->error() == QNetworkReply::NoError ? QString() : reply->errorString());
    });
}

void FfmpegTool::gotRelease(quint64 serial, const QByteArray &json, const QString &networkError)
{
    if (serial != m_serial || m_state != State::Checking)
        return;
    const auto source = ReleaseSources::parse(ReleaseProject);
    QString error;
    const auto release = ReleaseSources::parseRelease(*source, json, &error);
    if (!release) {
        fail(tr("Couldn't check the latest ffmpeg release: %1").arg(json.isEmpty() && !networkError.isEmpty() ? networkError : error));
        return;
    }
    const QString name = m_assetName.isEmpty() ? FfmpegInstall::assetName(QSysInfo::currentCpuArchitecture()) : m_assetName;
    const ReleaseAsset *asset = ReleaseSources::pickAsset(*release, name);
    if (!asset) {
        fail(tr("The latest ffmpeg release has no download named %1").arg(name));
        return;
    }
    // An asset without a checksum is refused: the download is run as a program.
    if (asset->sha256.isEmpty()) {
        fail(tr("The latest ffmpeg release lists no checksum for %1, so it was not downloaded").arg(name));
        return;
    }
    m_size = asset->size;
    QDir().mkpath(m_setup.toolsDir);
    m_archive = QDir(m_setup.toolsDir).filePath(QStringLiteral(".%1.partial").arg(asset->name));
    auto *transfer = new AppImageDownload(network(), *asset, {}, m_setup.toolsDir, this);
    m_download = transfer;
    transfer->setDownloadOnly(true);
    transfer->setRelaxed(m_relaxed);
    connect(transfer, &AppImageDownload::progress, this, [this, serial](qint64 done, qint64 total) {
        if (serial != m_serial)
            return;
        const double progress = total > 0 ? double(done) / double(total) : -1;
        if (qAbs(progress - m_progress) >= 0.005 || progress < 0 || progress == 1) {
            m_progress = progress;
            emit changed();
        }
    });
    connect(transfer, &AppImageDownload::finished, this,
            [this, serial](const AppImageDownload::Result &result) { downloadFinished(serial, result); });
    setState(State::Downloading);
    transfer->start();
}

void FfmpegTool::downloadFinished(quint64 serial, const AppImageDownload::Result &result)
{
    if (m_download) {
        m_download->deleteLater();
        m_download = nullptr;
    }
    if (serial != m_serial || m_state != State::Downloading)
        return;
    if (!result.ok) {
        if (result.cancelled)
            abandon();
        else
            fail(result.error);
        return;
    }
    m_archive = result.path;
    m_progress = -1;
    setState(State::Installing);
    QDir(partialFolder()).removeRecursively();
    m_cancelExtract = false;
    m_extract.setFuture(QtConcurrent::run(FfmpegInstall::extract, m_archive, partialFolder(), &m_cancelExtract));
}

void FfmpegTool::extracted(quint64 serial)
{
    QFile::remove(m_archive);
    if (serial != m_serial || m_state != State::Installing)
        return;
    const FfmpegInstall::Extracted result = m_extract.result();
    if (result.cancelled) {
        abandon();
        return;
    }
    if (!result.error.isEmpty()) {
        fail(result.error);
        return;
    }
    verify(serial);
}

// A build that won't start (a missing system library, the wrong CPU) is caught here, before it replaces anything.
void FfmpegTool::verify(quint64 serial)
{
    auto *process = new QProcess(this);
    m_check = process;
    if (m_setup.environment)
        process->setEnvironment(m_setup.environment());
    process->setProgram(QDir(partialFolder()).filePath(QStringLiteral("bin/ffmpeg")));
    process->setArguments({QStringLiteral("-version")});
    connect(process, &QProcess::finished, this, [this, serial] { verified(serial); });
    connect(process, &QProcess::errorOccurred, this, [this, serial](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            verified(serial);
    });
    QTimer::singleShot(10000, process, [process] {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });
    process->start();
}

void FfmpegTool::verified(quint64 serial)
{
    QProcess *process = m_check;
    if (serial != m_serial || m_state != State::Installing || !process)
        return;
    m_check = nullptr;
    process->deleteLater();
    const bool ok = process->error() != QProcess::FailedToStart && process->exitStatus() == QProcess::NormalExit
                    && process->exitCode() == 0 && process->readAllStandardOutput().startsWith("ffmpeg version");
    if (!ok) {
        const QString detail = process->error() == QProcess::FailedToStart
                                   ? process->errorString()
                                   : QString::fromLocal8Bit(process->readAllStandardError()).section(u'\n', 0, 0).trimmed();
        fail(tr("The downloaded ffmpeg doesn't start%1").arg(detail.isEmpty() ? QString() : QStringLiteral(": ") + detail));
        return;
    }
    QDir(ownFolder()).removeRecursively();
    if (!QDir().rename(partialFolder(), ownFolder())) {
        fail(tr("Couldn't put ffmpeg in place in %1").arg(m_setup.toolsDir));
        return;
    }
    refresh();
    setState(State::Idle);
}

void FfmpegTool::fail(const QString &error)
{
    abandon();
    m_error = error;
    qCWarning(lcFfmpeg).noquote() << error;
    setState(State::Failed);
}

// Stops whatever runs and deletes what it wrote; the state is left to the caller.
void FfmpegTool::abandon()
{
    ++m_serial;
    if (m_reply)
        m_reply->abort();
    if (m_download) {
        m_download->disconnect(this);
        delete m_download;
    }
    m_cancelExtract = true;
    m_extract.waitForFinished();
    if (m_check) {
        m_check->disconnect(this);
        m_check->kill();
        m_check->waitForFinished(1000);
        delete m_check;
        m_check = nullptr;
    }
    if (!m_archive.isEmpty())
        QFile::remove(m_archive);
    m_archive.clear();
    if (!m_setup.toolsDir.isEmpty())
        QDir(partialFolder()).removeRecursively();
}

void FfmpegTool::cancel()
{
    if (m_state == State::Idle)
        return;
    abandon();
    m_error.clear();
    m_progress = -1;
    m_size = 0;
    setState(State::Idle);
}

void FfmpegTool::remove()
{
    if (m_state != State::Idle && m_state != State::Failed)
        return;
    if (QFileInfo::exists(ownFolder()) && !QDir(ownFolder()).removeRecursively()) {
        m_error = tr("Couldn't delete %1").arg(ownFolder());
        refresh();
        setState(State::Failed);
        return;
    }
    m_error.clear();
    refresh();
    setState(State::Idle);
}
