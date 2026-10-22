#include "appimageinstall.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>

#include <archive.h>
#include <archive_entry.h>

#include <memory>

namespace AppImageInstall {
namespace {
constexpr qint64 ExtractedLimit = qint64(2) * 1024 * 1024 * 1024;

QString tr(const char *text)
{
    return QCoreApplication::translate("AppImageInstall", text);
}

struct ReadCloser
{
    void operator()(struct archive *a) const { archive_read_free(a); }
};
}

QString targetFolder(const QStringList &applicationsDirs, const QString &home)
{
    for (const QString &dir : applicationsDirs) {
        if (QFileInfo(dir).isDir())
            return dir;
    }
    return home + QStringLiteral("/Applications");
}

bool isArchive(const QString &fileName)
{
    static const QStringList endings = {QStringLiteral(".zip"),     QStringLiteral(".tar"),     QStringLiteral(".tar.gz"),
                                        QStringLiteral(".tgz"),     QStringLiteral(".tar.xz"),  QStringLiteral(".tar.zst"),
                                        QStringLiteral(".tar.bz2")};
    return std::any_of(endings.cbegin(), endings.cend(),
                       [&](const QString &e) { return fileName.endsWith(e, Qt::CaseInsensitive); });
}

Extracted extractOne(const QString &archivePath, const QString &inside, const QString &folder,
                     const std::atomic_bool *cancel)
{
    std::unique_ptr<struct archive, ReadCloser> a(archive_read_new());
    archive_read_support_filter_all(a.get());
    archive_read_support_format_zip(a.get());
    archive_read_support_format_tar(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(archivePath).constData(), 64 * 1024) != ARCHIVE_OK)
        return {{}, tr("Couldn't open the download: %1").arg(QString::fromLocal8Bit(archive_error_string(a.get())))};

    struct archive_entry *entry = nullptr;
    int status = ARCHIVE_OK;
    while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK || status == ARCHIVE_WARN) {
        if (cancel && cancel->load())
            return {{}, {}};
        const QString path = QString::fromUtf8(archive_entry_pathname(entry));
        const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
        if (path.startsWith(u'/') || parts.contains(QStringLiteral("..")))
            return {{}, tr("The download holds a path that leads outside its folder, so it was not unpacked")};
        if (parts.isEmpty() || !ReleaseSources::matches(parts.last(), inside))
            continue;
        if (archive_entry_filetype(entry) != AE_IFREG || archive_entry_hardlink(entry) || archive_entry_symlink(entry))
            return {{}, tr("%1 in the download is a link, not a file, so it was not unpacked").arg(parts.last())};
        if (archive_entry_size(entry) > ExtractedLimit)
            return {{}, tr("%1 in the download is too large").arg(parts.last())};

        const QString name = parts.last();
        QFile out(QDir(folder).filePath(QStringLiteral(".%1.partial").arg(name)));
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return {{}, tr("Couldn't write %1: %2").arg(out.fileName(), out.errorString())};
        char block[64 * 1024];
        for (;;) {
            if (cancel && cancel->load()) {
                out.close();
                out.remove();
                return {{}, {}};
            }
            const la_ssize_t n = archive_read_data(a.get(), block, sizeof block);
            if (n == 0)
                break;
            if (n < 0 || out.write(block, n) != n) {
                const QString error = n < 0 ? QString::fromLocal8Bit(archive_error_string(a.get())) : out.errorString();
                out.close();
                out.remove();
                return {{}, tr("Couldn't unpack %1: %2").arg(name, error)};
            }
        }
        if (!out.flush()) {
            out.remove();
            return {{}, tr("Couldn't write %1: %2").arg(name, out.errorString())};
        }
        return {name, {}};
    }
    if (status != ARCHIVE_EOF)
        return {{}, tr("Couldn't read the download: %1").arg(QString::fromLocal8Bit(archive_error_string(a.get())))};
    return {{}, tr("The download has no file matching %1").arg(inside)};
}
}

AppImageDownload::AppImageDownload(QNetworkAccessManager *network, const ReleaseAsset &asset, const QString &inside,
                                   const QString &folder, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_asset(asset)
    , m_inside(inside)
    , m_folder(folder)
{
    connect(&m_paceTimer, &QTimer::timeout, this, &AppImageDownload::drain);
    connect(&m_extract, &QFutureWatcher<AppImageInstall::Extracted>::finished, this, &AppImageDownload::extracted);
}

AppImageDownload::~AppImageDownload()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    if (m_reply && !m_ended)
        m_reply->deleteLater();
    m_cancelExtract = true;
    m_extract.waitForFinished();
    if (!m_ended) {
        m_file.close();
        QFile::remove(partialPath(m_asset.name));
    }
}

void AppImageDownload::setPace(qint64 bytes, int tickMs)
{
    m_paceBytes = bytes;
    m_paceTimer.setInterval(tickMs);
}

QString AppImageDownload::partialPath(const QString &name) const
{
    return QDir(m_folder).filePath(QStringLiteral(".%1.partial").arg(name));
}

void AppImageDownload::start()
{
    const QString scheme = m_asset.url.scheme();
    if (scheme != QLatin1String("https") && !(m_relaxed && (scheme == QLatin1String("http") || scheme == QLatin1String("file")))) {
        QTimer::singleShot(0, this, [this] { fail(tr("The download is not over HTTPS, so it was refused")); });
        return;
    }
    if (!QDir().mkpath(m_folder)) {
        QTimer::singleShot(0, this, [this] { fail(tr("Couldn't make the folder %1").arg(m_folder)); });
        return;
    }
    m_file.setFileName(partialPath(m_asset.name));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTimer::singleShot(0, this, [this] { fail(tr("Couldn't write %1: %2").arg(m_file.fileName(), m_file.errorString())); });
        return;
    }

    QNetworkRequest request(m_asset.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    m_reply = m_network->get(request);
    if (m_paceBytes > 0)
        m_paceTimer.start();
    else
        connect(m_reply, &QNetworkReply::readyRead, this, &AppImageDownload::drain);
    connect(m_reply, &QNetworkReply::finished, this, &AppImageDownload::replyFinished);
    emit progress(0, m_asset.size);
}

