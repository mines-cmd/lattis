#include "installer.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(lcInstall, "launcher.install")

Installer::Setup Installer::s_setup;

namespace {
constexpr qint64 FixtureAssetSize = 2 * 1024 * 1024;
constexpr int FixtureSteps = 40;
}

Installer *Installer::instance()
{
    static Installer *installer = [] {
        auto *i = new Installer(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(i, QJSEngine::CppOwnership);
        return i;
    }();
    return installer;
}

Installer::Installer(const Setup &setup, QObject *parent)
    : QObject(parent)
    , m_setup(setup)
    , m_fixturesDir(qEnvironmentVariable("LATTIS_INSTALL_FIXTURES"))
{
    m_changed.setSingleShot(true);
    m_changed.setInterval(80);
    connect(&m_changed, &QTimer::timeout, this, &Installer::rebuild);
    if (auto *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this] {
            const QStringList ids = m_jobs.keys();
            for (const QString &id : ids)
                cancel(id);
        });
    }
    refresh();
}

Installer::~Installer() = default;

QNetworkAccessManager *Installer::network()
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    return m_network;
}

QString Installer::stateName(State state)
{
    switch (state) {
    case State::Idle: return QStringLiteral("idle");
    case State::Checking: return QStringLiteral("checking");
    case State::Downloading: return QStringLiteral("downloading");
    case State::Extracting: return QStringLiteral("extracting");
    case State::Installing: return QStringLiteral("installing");
    case State::Failed: return QStringLiteral("failed");
    }
    return {};
}

void Installer::refresh()
{
    m_records = InstallRecords::read(InstallRecords::path(m_setup.stateDir)).value;
    rebuild();
}

void Installer::changed()
{
    if (!m_changed.isActive())
        m_changed.start();
}

void Installer::rebuild()
{
    QVariantMap out;
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const InstallLocations where = m_setup.locations ? m_setup.locations() : InstallLocations();
    const QSet<QString> userFlatpaks = where.flatpakDirs.isEmpty() ? QSet<QString>()
                                                                   : Emulators::installedFlatpaks({where.flatpakDirs.first()});
    for (const EmulatorDef &def : defs) {
        const bool appImage = !def.release.isEmpty() && !def.releaseAsset.isEmpty();
        if (!appImage && !m_jobs.contains(def.id) && !m_records.contains(def.id))
            continue;
        const Job job = m_jobs.value(def.id);
        const InstallRecord record = m_records.value(def.id);
        const bool byApp = m_records.contains(def.id) && InstallRecords::stillThere(record, userFlatpaks);
        QStringList routes;
        if (appImage)
            routes.append(QStringLiteral("appimage"));
        QString source;
        if (const auto parsed = ReleaseSources::parse(def.release))
            source = parsed->label();
        out.insert(def.id, QVariantMap{
                               {QStringLiteral("state"), stateName(job.state)},
                               {QStringLiteral("progress"), job.progress},
                               {QStringLiteral("error"), job.error},
                               {QStringLiteral("routes"), routes},
                               {QStringLiteral("source"), source},
                               {QStringLiteral("installedByApp"), byApp},
                               {QStringLiteral("version"), byApp ? record.tag : QString()},
                               {QStringLiteral("updateVersion"), QString()},
                           });
    }
    if (out != m_emulators) {
        m_emulators = out;
        emit emulatorsChanged();
    }
}

void Installer::install(const QString &id, const QString &route)
{
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const EmulatorDef *def = Emulators::find(defs, id);
    if (!def)
        return;
    if (const auto it = m_jobs.constFind(id); it != m_jobs.cend() && it->state != State::Failed)
        return;
    Job job;
    job.serial = ++m_serial;
    job.emulatorName = def->name;
    m_jobs.insert(id, job);
    if (route != QLatin1String("appimage")) {
        fail(id, tr("%1 can't be installed that way yet").arg(def->name));
        return;
    }
    begin(*def);
}

void Installer::begin(const EmulatorDef &emulator)
{
    const QString id = emulator.id;
    Job &job = m_jobs[id];
    QString problem;
    const auto source = ReleaseSources::parse(emulator.release, &problem);
    if (emulator.release.isEmpty() || emulator.releaseAsset.isEmpty()) {
        fail(id, tr("%1 has no AppImage to download").arg(emulator.name));
        return;
    }
    if (!source) {
        fail(id, problem);
        return;
    }
    job.state = State::Checking;
    job.progress = -1;
    const quint64 serial = job.serial;
    rebuild();

    if (!m_fixturesDir.isEmpty()) {
        QString name = emulator.release;
        name.replace(u':', u'-').replace(u'/', u'-');
        const QString path = QDir(m_fixturesDir).filePath(name + QStringLiteral(".json"));
        QTimer::singleShot(m_fixtureDelayMs, this, [this, emulator, serial, path] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                gotRelease(emulator, serial, {}, tr("No canned release answer in %1").arg(path));
            else
                gotRelease(emulator, serial, file.readAll(), {});
        });
        return;
    }

    QNetworkRequest request(source->apiUrl());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QCoreApplication::applicationName() + u'/' + QCoreApplication::applicationVersion());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    QNetworkReply *reply = network()->get(request);
    job.reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, emulator, serial] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        const QByteArray body = reply->readAll();
        gotRelease(emulator, serial, body, reply->error() == QNetworkReply::NoError ? QString() : reply->errorString());
    });
}

