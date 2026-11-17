#include "installer.h"

#include "appupdate.h"
#include "desktopentry.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFutureWatcher>
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
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcInstall, "launcher.install")

Installer::Setup Installer::s_setup;

namespace {
std::optional<ReleaseSource> sourceOf(const QString &release, bool prereleases, QString *problem = nullptr)
{
    auto source = ReleaseSources::parse(release, problem);
    if (source && source->host == ReleaseSource::Host::GitHub)
        source->prereleases = prereleases;
    return source;
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

QString versionText(const QString &tag, bool prerelease)
{
    return prerelease ? tag + QStringLiteral(" (pre-release)") : tag;
}

constexpr qint64 FixtureAssetSize = 2 * 1024 * 1024;
constexpr int FixtureSteps = 40;
constexpr qint64 ReleaseCacheSeconds = 3600;
constexpr int DayMs = 24 * 60 * 60 * 1000;

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
    m_daily.setSingleShot(true);
    connect(&m_daily, &QTimer::timeout, this, [this] {
        if (m_autoCheck && m_autoCheck())
            checkUpdates();
        m_daily.start(DayMs);
    });
    refresh();
}

Installer::~Installer() = default;

void Installer::startDailyCheck(std::function<bool()> autoCheck, int delayMs)
{
    m_autoCheck = std::move(autoCheck);
    m_daily.start(delayMs);
}

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
    m_gameRecords = GameUpdateRecords::read(GameUpdateRecords::path(m_setup.stateDir)).value;
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
    QVariantList updateList;
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
        if (const auto parsed = sourceOf(def.release, def.releasePrereleases)) {
            source = parsed->label();
            project = parsed->project;
        }
        QString updateVersion;
        if (byApp && record.route == QLatin1String("appimage")) {
            if (const auto update = m_updates.constFind(def.id); update != m_updates.cend())
                updateVersion = update->release.tag.isEmpty() ? update->asset.date
                                                               : versionText(update->release.tag, update->release.prerelease);
        }
        if (!updateVersion.isEmpty())
            updateList.append(QVariantMap{{QStringLiteral("id"), def.id},
                                          {QStringLiteral("kind"), QStringLiteral("emulator")},
                                          {QStringLiteral("gameId"), QString()},
                                          {QStringLiteral("name"), def.name},
                                          {QStringLiteral("version"), updateVersion},
                                          {QStringLiteral("from"), versionText(record.tag, record.prerelease)},
                                          {QStringLiteral("notes"), m_updates.value(def.id).release.notes},
                                          {QStringLiteral("size"), double(m_updates.value(def.id).asset.size)},
                                          {QStringLiteral("state"), stateName(job.state)}});
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
                               {QStringLiteral("version"), byApp ? versionText(record.tag, record.prerelease) : QString()},
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
    QVariantMap gameRows;
    const QList<GameTarget> games = m_setup.games ? m_setup.games() : QList<GameTarget>();
    for (const GameTarget &game : games) {
        const QString key = gameKey(game.id);
        const Job job = m_jobs.value(key);
        const GameUpdateRecord record = m_gameRecords.value(game.id);
        const bool recorded = record.file == QFileInfo(game.file).absoluteFilePath();
        const auto update = m_updates.constFind(key);
        const bool hasUpdate = update != m_updates.cend();
        const bool upToDate = !hasUpdate && m_upToDate.contains(key);
        if (!hasUpdate && !upToDate && job.state == State::Idle)
            continue;
        QString updateVersion;
        if (hasUpdate)
            updateVersion = update->release.tag.isEmpty() ? update->asset.date
                                                           : versionText(update->release.tag, update->release.prerelease);
        const QString version = recorded ? record.tag : QString();
        if (hasUpdate)
            updateList.append(QVariantMap{{QStringLiteral("id"), key},
                                          {QStringLiteral("kind"), QStringLiteral("game")},
                                          {QStringLiteral("gameId"), game.id},
                                          {QStringLiteral("name"), game.title},
                                          {QStringLiteral("version"), updateVersion},
                                          {QStringLiteral("from"), version},
                                          {QStringLiteral("notes"), update->release.notes},
                                          {QStringLiteral("size"), double(update->asset.size)},
                                          {QStringLiteral("state"), stateName(job.state)}});
        QString project;
        if (const auto parsed = sourceOf(game.source, game.prereleases))
            project = parsed->label();
        gameRows.insert(game.id, QVariantMap{
                                     {QStringLiteral("state"), stateName(job.state)},
                                     {QStringLiteral("progress"), job.progress},
                                     {QStringLiteral("error"), job.error},
                                     {QStringLiteral("version"), version},
                                     {QStringLiteral("updateVersion"), updateVersion},
                                     {QStringLiteral("upToDate"), upToDate},
                                     {QStringLiteral("match"), m_matched.value(key)},
                                     {QStringLiteral("notes"), hasUpdate ? update->release.notes : QString()},
                                     {QStringLiteral("size"), hasUpdate ? double(update->asset.size) : 0.0},
                                     {QStringLiteral("source"), project},
                                     {QStringLiteral("canCancel"), job.state != State::Installing},
                                 });
    }
    if (out != m_emulators || gameRows != m_gameRows) {
        m_emulators = out;
        m_updateList = updateList;
        m_gameRows = gameRows;
        emit emulatorsChanged();
    }
}

