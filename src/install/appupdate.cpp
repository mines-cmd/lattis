#include "appupdate.h"

#include "releasesource.h"
#include "tomlsection.h"
#include "versions.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcAppUpdate, "launcher.appupdate")

AppUpdate::Setup AppUpdate::s_setup;

namespace {
constexpr qint64 ReleaseCacheSeconds = 3600;
constexpr qint64 DaySeconds = 24 * 3600;
const QString ReleaseProject = QStringLiteral("github:mines-cmd/lattis");
const QString AssetPattern = QStringLiteral("Lattis-*-x86_64.AppImage");

QString tr(const char *text)
{
    return QCoreApplication::translate("AppUpdate", text);
}

QString plainVersion(const QString &tag)
{
    QString t = tag.trimmed();
    if (t.startsWith(u'v') || t.startsWith(u'V'))
        t.remove(0, 1);
    return t;
}
}

namespace AppUpdates {
QString replaceBlocker(const QString &appImage, const QString &applicationsDir)
{
    const QFileInfo file(appImage);
    if (!file.isFile())
        return tr("The AppImage file isn't there");
    if (!QFileInfo(file.absolutePath()).isWritable())
        return tr("%1 can't be written to").arg(QDir::toNativeSeparators(file.absolutePath()));
    // The managers keep the file and its menu entry in step, and would put the old one back.
    const QString manager = managedBy(appImage, applicationsDir);
    if (!manager.isEmpty())
        return tr("It is managed by %1, which updates it").arg(manager);
    return {};
}

QString managedBy(const QString &appImage, const QString &applicationsDir)
{
    const struct
    {
        const char *pattern;
        const char *name;
    } managers[] = {{"gearlever_*.desktop", "Gear Lever"}, {"appimagekit_*.desktop", "AppImageLauncher"}};
    const QByteArray path = QFileInfo(appImage).absoluteFilePath().toUtf8();
    for (const auto &manager : managers) {
        const QDir dir(applicationsDir);
        for (const QString &entry : dir.entryList({QString::fromLatin1(manager.pattern)}, QDir::Files)) {
            QFile f(dir.filePath(entry));
            if (f.open(QIODevice::ReadOnly) && f.readAll().contains(path))
                return QString::fromLatin1(manager.name);
        }
    }
    return {};
}
}

void AppUpdate::setAppImage(const QString &appImage)
{
    m_setup.appImage = appImage;
    m_blocker = m_available ? AppUpdates::replaceBlocker(appImage, m_setup.applicationsDir) : QString();
    emit changed();
}

AppUpdate *AppUpdate::instance()
{
    static AppUpdate *update = [] {
        auto *u = new AppUpdate(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(u, QJSEngine::CppOwnership);
        return u;
    }();
    return update;
}

AppUpdate::AppUpdate(const Setup &setup, QObject *parent)
    : QObject(parent)
    , m_setup(setup)
{
    // The hooks stand in for the release and the running file, and only exist in fixture mode.
    if (!qEnvironmentVariableIsEmpty("LATTIS_INSTALL_FIXTURES")) {
        const QString release = qEnvironmentVariable("LATTIS_UPDATE_RELEASE");
        if (!release.isEmpty()) {
            m_releaseUrl = release.contains(QStringLiteral("://")) ? QUrl(release) : QUrl::fromLocalFile(release);
            m_relaxed = true;
        }
        const QString appImage = qEnvironmentVariable("LATTIS_UPDATE_APPIMAGE");
        if (!appImage.isEmpty()) {
            m_setup.appImage = appImage;
            m_setup.enabled = true;
        }
    }
    m_available = m_setup.enabled && !m_setup.appImage.isEmpty();
    if (m_available)
        m_blocker = AppUpdates::replaceBlocker(m_setup.appImage, m_setup.applicationsDir);
    loadWhatsNew();
    m_daily.setSingleShot(true);
    connect(&m_daily, &QTimer::timeout, this, &AppUpdate::dailyCheck);
    if (auto *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, [this] {
            if (m_state == State::Downloading)
                cancel();
        });
}

AppUpdate::~AppUpdate()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    delete m_download;
}

QString AppUpdate::state() const
{
    switch (m_state) {
    case State::Idle: return QStringLiteral("idle");
    case State::Checking: return QStringLiteral("checking");
    case State::Downloading: return QStringLiteral("downloading");
    case State::Ready: return QStringLiteral("ready");
    case State::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("idle");
}

bool AppUpdate::skipped() const
{
    return m_hasUpdate && m_setup.skippedVersion && m_setup.skippedVersion() == m_version;
}

QUrl AppUpdate::releasePage() const
{
    const QString base = QStringLiteral("https://github.com/") + ReleaseProject.mid(7) + QStringLiteral("/releases/");
    return QUrl(m_release.tag.isEmpty() ? base + QStringLiteral("latest") : base + QStringLiteral("tag/") + m_release.tag);
}

QString AppUpdate::runningGame() const
{
    return m_setup.runningGame ? m_setup.runningGame() : QString();
}

QString AppUpdate::currentVersion() const
{
    return m_currentVersion.isEmpty() ? QCoreApplication::applicationVersion() : m_currentVersion;
}

void AppUpdate::setCurrentVersion(const QString &version)
{
    m_currentVersion = version;
    loadWhatsNew();
}

void AppUpdate::setPace(qint64 bytes, int tickMs)
{
    m_paceBytes = bytes;
    m_paceTickMs = tickMs;
}

QNetworkAccessManager *AppUpdate::network()
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    return m_network;
}

