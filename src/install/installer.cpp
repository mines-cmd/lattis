#include "installer.h"

#include "desktopentry.h"

#include <QCoreApplication>
#include <QDate>
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

Q_LOGGING_CATEGORY(lcInstall, "launcher.install")

Installer::Setup Installer::s_setup;

namespace {
constexpr qint64 FixtureAssetSize = 2 * 1024 * 1024;
constexpr int FixtureSteps = 40;
constexpr qint64 ReleaseCacheSeconds = 3600;

QSet<QString> userFlatpaks(const Installer::Setup &setup)
{
    const InstallLocations where = setup.locations ? setup.locations() : InstallLocations();
    return where.flatpakDirs.isEmpty() ? QSet<QString>() : Emulators::installedFlatpaks({where.flatpakDirs.first()});
}

QString releaseFileName(const QString &release)
{
    QString name = release;
    name.replace(u':', u'-').replace(u'/', u'-');
    return name + QStringLiteral(".json");
}
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
    case State::Removing: return QStringLiteral("removing");
    case State::Failed: return QStringLiteral("failed");
    }
    return {};
}

void Installer::refresh()
{
    m_flatpakProgram = FlatpakInstall::program();
    m_records = InstallRecords::read(InstallRecords::path(m_setup.stateDir)).value;
    rebuild();
}

void Installer::changed()
{
    if (!m_changed.isActive())
        m_changed.start();
}

std::optional<InstallRecord> Installer::installedByApp(const QString &id) const
{
    const auto it = m_records.constFind(id);
    if (it == m_records.cend() || !InstallRecords::stillThere(*it, userFlatpaks(m_setup)))
        return std::nullopt;
    return *it;
}

void Installer::rebuild()
{
    QVariantMap out;
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const QSet<QString> flatpaks = userFlatpaks(m_setup);
    for (const EmulatorDef &def : defs) {
        const bool appImage = !def.release.isEmpty() && !def.releaseAsset.isEmpty();
        const bool flatpak = FlatpakInstall::validId(def.flatpak);
        if (!appImage && !flatpak && !m_jobs.contains(def.id) && !m_records.contains(def.id))
            continue;
        const Job job = m_jobs.value(def.id);
        const InstallRecord record = m_records.value(def.id);
        const bool byApp = m_records.contains(def.id) && InstallRecords::stillThere(record, flatpaks);
        QStringList routes;
        if (appImage)
            routes.append(QStringLiteral("appimage"));
        if (flatpak && !m_flatpakProgram.isEmpty()) {
            if (def.releaseUnofficial.isEmpty())
                routes.append(QStringLiteral("flatpak"));
            else
                routes.prepend(QStringLiteral("flatpak"));
        }
        QString source;
        QString project;
        if (const auto parsed = ReleaseSources::parse(def.release)) {
            source = parsed->label();
            project = parsed->project;
        }
        QString updateVersion;
        if (byApp && record.route == QLatin1String("appimage")) {
            if (const auto update = m_updates.constFind(def.id); update != m_updates.cend())
                updateVersion = update->release.tag.isEmpty() ? update->asset.date : update->release.tag;
        }
        out.insert(def.id, QVariantMap{
                               {QStringLiteral("state"), stateName(job.state)},
                               {QStringLiteral("progress"), job.progress},
                               {QStringLiteral("error"), job.error},
                               {QStringLiteral("jobRoute"), job.state == State::Idle ? QString()
                                    : job.flatpak.isNull() ? QStringLiteral("appimage") : QStringLiteral("flatpak")},
                               {QStringLiteral("routes"), routes},
                               {QStringLiteral("noFlatpak"), flatpak && m_flatpakProgram.isEmpty()},
                               {QStringLiteral("source"), source},
                               {QStringLiteral("installedByApp"), byApp},
                               {QStringLiteral("route"), byApp ? record.route : QString()},
                               {QStringLiteral("file"), byApp ? record.file : QString()},
                               {QStringLiteral("installed"), byApp ? record.installed : QString()},
                               {QStringLiteral("version"), byApp ? record.tag : QString()},
                               {QStringLiteral("updateVersion"), updateVersion},
                               {QStringLiteral("upToDate"), byApp && record.route == QLatin1String("appimage")
                                                                && updateVersion.isEmpty() && m_upToDate.contains(def.id)},
                               {QStringLiteral("project"), project},
                               {QStringLiteral("unofficial"), appImage && !def.releaseUnofficial.isEmpty()},
                               {QStringLiteral("size"), job.state == State::Downloading ? double(job.asset.size) : 0.0},
                               // The AppImage's rename into place is too short and too final to stop.
                               {QStringLiteral("canCancel"), job.state != State::Installing || !job.flatpak.isNull()},
                           });
    }
    if (out != m_emulators) {
        m_emulators = out;
        emit emulatorsChanged();
    }
}

