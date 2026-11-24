#include "romdbfetch.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

Q_LOGGING_CATEGORY(lcRomDbFetch, "launcher.romdb")

namespace RomDbFetch {
namespace {
bool s_enabled = false;
// The largest real database is 2 MB.
constexpr qint64 kMaxBytes = qint64(16) * 1024 * 1024;
}

void setEnabled(bool enabled)
{
    s_enabled = enabled;
}

QString baseUrl()
{
    QString url = qEnvironmentVariable("LATTIS_ROMDB_URL");
    if (url.isEmpty())
        url = QString::fromLatin1(kDefaultUrl);
    return url.endsWith(u'/') ? url : url + u'/';
}

bool download(const QString &base, const QString &platform, const QString &path, int timeoutMs,
              const std::function<bool()> &cancelled)
{
    if (cancelled && cancelled())
        return false;
    const QString file = RomDb::fileOf(platform);
    if (file.isEmpty())
        return false;
    // The canned-answers mode of the metadata lookup never reaches the network.
    if (qEnvironmentVariableIsSet("LATTIS_METADATA_FIXTURES") && !qEnvironmentVariableIsSet("LATTIS_ROMDB_URL"))
        return false;

    QNetworkAccessManager network;
    QNetworkRequest request{QUrl(base + file + QStringLiteral(".tsv"))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = network.get(request);
    bool tooLarge = false;
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [&](qint64 received, qint64 total) {
        if (!tooLarge && (received > kMaxBytes || total > kMaxBytes)) {
            tooLarge = true;
            qCDebug(lcRomDbFetch).noquote() << "stopped" << request.url().toString() << "after" << received
                                            << "bytes: the limit is" << kMaxBytes;
            reply->abort();
        }
    });
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    // The scan's thread sets the flag; nothing here can signal this loop from outside.
    QTimer poll;
    if (cancelled) {
        QObject::connect(&poll, &QTimer::timeout, reply, [&] {
            if (cancelled())
                reply->abort();
        });
        poll.start(50);
    }
    loop.exec();

    const QByteArray body = reply->error() == QNetworkReply::NoError && !tooLarge ? reply->readAll() : QByteArray();
    if (reply->error() != QNetworkReply::NoError && !tooLarge)
        qCDebug(lcRomDbFetch).noquote() << "could not download" << request.url().toString() << reply->errorString();
    reply->deleteLater();
    // A page that isn't a database (an error page with a 200) parses to nothing.
    if (RomDb::parse(body).isEmpty())
        return false;

    QFile out(path + QStringLiteral(".partial"));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(body) != body.size()) {
        out.remove();
        return false;
    }
    out.close();
    QFile::remove(path);
    return out.rename(path);
}

RomDbOptions options(const QString &cacheDir, bool allowDownload, std::function<bool()> cancelled)
{
    RomDbOptions out;
    out.cancelled = cancelled;
    if (cacheDir.isEmpty())
        return out;
    out.dir = QDir(cacheDir).filePath(QStringLiteral("romdb"));
    if (allowDownload && s_enabled) {
        out.download = [base = baseUrl(), cancelled](const QString &platform, const QString &path) {
            return download(base, platform, path, 20000, cancelled);
        };
    }
    return out;
}
}