void Installer::gotRelease(const EmulatorDef &emulator, quint64 serial, const QByteArray &json, const QString &networkError)
{
    const auto it = m_jobs.find(emulator.id);
    if (it == m_jobs.end() || it->serial != serial || it->state != State::Checking)
        return;
    const auto source = ReleaseSources::parse(emulator.release);
    QString error;
    const auto release = source ? ReleaseSources::parseRelease(*source, json, &error) : std::nullopt;
    if (!release) {
        const QString reason = json.isEmpty() && !networkError.isEmpty() ? networkError : error;
        fail(emulator.id, tr("Couldn't check the latest release: %1").arg(reason));
        return;
    }
    const ReleaseAsset *asset = ReleaseSources::pickAsset(*release, emulator.releaseAsset);
    if (!asset) {
        fail(emulator.id, tr("The latest release (%1) has no download named %2").arg(release->tag, emulator.releaseAsset));
        return;
    }
    it->release = *release;
    it->asset = *asset;
    download(emulator);
}

void Installer::useFixtureAsset(ReleaseAsset *asset) const
{
    const QString given = QDir(m_fixturesDir).filePath(asset->name);
    if (QFileInfo::exists(given)) {
        asset->url = QUrl::fromLocalFile(given);
        return;
    }
    // No canned file: a script that sleeps, which has no checksum to match.
    const QString folder = QDir(m_setup.cacheDir).filePath(QStringLiteral("install-fixtures"));
    QDir().mkpath(folder);
    const QString path = QDir(folder).filePath(asset->name);
    const qint64 size = asset->size > 0 ? qMin(asset->size, FixtureAssetSize) : FixtureAssetSize;
    QByteArray bytes = "#!/bin/sh\nsleep 600\n";
    while (bytes.size() < size)
        bytes += QByteArray(63, '#') + '\n';
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(bytes);
    asset->url = QUrl::fromLocalFile(path);
    asset->size = bytes.size();
    asset->sha256.clear();
}

void Installer::download(const EmulatorDef &emulator)
{
    const QString id = emulator.id;
    Job &job = m_jobs[id];
    const bool fixtures = !m_fixturesDir.isEmpty();
    if (fixtures)
        useFixtureAsset(&job.asset);
    const InstallLocations where = m_setup.locations();
    job.folder = AppImageInstall::targetFolder(where.applicationsDirs, where.home);
    job.state = State::Downloading;
    job.progress = -1;

    auto *transfer = new AppImageDownload(network(), job.asset, emulator.releaseInside, job.folder, this);
    job.download = transfer;
    if (fixtures) {
        transfer->setRelaxed(true);
        if (m_fixtureTickMs > 0)
            transfer->setPace(qMax<qint64>(1, job.asset.size / FixtureSteps), m_fixtureTickMs);
    }
    connect(transfer, &AppImageDownload::progress, this, [this, id](qint64 done, qint64 total) {
        const auto it = m_jobs.find(id);
        if (it == m_jobs.end())
            return;
        it->progress = total > 0 ? double(done) / double(total) : -1;
        changed();
    });
    connect(transfer, &AppImageDownload::stageChanged, this, [this, id, transfer] {
        const auto it = m_jobs.find(id);
        if (it == m_jobs.end())
            return;
        switch (transfer->stage()) {
        case AppImageDownload::Stage::Downloading: it->state = State::Downloading; break;
        case AppImageDownload::Stage::Extracting: it->state = State::Extracting; break;
        case AppImageDownload::Stage::Installing: it->state = State::Installing; break;
        }
        it->progress = -1;
        rebuild();
    });
    connect(transfer, &AppImageDownload::finished, this,
            [this, id](const AppImageDownload::Result &result) { downloadFinished(id, result); });
    rebuild();
    transfer->start();
}

void Installer::downloadFinished(const QString &id, const AppImageDownload::Result &result)
{
    const auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return;
    if (it->download)
        it->download->deleteLater();
    if (result.cancelled) {
        m_jobs.erase(it);
        changed();
        return;
    }
    if (!result.ok) {
        fail(id, result.error);
        return;
    }

    InstallRecord record;
    record.route = QStringLiteral("appimage");
    record.file = result.path;
    record.tag = it->release.tag;
    record.sha256 = it->asset.sha256;
    record.assetId = it->asset.id;
    record.assetDate = it->asset.date;
    record.installed = QDate::currentDate().toString(Qt::ISODate);
    m_jobs.erase(it);

    InstallRecords::Records records = InstallRecords::read(InstallRecords::path(m_setup.stateDir)).value;
    records.insert(id, record);
    QString error;
    if (!InstallRecords::write(InstallRecords::path(m_setup.stateDir), records, &error))
        qCWarning(lcInstall).noquote() << error;
    m_records = records;
    rebuild();
    if (m_setup.redetect)
        m_setup.redetect();
}

void Installer::fail(const QString &id, const QString &error)
{
    Job &job = m_jobs[id];
    job.state = State::Failed;
    job.progress = -1;
    job.error = error;
    job.reply.clear();
    qCWarning(lcInstall).noquote() << id << error;
    rebuild();
}

void Installer::cancel(const QString &id)
{
    const auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return;
    if (it->state == State::Failed || it->state == State::Idle) {
        m_jobs.erase(it);
        rebuild();
        return;
    }
    if (it->state == State::Checking) {
        if (it->reply)
            it->reply->abort();
        m_jobs.erase(it);
        rebuild();
        return;
    }
    if (it->download)
        it->download->cancel();
}