bool Installer::busy(const QString &id) const
{
    const auto it = m_jobs.constFind(id);
    return it != m_jobs.cend() && it->state != State::Failed;
}

Installer::Job &Installer::startJob(const EmulatorDef &emulator, Kind kind)
{
    Job job;
    job.serial = ++m_serial;
    job.kind = kind;
    job.emulatorName = emulator.name;
    return m_jobs.insert(emulator.id, job).value();
}

QString Installer::gameRunning(const EmulatorDef &emulator) const
{
    return m_setup.runningGame ? m_setup.runningGame(emulator.name) : QString();
}

void Installer::install(const QString &id, const QString &route)
{
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const EmulatorDef *def = Emulators::find(defs, id);
    if (!def || busy(id))
        return;
    startJob(*def, Kind::Install).route = route;
    if (route == QLatin1String("appimage"))
        begin(*def);
    else if (route == QLatin1String("flatpak"))
        beginFlatpak(*def, Kind::Install, def->flatpak);
    else
        fail(id, tr("%1 can't be installed that way").arg(def->name));
}

void Installer::update(const QString &id)
{
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const EmulatorDef *def = Emulators::find(defs, id);
    if (!def || busy(id))
        return;
    const auto record = installedByApp(id);
    startJob(*def, Kind::Update);
    if (!record || record->route != QLatin1String("appimage")) {
        fail(id, tr("%1 wasn't installed as an AppImage by Lattis, so it can't be updated here").arg(def->name));
        return;
    }
    if (const QString game = gameRunning(*def); !game.isEmpty()) {
        fail(id, tr("Close %1 first").arg(game));
        return;
    }
    m_jobs[id].oldFile = record->file;
    begin(*def);
}

void Installer::uninstall(const QString &id)
{
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const EmulatorDef *def = Emulators::find(defs, id);
    if (!def || busy(id))
        return;
    const auto record = installedByApp(id);
    startJob(*def, Kind::Uninstall);
    if (!record) {
        fail(id, tr("%1 wasn't installed by Lattis, so it can't be uninstalled here").arg(def->name));
        return;
    }
    if (const QString game = gameRunning(*def); !game.isEmpty()) {
        fail(id, tr("Close %1 first").arg(game));
        return;
    }
    if (record->route == QLatin1String("flatpak"))
        beginFlatpak(*def, Kind::Uninstall, record->file);
    else
        removeAppImage(*def, *record);
}

void Installer::checkUpdates(const QString &id)
{
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const QStringList ids = id.isEmpty() ? m_records.keys() : QStringList{id};
    for (const QString &target : ids) {
        const EmulatorDef *def = Emulators::find(defs, target);
        if (!def || def->release.isEmpty() || def->releaseAsset.isEmpty() || busy(target))
            continue;
        const auto record = installedByApp(target);
        if (!record || record->route != QLatin1String("appimage"))
            continue;
        startJob(*def, Kind::Check).quiet = id.isEmpty();
        begin(*def);
    }
}

QString Installer::cachedReleasePath(const QString &release) const
{
    return QDir(m_setup.cacheDir).filePath(QStringLiteral("releases/") + releaseFileName(release));
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
    const Kind kind = job.kind;
    rebuild();

    if (!m_fixturesDir.isEmpty()) {
        const QString path = QDir(m_fixturesDir).filePath(releaseFileName(emulator.release));
        QTimer::singleShot(m_fixtureDelayMs, this, [this, emulator, serial, path] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                gotRelease(emulator, serial, {}, tr("No canned release answer in %1").arg(path), true);
            else
                gotRelease(emulator, serial, file.readAll(), {}, true);
        });
        return;
    }

    // Only the quiet looks reuse an answer; an install or update asks again for the newest.
    if (kind == Kind::Check) {
        const QString cached = cachedReleasePath(emulator.release);
        const QFileInfo info(cached);
        QFile file(cached);
        if (info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < ReleaseCacheSeconds
            && file.open(QIODevice::ReadOnly)) {
            const QByteArray body = file.readAll();
            QTimer::singleShot(0, this, [this, emulator, serial, body] { gotRelease(emulator, serial, body, {}, true); });
            return;
        }
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
        gotRelease(emulator, serial, body, reply->error() == QNetworkReply::NoError ? QString() : reply->errorString(),
                   false);
    });
}