bool Installer::busy(const QString &id) const
{
    const auto it = m_jobs.constFind(id);
    return it != m_jobs.cend() && it->state != State::Failed;
}

Installer::Job &Installer::startJob(const ReleaseTarget &target, Kind kind)
{
    Job job;
    job.serial = ++m_serial;
    job.kind = kind;
    job.emulatorName = target.name;
    return m_jobs.insert(target.id, job).value();
}

Installer::ReleaseTarget Installer::targetOf(const EmulatorDef &emulator)
{
    ReleaseTarget target;
    target.id = emulator.id;
    target.name = emulator.name;
    target.release = emulator.release;
    target.assets = emulator.releaseAsset;
    target.prereleases = emulator.releasePrereleases;
    target.inside = emulator.releaseInside;
    return target;
}

Installer::ReleaseTarget Installer::targetOf(const GameTarget &game)
{
    ReleaseTarget target;
    target.id = gameKey(game.id);
    target.name = game.title;
    target.release = game.source;
    target.assets = ReleaseSources::withoutZsync(game.asset);
    target.prereleases = game.prereleases;
    target.game = true;
    target.gameId = game.id;
    target.file = game.file;
    return target;
}

std::optional<Installer::GameTarget> Installer::findGame(const QString &gameId) const
{
    const QList<GameTarget> games = m_setup.games ? m_setup.games() : QList<GameTarget>();
    for (const GameTarget &game : games)
        if (game.id == gameId)
            return game;
    return std::nullopt;
}

QString Installer::gameBlocker(const GameTarget &game) const
{
    const QFileInfo file(game.file);
    if (!file.isFile())
        return tr("The AppImage file isn't there");
    if (!QFileInfo(file.absolutePath()).isWritable())
        return tr("%1 can't be written to").arg(QDir::toNativeSeparators(file.absolutePath()));
    QString applications;
    if (m_setup.locations) {
        const InstallLocations where = m_setup.locations();
        applications = where.xdgDirs.contains(QStringLiteral("data"))
                           ? where.xdgDirs.value(QStringLiteral("data")) + QStringLiteral("/applications")
                           : where.home + QStringLiteral("/.local/share/applications");
    }
    if (const QString manager = AppUpdates::managedBy(game.file, applications); !manager.isEmpty())
        return tr("%1 manages this AppImage; update it there").arg(manager);
    return {};
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
    startJob(targetOf(*def), Kind::Install).route = route;
    if (route == QLatin1String("appimage"))
        begin(targetOf(*def));
    else if (route == QLatin1String("flatpak"))
        beginFlatpak(*def, Kind::Install, def->flatpak);
    else
        fail(id, tr("%1 can't be installed that way").arg(def->name));
}

void Installer::update(const QString &id)
{
    if (id.startsWith(gameKey({}))) {
        const auto game = findGame(id.mid(gameKey({}).size()));
        if (game && !busy(id))
            updateGameFile(*game);
        return;
    }
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const EmulatorDef *def = Emulators::find(defs, id);
    if (!def || busy(id))
        return;
    const auto record = installedByApp(id);
    startJob(targetOf(*def), Kind::Update);
    if (!record || record->route != QLatin1String("appimage")) {
        fail(id, tr("%1 wasn't installed as an AppImage by Lattis, so it can't be updated here").arg(def->name));
        return;
    }
    if (const QString game = gameRunning(*def); !game.isEmpty()) {
        fail(id, tr("Close %1 first").arg(game));
        return;
    }
    m_jobs[id].oldFile = record->file;
    begin(targetOf(*def));
}

void Installer::updateGameFile(const GameTarget &game)
{
    const ReleaseTarget target = targetOf(game);
    startJob(target, Kind::Update);
    if (const QString running = m_setup.gameRunningTitle ? m_setup.gameRunningTitle(game.id) : QString();
        !running.isEmpty()) {
        fail(target.id, tr("Close %1 first").arg(running));
        return;
    }
    if (const QString blocker = gameBlocker(game); !blocker.isEmpty()) {
        fail(target.id, blocker);
        return;
    }
    m_jobs[target.id].oldFile = game.file;
    begin(target);
}

