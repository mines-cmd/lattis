#include "savesmanifest.h"
#include "apppaths.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

Q_LOGGING_CATEGORY(lcSavesManifest, "launcher.saves")

namespace SavesManifest {
namespace {
QString manifestPath(const QString &dir) { return QDir(dir).filePath(QStringLiteral("manifest.yaml")); }
QString indexPath(const QString &dir) { return QDir(dir).filePath(QStringLiteral("index.bin")); }
QString metaPath(const QString &dir) { return QDir(dir).filePath(QStringLiteral("meta.json")); }

bool writeMeta(const QString &dir, const Meta &meta)
{
    QSaveFile file(metaPath(dir));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonObject json{{QStringLiteral("etag"), meta.etag},
                           {QStringLiteral("fetched"), meta.fetched.toString(Qt::ISODate)},
                           {QStringLiteral("checked"), meta.checked.toString(Qt::ISODate)}};
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    return file.commit();
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

struct Reply
{
    int status = 0; // 0 = no HTTP status (a file:// URL)
    QByteArray body;
    QString etag;
    QString error;
};

Reply get(const QString &url, const QString &etag, int timeoutMs)
{
    QNetworkAccessManager network;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    if (!etag.isEmpty())
        request.setRawHeader("If-None-Match", etag.toUtf8());
    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    Reply out;
    out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (out.status == 304) {
        reply->deleteLater();
        return out;
    }
    if (reply->error() != QNetworkReply::NoError) {
        out.error = reply->errorString();
        qCDebug(lcSavesManifest).noquote() << "could not download" << url << out.error;
    } else {
        out.body = reply->readAll();
        out.etag = QString::fromUtf8(reply->rawHeader("ETag"));
    }
    reply->deleteLater();
    return out;
}
}

QString defaultDir()
{
    return QDir(AppPaths::stateDir()).filePath(QStringLiteral("saves-manifest"));
}

QString url()
{
    const QString url = qEnvironmentVariable("LATTIS_SAVES_MANIFEST_URL");
    return url.isEmpty() ? QString::fromLatin1(kDefaultUrl) : url;
}

Meta readMeta(const QString &dir)
{
    Meta meta;
    QFile file(metaPath(dir));
    if (!file.open(QIODevice::ReadOnly))
        return meta;
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    meta.etag = json.value(QStringLiteral("etag")).toString();
    meta.fetched = QDateTime::fromString(json.value(QStringLiteral("fetched")).toString(), Qt::ISODate);
    meta.checked = QDateTime::fromString(json.value(QStringLiteral("checked")).toString(), Qt::ISODate);
    return meta;
}

bool checkDue(const Meta &meta, const QDateTime &now)
{
    return !meta.checked.isValid() || meta.checked.secsTo(now) > 24 * 60 * 60;
}

UpdateResult update(const QString &dir, const QString &url, bool force, int timeoutMs)
{
    UpdateResult result;
    QDir().mkpath(dir);
    Meta meta = readMeta(dir);
    const bool haveFile = QFile::exists(manifestPath(dir));
    const QDateTime now = QDateTime::currentDateTimeUtc();

    // An index in an older format is made again from the file on disk, without the network.
    bool haveIndex = loadIndex(dir).has_value();
    if (haveFile && !haveIndex) {
        QFile file(manifestPath(dir));
        if (file.open(QIODevice::ReadOnly)) {
            const Parsed<SavesIndex> parsed = SavesIndexFile::parse(file.readAll());
            haveIndex = parsed.ok && SavesIndexFile::save(indexPath(dir), parsed.value);
        }
    }

    const bool conditional = haveFile && haveIndex && !force && !meta.etag.isEmpty();
    const Reply reply = get(url, conditional ? meta.etag : QString(), timeoutMs);

    if (reply.status == 304) {
        meta.checked = now;
        writeMeta(dir, meta);
        result.status = UpdateResult::Unchanged;
        return result;
    }
    if (!reply.error.isEmpty() || reply.body.isEmpty()) {
        result.error = reply.error.isEmpty() ? QStringLiteral("the reply was empty") : reply.error;
        return result;
    }

    const Parsed<SavesIndex> parsed = SavesIndexFile::parse(reply.body);
    result.warnings = parsed.warnings;
    if (!parsed.ok) {
        result.error = parsed.warnings.value(0, QStringLiteral("not a manifest"));
        return result;
    }
    if (!writeFile(manifestPath(dir), reply.body) || !SavesIndexFile::save(indexPath(dir), parsed.value)) {
        result.error = QStringLiteral("could not write to %1").arg(dir);
        return result;
    }
    meta.etag = reply.etag;
    meta.fetched = now;
    meta.checked = now;
    writeMeta(dir, meta);
    result.status = UpdateResult::Updated;
    return result;
}

std::optional<SavesIndex> loadIndex(const QString &dir)
{
    return SavesIndexFile::load(indexPath(dir));
}
}