QString AppUpdate::cachedReleasePath() const
{
    return QDir(m_setup.cacheDir).filePath(QStringLiteral("releases/app-update.json"));
}

QString AppUpdate::lastCheckPath() const
{
    return QDir(m_setup.stateDir).filePath(QStringLiteral("app-update.toml"));
}

void AppUpdate::setState(State state)
{
    m_state = state;
    emit changed();
}

void AppUpdate::fail(const QString &error, bool quiet)
{
    if (m_download) {
        m_download->deleteLater();
        m_download = nullptr;
    }
    m_progress = -1;
    if (quiet) {
        qCInfo(lcAppUpdate).noquote() << error;
        m_error.clear();
        setState(State::Idle);
        return;
    }
    m_error = error;
    setState(State::Failed);
}

void AppUpdate::check(bool quiet)
{
    if (!m_available || m_state == State::Checking || m_state == State::Downloading || m_state == State::Ready)
        return;
    m_error.clear();
    const quint64 serial = ++m_serial;

    // Only the answer from the real API is cached.
    if (m_releaseUrl.isEmpty()) {
        const QString cached = cachedReleasePath();
        const QFileInfo info(cached);
        QFile file(cached);
        if (info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < ReleaseCacheSeconds
            && file.open(QIODevice::ReadOnly)) {
            const QByteArray body = file.readAll();
            setState(State::Checking);
            QTimer::singleShot(0, this, [this, serial, body, quiet] { gotRelease(serial, body, {}, true, quiet); });
            return;
        }
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial, quiet] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        gotRelease(serial, reply->readAll(), reply->error() == QNetworkReply::NoError ? QString() : reply->errorString(),
                   false, quiet);
    });
}

void AppUpdate::gotRelease(quint64 serial, const QByteArray &json, const QString &networkError, bool fromCache,
                           bool quiet)
{
    if (serial != m_serial || m_state != State::Checking)
        return;
    const auto source = ReleaseSources::parse(ReleaseProject);
    QString error;
    const auto release = ReleaseSources::parseRelease(*source, json, &error);
    if (!release) {
        if (fromCache)
            QFile::remove(cachedReleasePath());
        fail(tr("Couldn't check for a newer Lattis: %1").arg(json.isEmpty() && !networkError.isEmpty() ? networkError : error),
             quiet);
        return;
    }
    if (!fromCache && m_releaseUrl.isEmpty()) {
        QDir().mkpath(QFileInfo(cachedReleasePath()).absolutePath());
        QSaveFile file(cachedReleasePath());
        if (file.open(QIODevice::WriteOnly) && file.write(json) == json.size())
            file.commit();
    }
    saveLastCheck();

    m_release = *release;
    m_version = plainVersion(release->tag);
    const ReleaseAsset *asset = ReleaseSources::pickAsset(*release, AssetPattern);
    m_asset = asset ? *asset : ReleaseAsset();
    m_hasUpdate = Versions::newer(release->tag, currentVersion());
    if (m_hasUpdate && !asset) {
        m_hasUpdate = false;
        fail(tr("Lattis %1 has no AppImage in its release").arg(m_version), quiet);
        return;
    }
    setState(State::Idle);
}

void AppUpdate::update()
{
    if (!m_available || !m_hasUpdate || (m_state != State::Idle && m_state != State::Failed))
        return;
    m_error.clear();
    if (!m_blocker.isEmpty()) {
        fail(m_blocker);
        return;
    }
    // The file is run, so one without a checksum is refused.
    if (m_asset.sha256.isEmpty()) {
        fail(tr("The release lists no checksum for %1, so it was not downloaded").arg(m_asset.name));
        return;
    }
    const quint64 serial = ++m_serial;
    const QFileInfo target(m_setup.appImage);
    auto *transfer = new AppImageDownload(network(), m_asset, {}, target.absolutePath(), this);
    m_download = transfer;
    // The same path keeps every menu entry, the autostart entry and the Steam shortcut that point at the file working.
    transfer->setTargetName(target.fileName());
    // Replacing the running file is safe: the mounted image holds the old one open.
    transfer->setReplace(true);
    transfer->setRelaxed(m_relaxed);
    if (m_paceBytes > 0)
        transfer->setPace(m_paceBytes, m_paceTickMs);
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
    m_progress = -1;
    setState(State::Downloading);
    transfer->start();
}