void Installer::uninstall(const QString &id)
{
    const QList<EmulatorDef> defs = m_setup.emulators ? m_setup.emulators() : QList<EmulatorDef>();
    const EmulatorDef *def = Emulators::find(defs, id);
    if (!def || busy(id))
        return;
    const auto record = installedByApp(id);
    startJob(targetOf(*def), Kind::Uninstall);
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
    if (id.isEmpty() || id.startsWith(gameKey({}))) {
        const QList<GameTarget> games = m_setup.games ? m_setup.games() : QList<GameTarget>();
        for (const GameTarget &game : games) {
            if ((!id.isEmpty() && gameKey(game.id) != id) || game.source.isEmpty() || game.asset.isEmpty()
                || !QFileInfo(game.file).isFile() || busy(gameKey(game.id)))
                continue;
            const ReleaseTarget target = targetOf(game);
            startJob(target, Kind::Check).quiet = id.isEmpty();
            begin(target);
        }
        if (!id.isEmpty())
            return;
    }
    const QStringList ids = id.isEmpty() ? m_records.keys() : QStringList{id};
    for (const QString &target : ids) {
        const EmulatorDef *def = Emulators::find(defs, target);
        if (!def || def->release.isEmpty() || def->releaseAsset.isEmpty() || busy(target))
            continue;
        const auto record = installedByApp(target);
        if (!record || record->route != QLatin1String("appimage"))
            continue;
        startJob(targetOf(*def), Kind::Check).quiet = id.isEmpty();
        begin(targetOf(*def));
    }
}

int Installer::checkUpdatesAsked()
{
    m_asked.clear();
    checkUpdates();
    // Includes a look already running, whose end counts too.
    for (auto it = m_jobs.cbegin(); it != m_jobs.cend(); ++it)
        if (it->kind == Kind::Check && it->quiet)
            m_asked.insert(it.key());
    m_askedTotal = m_asked.size();
    m_askedFailed = 0;
    return m_askedTotal;
}

void Installer::askedDone(const QString &id, bool failed)
{
    if (!m_asked.remove(id))
        return;
    if (failed)
        ++m_askedFailed;
    if (m_asked.isEmpty())
        emit updatesChecked(m_askedTotal, m_askedFailed);
}

QString Installer::cachedReleasePath(const QString &release) const
{
    return QDir(m_setup.cacheDir).filePath(QStringLiteral("releases/") + releaseFileName(release));
}

void Installer::begin(const ReleaseTarget &emulator)
{
    const QString id = emulator.id;
    Job &job = m_jobs[id];
    QString problem;
    const auto source = sourceOf(emulator.release, emulator.prereleases, &problem);
    if (emulator.release.isEmpty() || emulator.assets.isEmpty()) {
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

void Installer::gotRelease(const ReleaseTarget &emulator, quint64 serial, const QByteArray &json,
                           const QString &networkError, bool fromCache)
{
    const auto it = m_jobs.find(emulator.id);
    if (it == m_jobs.end() || it->serial != serial || it->state != State::Checking)
        return;
    const auto source = sourceOf(emulator.release, emulator.prereleases);
    QString error;
    const auto release = source ? ReleaseSources::parseRelease(*source, json, &error, emulator.assets) : std::nullopt;
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
    const ReleaseAsset *asset = ReleaseSources::pickAsset(*release, emulator.assets);
    if (!asset) {
        m_matched.remove(emulator.id);
        fail(emulator.id, tr("The latest release (%1) has no download named %2").arg(release->tag, emulator.assets));
        return;
    }
    it->release = *release;
    it->asset = *asset;
    m_matched.insert(emulator.id, asset->name);
    if (it->kind == Kind::Check)
        checked(emulator);
    else
        download(emulator);
}

void Installer::checked(const ReleaseTarget &emulator)
{
    if (emulator.game) {
        checkedGame(emulator);
        return;
    }
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
    askedDone(id, false);
}

void Installer::checkedGame(const ReleaseTarget &target)
{
    const Job job = m_jobs.value(target.id);
    const ReleaseAsset &asset = job.asset;
    const QFileInfo file(target.file);
    const GameUpdateRecord record = m_gameRecords.value(target.gameId);
    const bool known = record.file == file.absoluteFilePath() && record.size == file.size() && !record.sha256.isEmpty();
    if (known) {
        const bool newer = !asset.sha256.isEmpty() ? record.sha256 != asset.sha256
                                                    : record.assetId != asset.id || record.assetDate != asset.date;
        gameCheckDone(target, newer);
        return;
    }
    // Nothing says which build is installed: its checksum is compared to the release's.
    if (asset.sha256.isEmpty()) {
        gameCheckDone(target, true);
        return;
    }
    const quint64 serial = job.serial;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, target, serial] {
        const QString hash = watcher->result();
        watcher->deleteLater();
        const auto it = m_jobs.constFind(target.id);
        if (it == m_jobs.cend() || it->serial != serial || it->state != State::Checking)
            return;
        if (hash.isEmpty()) {
            fail(target.id, tr("Couldn't read %1").arg(QDir::toNativeSeparators(target.file)));
            return;
        }
        const ReleaseAsset &asset = it->asset;
        const QFileInfo file(target.file);
        GameUpdateRecord record;
        record.file = file.absoluteFilePath();
        record.size = file.size();
        record.sha256 = hash;
        const bool same = hash == asset.sha256;
        if (same) {
            record.tag = it->release.tag;
            record.assetId = asset.id;
            record.assetDate = asset.date;
        }
        record.checked = QDate::currentDate().toString(Qt::ISODate);
        saveGameRecord(target.gameId, record);
        gameCheckDone(target, !same);
    });
    watcher->setFuture(QtConcurrent::run(fileSha256, target.file));
}