void Installer::gotRelease(const EmulatorDef &emulator, quint64 serial, const QByteArray &json,
                           const QString &networkError, bool fromCache)
{
    const auto it = m_jobs.find(emulator.id);
    if (it == m_jobs.end() || it->serial != serial || it->state != State::Checking)
        return;
    const auto source = ReleaseSources::parse(emulator.release);
    QString error;
    const auto release = source ? ReleaseSources::parseRelease(*source, json, &error) : std::nullopt;
    if (!release) {
        if (fromCache && m_fixturesDir.isEmpty())
            QFile::remove(cachedReleasePath(emulator.release));
        const QString reason = json.isEmpty() && !networkError.isEmpty() ? networkError : error;
        fail(emulator.id, tr("Couldn't check the latest release: %1").arg(reason));
        return;
    }
    if (!fromCache) {
        const QString cached = cachedReleasePath(emulator.release);
        QDir().mkpath(QFileInfo(cached).absolutePath());
        QSaveFile file(cached);
        if (file.open(QIODevice::WriteOnly) && file.write(json) == json.size())
            file.commit();
    }
    const ReleaseAsset *asset = ReleaseSources::pickAsset(*release, emulator.releaseAsset);
    if (!asset) {
        fail(emulator.id, tr("The latest release (%1) has no download named %2").arg(release->tag, emulator.releaseAsset));
        return;
    }
    it->release = *release;
    it->asset = *asset;
    if (it->kind == Kind::Check)
        checked(emulator);
    else
        download(emulator);
}