void AppUpdate::downloadFinished(quint64 serial, const AppImageDownload::Result &result)
{
    if (m_download) {
        m_download->deleteLater();
        m_download = nullptr;
    }
    if (serial != m_serial || m_state != State::Downloading)
        return;
    m_progress = -1;
    if (result.cancelled) {
        setState(State::Idle);
        return;
    }
    if (!result.ok) {
        fail(result.error);
        return;
    }
    if (!result.replaced.isEmpty()) {
        QString problem;
        if (!m_setup.trash || !m_setup.trash(result.replaced, &problem))
            qCWarning(lcAppUpdate).noquote() << "can't trash the old AppImage" << result.replaced << problem;
        QDir().rmdir(QFileInfo(result.replaced).absolutePath());
    }
    saveUpdated();
    setState(State::Ready);
}

void AppUpdate::cancel()
{
    switch (m_state) {
    case State::Checking:
        ++m_serial;
        if (m_reply) {
            m_reply->disconnect(this);
            m_reply->abort();
        }
        setState(State::Idle);
        break;
    case State::Downloading:
        if (m_download)
            m_download->cancel();
        break;
    case State::Failed:
        m_error.clear();
        setState(State::Idle);
        break;
    default:
        break;
    }
}

void AppUpdate::skip()
{
    if (!m_hasUpdate || !m_setup.setSkippedVersion)
        return;
    m_setup.setSkippedVersion(m_version);
    emit changed();
}

void AppUpdate::restart()
{
    if (m_state != State::Ready || !m_setup.restart || !runningGame().isEmpty())
        return;
    QString problem;
    if (!m_setup.restart(&problem)) {
        m_error = tr("Couldn't start Lattis again: %1").arg(problem);
        emit changed();
    }
}

QString AppUpdate::updatedPath() const
{
    return QDir(m_setup.stateDir).filePath(QStringLiteral("app-updated.toml"));
}

QString AppUpdate::notesPath() const
{
    return QDir(m_setup.cacheDir).filePath(QStringLiteral("update-notes.md"));
}

// The marker waits for the start that runs the new version; after "Later" the old one still runs and leaves it.
void AppUpdate::saveUpdated()
{
    QDir().mkpath(m_setup.stateDir);
    QDir().mkpath(m_setup.cacheDir);
    QSaveFile notes(notesPath());
    if (notes.open(QIODevice::WriteOnly)) {
        notes.write(m_release.notes.toUtf8());
        notes.commit();
    }
    QSaveFile marker(updatedPath());
    if (marker.open(QIODevice::WriteOnly)) {
        marker.write(QStringLiteral("version = \"%1\"\n").arg(m_version).toUtf8());
        marker.commit();
    }
}

void AppUpdate::loadWhatsNew()
{
    m_whatsNewVersion.clear();
    m_whatsNewNotes.clear();
    QFile file(updatedPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    QString version;
    try {
        const toml::table table = toml::parse(std::string_view(file.readAll().constData()));
        if (const auto *text = table["version"].as_string())
            version = toQString(text->get());
    } catch (const toml::parse_error &) {
    }
    file.close();
    const QString current = currentVersion();
    if (!version.isEmpty() && Versions::newer(version, current))
        return;
    if (version.isEmpty() || Versions::newer(current, version)) {
        QFile::remove(updatedPath());
        return;
    }
    m_whatsNewVersion = version;
    QFile notes(notesPath());
    if (notes.open(QIODevice::ReadOnly))
        m_whatsNewNotes = QString::fromUtf8(notes.readAll());
}

void AppUpdate::dismissWhatsNew()
{
    if (m_whatsNewVersion.isEmpty())
        return;
    QFile::remove(updatedPath());
    m_whatsNewVersion.clear();
    emit changed();
}

void AppUpdate::saveLastCheck()
{
    QDir().mkpath(m_setup.stateDir);
    QSaveFile file(lastCheckPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QStringLiteral("last_check = \"%1\"\n").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)).toUtf8());
    file.commit();
}

void AppUpdate::startDailyCheck(int delayMs)
{
    if (m_available)
        m_daily.start(delayMs);
}

void AppUpdate::dailyCheck()
{
    qint64 wait = DaySeconds;
    if (m_setup.autoCheck && m_setup.autoCheck()) {
        QDateTime last;
        QFile file(lastCheckPath());
        if (file.open(QIODevice::ReadOnly)) {
            try {
                const toml::table table = toml::parse(std::string_view(file.readAll().constData()));
                if (const auto *text = table["last_check"].as_string())
                    last = QDateTime::fromString(toQString(text->get()), Qt::ISODate);
            } catch (const toml::parse_error &) {
            }
        }
        const qint64 age = last.isValid() ? last.secsTo(QDateTime::currentDateTimeUtc()) : DaySeconds;
        if (age >= DaySeconds)
            check(true);
        else
            wait = DaySeconds - age;
    }
    m_daily.start(int(qMin<qint64>(wait, DaySeconds) * 1000));
}