void Installer::gameCheckDone(const ReleaseTarget &target, bool newer)
{
    const QString key = target.id;
    const Job job = m_jobs.value(key);
    if (newer) {
        m_updates.insert(key, {job.release, job.asset});
        m_upToDate.remove(key);
    } else {
        m_updates.remove(key);
        m_upToDate.insert(key);
    }
    m_jobs.remove(key);
    rebuild();
    askedDone(key, false);
}

void Installer::saveGameRecord(const QString &gameId, const GameUpdateRecord &record)
{
    GameUpdateRecords::Records records = GameUpdateRecords::read(GameUpdateRecords::path(m_setup.stateDir)).value;
    records.insert(gameId, record);
    QString error;
    if (!GameUpdateRecords::write(GameUpdateRecords::path(m_setup.stateDir), records, &error))
        qCWarning(lcInstall).noquote() << error;
    m_gameRecords = records;
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

void Installer::download(const ReleaseTarget &emulator)
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

    auto *transfer = new AppImageDownload(network(), job.asset, emulator.inside, job.folder, this);
    job.download = transfer;
    transfer->setReplace(updating);
    // The same path keeps the game's program, Steam shortcuts and menu entries working.
    if (emulator.game)
        transfer->setTargetName(QFileInfo(emulator.file).fileName());
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
    const bool game = emulator.game;
    connect(transfer, &AppImageDownload::finished, this, [this, id, game](const AppImageDownload::Result &result) {
        if (game)
            gameDownloadFinished(id, result);
        else
            downloadFinished(id, result);
    });
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
    const QString updatedName = it->emulatorName;
    const Release updatedRelease = it->release;
    InstallRecord record;
    record.route = QStringLiteral("appimage");
    record.file = result.path;
    record.tag = it->release.tag;
    record.prerelease = it->release.prerelease;
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
    if (updated)
        emit emulatorUpdated(updatedName, updatedRelease.tag, updatedRelease.notes);
}

void Installer::gameDownloadFinished(const QString &key, const AppImageDownload::Result &result)
{
    const auto it = m_jobs.find(key);
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
        fail(key, result.error);
        return;
    }

    const QString gameId = key.mid(gameKey({}).size());
    const QString name = it->emulatorName;
    const Release release = it->release;
    const QString oldFile = it->oldFile;
    GameUpdateRecord record;
    record.file = QFileInfo(result.path).absoluteFilePath();
    record.size = QFileInfo(result.path).size();
    record.sha256 = result.sha256;
    record.assetId = it->asset.id;
    record.assetDate = it->asset.date;
    record.tag = release.tag;
    record.checked = QDate::currentDate().toString(Qt::ISODate);
    m_jobs.erase(it);
    saveGameRecord(gameId, record);
    m_updates.remove(key);
    m_upToDate.insert(key);

    // The new file is in place and recorded; only now does the old one go.
    const QString old = result.replaced.isEmpty() ? oldFile : result.replaced;
    if (!old.isEmpty() && old != result.path) {
        if (const QString problem = trashFile(old); !problem.isEmpty())
            qCWarning(lcInstall).noquote() << "couldn't trash the old" << old << problem;
        if (!result.replaced.isEmpty())
            QDir().rmdir(QFileInfo(result.replaced).absolutePath());
    }
    rebuild();
    if (m_setup.redetect)
        m_setup.redetect();
    emit emulatorUpdated(name, release.tag, release.notes);
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
        const bool check = it->kind == Kind::Check;
        m_jobs.erase(it);
        rebuild();
        if (check)
            askedDone(id, true);
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