void AppImageDownload::cancel()
{
    if (m_ended)
        return;
    m_cancelled = true;
    m_cancelExtract = true;
    m_paceTimer.stop();
    if (m_reply && !m_replyDone) {
        m_reply->abort();
        return;
    }
    if (m_stage == Stage::Downloading)
        end({false, true, {}, {}, {}});
}

void AppImageDownload::drain()
{
    if (m_ended || m_stage != Stage::Downloading || !m_reply)
        return;
    while (m_reply->bytesAvailable() > 0) {
        const QByteArray bytes = m_reply->read(m_paceBytes > 0 ? m_paceBytes : 1024 * 1024);
        if (bytes.isEmpty())
            break;
        if (m_file.write(bytes) != bytes.size()) {
            fail(tr("Couldn't write %1: %2").arg(m_file.fileName(), m_file.errorString()));
            return;
        }
        m_hash.addData(bytes);
        m_done += bytes.size();
        if (m_paceBytes > 0)
            break;
    }
    qint64 total = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    if (total <= 0)
        total = m_asset.size;
    emit progress(m_done, total);
    if (m_replyDone && m_reply->bytesAvailable() == 0)
        downloaded();
}

void AppImageDownload::replyFinished()
{
    m_replyDone = true;
    m_paceTimer.stop();
    if (m_cancelled) {
        end({false, true, {}, {}, {}});
        return;
    }
    if (m_reply->error() != QNetworkReply::NoError) {
        fail(tr("The download failed: %1").arg(m_reply->errorString()));
        return;
    }
    if (m_paceBytes > 0 && m_reply->bytesAvailable() > 0) {
        m_paceTimer.start();
        return;
    }
    drain();
    if (m_stage == Stage::Downloading && !m_ended && m_reply->bytesAvailable() == 0)
        downloaded();
}

void AppImageDownload::downloaded()
{
    if (m_stage != Stage::Downloading || m_ended)
        return;
    m_paceTimer.stop();
    m_file.close();
    const QString sha = QString::fromLatin1(m_hash.result().toHex());
    if (m_asset.size > 0 && m_done != m_asset.size) {
        fail(tr("The download was cut short (%1 of %2 bytes)").arg(m_done).arg(m_asset.size));
        return;
    }
    if (!m_asset.sha256.isEmpty() && sha != m_asset.sha256) {
        fail(tr("The download doesn't match the checksum the release lists for it, so it was deleted"));
        return;
    }
    if (AppImageInstall::isArchive(m_asset.name)) {
        m_stage = Stage::Extracting;
        emit stageChanged();
        m_extract.setFuture(QtConcurrent::run(AppImageInstall::extractOne, partialPath(m_asset.name), m_inside,
                                              m_folder, &m_cancelExtract));
        return;
    }
    install(partialPath(m_asset.name), m_asset.name);
}

void AppImageDownload::extracted()
{
    QFile::remove(partialPath(m_asset.name));
    if (m_cancelled) {
        end({false, true, {}, {}, {}});
        return;
    }
    const AppImageInstall::Extracted result = m_extract.result();
    if (!result.error.isEmpty() || result.name.isEmpty()) {
        fail(result.error.isEmpty() ? tr("Couldn't unpack the download") : result.error);
        return;
    }
    install(partialPath(result.name), result.name);
}

void AppImageDownload::install(const QString &partial, const QString &name)
{
    m_stage = Stage::Installing;
    emit stageChanged();
    const QString target = QDir(m_folder).filePath(name);
    QString aside;
    if (QFileInfo::exists(target)) {
        if (!m_replace) {
            QFile::remove(partial);
            fail(tr("%1 is already in %2").arg(name, m_folder));
            return;
        }
        const QString asideFolder = QDir(m_folder).filePath(QStringLiteral(".%1.replaced").arg(name));
        aside = QDir(asideFolder).filePath(name);
        if (!QDir().mkpath(asideFolder) || (QFileInfo::exists(aside) && !QFile::remove(aside))
            || !QFile::rename(target, aside)) {
            QFile::remove(partial);
            fail(tr("Couldn't move the old %1 aside").arg(name));
            return;
        }
    }
    QFile file(partial);
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                             | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther
                             | QFileDevice::ExeOther)
        || !file.rename(target)) {
        const QString error = file.errorString();
        QFile::remove(partial);
        if (!aside.isEmpty()) {
            QFile::rename(aside, target);
            QDir().rmdir(QFileInfo(aside).absolutePath());
        }
        fail(tr("Couldn't put %1 in place: %2").arg(name, error));
        return;
    }
    end({true, false, target, {}, QString::fromLatin1(m_hash.result().toHex()), aside});
}

void AppImageDownload::fail(const QString &error)
{
    if (m_ended)
        return;
    m_file.close();
    QFile::remove(partialPath(m_asset.name));
    if (m_reply && !m_replyDone) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    end({false, false, {}, error, {}});
}

void AppImageDownload::end(const Result &result)
{
    if (m_ended)
        return;
    m_ended = true;
    m_paceTimer.stop();
    m_file.close();
    if (!result.ok)
        QFile::remove(partialPath(m_asset.name));
    if (m_reply)
        m_reply->deleteLater();
    emit finished(result);
}