void Installer::checked(const EmulatorDef &emulator)
{
    const QString id = emulator.id;
    const Job job = m_jobs.value(id);
    const InstallRecord record = m_records.value(id);
    const ReleaseAsset &asset = job.asset;
    // A rolling tag (DuckStation's) never changes, so the file decides, not the tag.
    const bool newer = !record.sha256.isEmpty() && !asset.sha256.isEmpty()
                           ? record.sha256 != asset.sha256
                           : record.assetId != asset.id || record.assetDate != asset.date;
    if (newer) {
        m_updates.insert(id, {job.release, asset});
        m_upToDate.remove(id);
    } else {
        m_updates.remove(id);
        m_upToDate.insert(id);
    }
    m_jobs.remove(id);
    rebuild();
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
    const bool updating = job.kind == Kind::Update;
    const InstallLocations where = m_setup.locations();
    job.folder = updating ? QFileInfo(job.oldFile).absolutePath()
                          : AppImageInstall::targetFolder(where.applicationsDirs, where.home);
    job.state = State::Downloading;
    job.progress = -1;

    auto *transfer = new AppImageDownload(network(), job.asset, emulator.releaseInside, job.folder, this);
    job.download = transfer;
    transfer->setReplace(updating);
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

void Installer::saveRecord(const QString &id, const InstallRecord *record)
{
    InstallRecords::Records records = InstallRecords::read(InstallRecords::path(m_setup.stateDir)).value;
    if (record)
        records.insert(id, *record);
    else
        records.remove(id);
    QString error;
    if (!InstallRecords::write(InstallRecords::path(m_setup.stateDir), records, &error))
        qCWarning(lcInstall).noquote() << error;
    m_records = records;
}

QString Installer::trashFile(const QString &path)
{
    QString error;
    if (!m_setup.trash)
        return tr("there is no trash to use");
    if (!m_setup.trash(path, &error))
        return error.isEmpty() ? tr("the trash refused it") : error;
    return {};
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

    const bool updated = it->kind == Kind::Update;
    const QString oldFile = it->oldFile;
    InstallRecord record;
    record.route = QStringLiteral("appimage");
    record.file = result.path;
    record.tag = it->release.tag;
    record.sha256 = it->asset.sha256;
    record.assetId = it->asset.id;
    record.assetDate = it->asset.date;
    record.installed = QDate::currentDate().toString(Qt::ISODate);
    m_jobs.erase(it);

    QString menuProblem;
    const DesktopEntry::Made menu =
        DesktopEntry::make(id, result.path,
                           m_fixturesDir.isEmpty() ? m_setup.locations().xdgDirs.value(QStringLiteral("data")) : QString(),
                           &menuProblem);
    if (!menuProblem.isEmpty())
        qCWarning(lcInstall).noquote() << "no menu entry for" << result.path << menuProblem;
    record.desktop = menu.desktop;
    record.icon = menu.icon;
    if (const auto previous = m_records.constFind(id); previous != m_records.cend() && previous->icon != record.icon
                                                      && !previous->icon.isEmpty())
        QFile::remove(previous->icon);
    saveRecord(id, &record);
    m_upToDate.insert(id);

    // The new file is in place and recorded; only now does the old one go.
    if (updated) {
        m_updates.remove(id);
        const QString old = result.replaced.isEmpty() ? oldFile : result.replaced;
        if (!old.isEmpty() && old != result.path) {
            if (const QString problem = trashFile(old); !problem.isEmpty())
                qCWarning(lcInstall).noquote() << "couldn't trash the old" << old << problem;
            if (!result.replaced.isEmpty())
                QDir().rmdir(QFileInfo(result.replaced).absolutePath());
        }
    }
    rebuild();
    if (m_setup.redetect)
        m_setup.redetect();
}

void Installer::removeAppImage(const EmulatorDef &emulator, const InstallRecord &record)
{
    if (const QString problem = trashFile(record.file); !problem.isEmpty()) {
        fail(emulator.id, tr("Couldn't move %1 to the trash: %2").arg(QFileInfo(record.file).fileName(), problem));
        return;
    }
    DesktopEntry::remove({record.desktop, record.icon});
    m_jobs.remove(emulator.id);
    m_updates.remove(emulator.id);
    m_upToDate.remove(emulator.id);
    saveRecord(emulator.id, nullptr);
    rebuild();
    if (m_setup.redetect)
        m_setup.redetect();
}

void Installer::beginFlatpak(const EmulatorDef &emulator, Kind kind, const QString &appId)
{
    const QString id = emulator.id;
    if (!FlatpakInstall::validId(appId)) {
        fail(id, tr("%1 has no Flatpak").arg(emulator.name));
        return;
    }
    if (m_flatpakProgram.isEmpty()) {
        fail(id, tr("Flatpak isn't installed on this system"));
        return;
    }
    Job &job = m_jobs[id];
    job.state = kind == Kind::Uninstall ? State::Removing : State::Installing;
    job.progress = -1;
    job.appId = appId;
    const QList<QStringList> steps = kind == Kind::Uninstall ? QList<QStringList>{FlatpakInstall::uninstallStep(appId)}
                                                             : FlatpakInstall::installSteps(appId);
    auto *run = new FlatpakJob(m_flatpakProgram, steps, m_setup.environment ? m_setup.environment() : QStringList(), this);
    job.flatpak = run;
    connect(run, &FlatpakJob::finished, this,
            [this, id](const FlatpakJob::Result &result) { flatpakFinished(id, result); });
    rebuild();
    run->start();
}

void Installer::flatpakFinished(const QString &id, const FlatpakJob::Result &result)
{
    const auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return;
    if (it->flatpak)
        it->flatpak->deleteLater();
    if (result.cancelled) {
        m_jobs.erase(it);
        rebuild();
        return;
    }
    if (!result.ok) {
        fail(id, result.error);
        return;
    }
    const bool removed = it->kind == Kind::Uninstall;
    const QString appId = it->appId;
    m_jobs.erase(it);
    if (removed) {
        m_updates.remove(id);
        m_upToDate.remove(id);
        saveRecord(id, nullptr);
    } else {
        InstallRecord record;
        record.route = QStringLiteral("flatpak");
        record.file = appId;
        record.installed = QDate::currentDate().toString(Qt::ISODate);
        saveRecord(id, &record);
        m_updates.remove(id);
    }
    rebuild();
    if (m_setup.redetect)
        m_setup.redetect();
}

void Installer::fail(const QString &id, const QString &error)
{
    const auto it = m_jobs.find(id);
    if (it == m_jobs.end())
        return;
    qCWarning(lcInstall).noquote() << id << error;
    if (it->quiet) {
        m_jobs.erase(it);
        rebuild();
        return;
    }
    it->state = State::Failed;
    it->progress = -1;
    it->error = error;
    it->reply.clear();
    rebuild();
}

void Installer::retry(const QString &id)
{
    const auto it = m_jobs.constFind(id);
    if (it == m_jobs.cend() || it->state != State::Failed)
        return;
    const Kind kind = it->kind;
    const QString route = it->route;
    switch (kind) {
    case Kind::Install: install(id, route); break;
    case Kind::Update: update(id); break;
    case Kind::Check: checkUpdates(id); break;
    case Kind::Uninstall: uninstall(id); break;
    }
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
    if (it->flatpak) {
        it->flatpak->cancel();
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
