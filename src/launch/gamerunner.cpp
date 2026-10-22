#include "gamerunner.h"
#include "detachedprocess.h"
#include "gameid.h"
#include "launchcommand.h"
#include "library.h"
#include "steamshortcuts.h"
#include "tomlpatch.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <utility>

Q_LOGGING_CATEGORY(lcLaunch, "launcher.launch")

namespace {
QStringList lastLines(const QString &path, int count)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 tail = 16 * 1024;
    if (file.size() > tail)
        file.seek(file.size() - tail);
    QStringList lines = QString::fromLocal8Bit(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
    return lines.mid(qMax(0, int(lines.size()) - count));
}

QString orList(const QStringList &names)
{
    if (names.size() < 2)
        return names.join(QString());
    return names.mid(0, names.size() - 1).join(QStringLiteral(", ")) + GameRunner::tr(" or ") + names.last();
}

QString missingRom(const Game &game)
{
    return game.romKey ? GameRunner::tr("%1's ROM file isn't found").arg(game.title)
                       : GameRunner::tr("%1's ROM isn't in the ROM folders").arg(game.title);
}

QString missingProgram(const Game &game)
{
    return game.uninstalled ? GameRunner::tr("%1 isn't installed; locate its program to play it").arg(game.title)
                            : GameRunner::tr("%1's program isn't found; locate it to play it").arg(game.title);
}

// TOML has no default handler of its own; xdg-open falls back to text/plain's, and so does this.
QString defaultDesktopFile(const QString &path, const QStringList &env)
{
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path);
    for (const QString &type : QStringList{mime.name()} + mime.allAncestors()) {
        QProcess query;
        query.setEnvironment(env);
        query.start(QStringLiteral("xdg-mime"), {QStringLiteral("query"), QStringLiteral("default"), type});
        if (!query.waitForFinished(2000)) {
            query.kill();
            return {};
        }
        const QString id = QString::fromLocal8Bit(query.readAllStandardOutput()).trimmed();
        if (!id.isEmpty())
            return QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id);
    }
    return {};
}

QStringList terminalEntryCommand(const QString &desktopFile, const QString &path)
{
    QFile file(desktopFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    bool inEntry = false;
    bool terminal = false;
    QString exec;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(u'[')) {
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        const qsizetype eq = line.indexOf(u'=');
        if (!inEntry || eq < 0)
            continue;
        const QString key = line.left(eq).trimmed();
        if (key == QLatin1String("Exec"))
            exec = line.mid(eq + 1).trimmed();
        else if (key == QLatin1String("Terminal"))
            terminal = line.mid(eq + 1).trimmed() == QLatin1String("true");
    }
    if (!terminal || exec.isEmpty())
        return {};
    QStringList command;
    bool placed = false;
    for (QString arg : QProcess::splitCommand(exec)) {
        if (arg == QLatin1String("%f") || arg == QLatin1String("%F") || arg == QLatin1String("%u")
            || arg == QLatin1String("%U")) {
            command << path;
            placed = true;
        } else if (arg.size() == 2 && arg.front() == u'%' && arg.back() != u'%') {
            continue;
        } else {
            command << arg.replace(QLatin1String("%%"), QLatin1String("%"));
        }
    }
    if (command.isEmpty())
        return {};
    if (!placed)
        command << path;
    return command;
}

bool startInTerminal(const QStringList &command, const QStringList &env)
{
    QList<std::pair<QString, QStringList>> terminals = {{QStringLiteral("xdg-terminal-exec"), {}}};
    if (const QString own = qEnvironmentVariable("TERMINAL"); !own.isEmpty())
        terminals.append({own, {QStringLiteral("-e")}});
    terminals += {
        {QStringLiteral("kitty"), {}},
        {QStringLiteral("foot"), {}},
        {QStringLiteral("alacritty"), {QStringLiteral("-e")}},
        {QStringLiteral("wezterm"), {QStringLiteral("start"), QStringLiteral("--")}},
        {QStringLiteral("ghostty"), {QStringLiteral("-e")}},
        {QStringLiteral("konsole"), {QStringLiteral("-e")}},
        {QStringLiteral("gnome-terminal"), {QStringLiteral("--")}},
        {QStringLiteral("xterm"), {QStringLiteral("-e")}},
    };
    for (const auto &[name, before] : std::as_const(terminals)) {
        const QString program = QStandardPaths::findExecutable(name);
        if (program.isEmpty())
            continue;
        QProcess process;
        process.setProgram(program);
        process.setArguments(before + command);
        process.setEnvironment(env);
        process.setWorkingDirectory(QDir::homePath());
        if (process.startDetached()) {
            qCInfo(lcLaunch).noquote() << QStringLiteral("opening in %1: %2").arg(name, command.join(u' '));
            return true;
        }
    }
    return false;
}
}

GameRunner::SteamRef GameRunner::steamRefOf(const Game &game)
{
    if (game.platform == kSteamPlatform) {
        if (const qint64 appId = GameId::steamAppId(game.id); appId > 0)
            return {quint64(appId), quint64(appId), false};
    } else if (game.platform == kProtonPlatform && game.shortcut != 0) {
        return {SteamShortcuts::gameId(game.shortcut), game.shortcut, true};
    }
    return {};
}

QStringList GameRunner::steamCommand(SteamInstall::Kind kind)
{
    if (kind == SteamInstall::Kind::Flatpak)
        return {QStringLiteral("flatpak"), QStringLiteral("run"), QStringLiteral("com.valvesoftware.Steam")};
    const QString steam = QStandardPaths::findExecutable(QStringLiteral("steam"));
    return steam.isEmpty() ? QStringList() : QStringList{steam};
}

QString GameRunner::runSteam(const QStringList &args, bool raiseWindow)
{
    QStringList command = m_steamProgram;
    if (command.isEmpty()) {
        const SteamInstall &install = m_library->store().snapshot().steam.install;
        if (!install.found())
            return tr("Steam isn't found");
        command = steamCommand(install.kind);
        if (command.isEmpty())
            return tr("Steam's steam command isn't on your PATH");
    }
    const QString log = m_logDir + QStringLiteral("/launch/steam.log");
    QDir().mkpath(QFileInfo(log).absolutePath());
    QProcess process;
    QProcessEnvironment env;
    for (const QString &var : programEnvironment(QProcess::systemEnvironment())) {
        const qsizetype eq = var.indexOf(u'=');
        env.insert(var.left(eq), var.mid(eq + 1));
    }
    process.setProcessEnvironment(env);
    process.setProgram(command.first());
    process.setArguments(command.mid(1) + args);
    process.setWorkingDirectory(QDir::homePath());
    process.setStandardOutputFile(log, QIODevice::Append);
    process.setStandardErrorFile(log, QIODevice::Append);
    qCInfo(lcLaunch).noquote() << QStringLiteral("handing %1 to Steam: %2").arg(args.last(), (command + args).join(u' '));
    if (!process.startDetached()) {
        qCWarning(lcLaunch).noquote() << QStringLiteral("could not start %1: %2").arg(command.first(), process.errorString());
        return tr("Could not start Steam: %1").arg(process.errorString());
    }
    // A running Steam can't take focus on Wayland without an activation token, which a
    // detached steam:// handoff doesn't carry; Hyprland can be asked to focus it directly.
    if (raiseWindow && qEnvironmentVariableIsSet("HYPRLAND_INSTANCE_SIGNATURE")) {
        QTimer::singleShot(500, this, [this] {
            // Current Hyprland takes Lua here; older versions take the legacy dispatcher and
            // answer the Lua form with an error, and the other way round.
            auto *lua = new QProcess(this);
            connect(lua, &QProcess::finished, this, [lua](int code) {
                const bool failed = code != 0 || lua->readAllStandardOutput().contains("rror");
                lua->deleteLater();
                if (failed)
                    QProcess::startDetached(QStringLiteral("hyprctl"),
                                            {QStringLiteral("dispatch"), QStringLiteral("focuswindow"),
                                             QStringLiteral("class:^steam$")});
            });
            lua->start(QStringLiteral("hyprctl"),
                       {QStringLiteral("dispatch"),
                        QStringLiteral("hl.dsp.focus({ window = \"class:^steam$\" })")});
        });
    }
    return {};
}

QString GameRunner::uninstall(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    const SteamRef steam = game ? steamRefOf(*game) : SteamRef();
    if (!steam || steam.shortcut)
        return {};
    if (!game->installed && game->steamUpdate != SteamUpdate::Downloading && game->steamUpdate != SteamUpdate::Paused)
        return tr("%1 isn't installed").arg(game->title);
    return runSteam({QStringLiteral("steam://uninstall/%1").arg(steam.runId)});
}

QString GameRunner::update(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    const SteamRef steam = game ? steamRefOf(*game) : SteamRef();
    if (!steam || steam.shortcut || game->steamUpdate == SteamUpdate::None)
        return {};
    // Steam ignores steam://update/<id>, steam://install/<id> on an installed game, and has no URL
    // that starts an update short of steam://validate, so Waiting opens the game's page, where
    // Steam's own Update button is.
    return runSteam({game->steamUpdate == SteamUpdate::Waiting
                         ? QStringLiteral("steam://nav/games/details/%1").arg(steam.runId)
                         : QStringLiteral("steam://open/downloads")},
                    true);
}

QVariantMap GameRunner::uninstallProton(const QString &gameId, bool files, bool prefix, bool shortcut,
                                        const QString &action, bool permanent)
{
    if (running().contains(gameId)) {
        const Game *game = m_library->game(gameId);
        return {{QStringLiteral("error"), tr("%1 is running; close it first").arg(game ? game->title : gameId)}};
    }
    return m_library->uninstallProtonGame(gameId, files, prefix, shortcut, action, permanent);
}

QString GameRunner::closeSteamAndApply()
{
    return closeSteamThen({});
}

QString GameRunner::restartSteamAndLaunch(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    if (!game || !steamRefOf(*game) || !game->installed || game->steamUpdate != SteamUpdate::None
        || m_steamSessions.contains(gameId))
        return launch(gameId);
    return closeSteamThen(gameId);
}

QString GameRunner::closeSteamThen(const QString &gameId)
{
    const Game *game = gameId.isEmpty() ? nullptr : m_library->game(gameId);
    const SteamRef steam = game ? steamRefOf(*game) : SteamRef();
    if (m_steamClosing) {
        if (gameId.isEmpty() || gameId == m_launchAfterClose)
            return {};
        if (!m_launchAfterClose.isEmpty())
            return tr("Steam is restarting to launch another game; try again once it has");
        m_launchAfterClose = gameId;
        addSteamSession(gameId, steam);
        return {};
    }
    LibraryStore &store = m_library->store();
    const qint64 pid = store.steamPid();
    auto *watch = new SteamExitWatch(this);
    if (pid <= 0 || !watch->watch(pid)) {
        delete watch;
        return gameId.isEmpty() ? store.applySteamQueue() : launch(gameId);
    }
    if (const QString error = runSteam({QStringLiteral("-shutdown")}); !error.isEmpty()) {
        delete watch;
        return error;
    }
    m_steamClosing = watch;
    connect(watch, &SteamExitWatch::exited, this, [this] { onSteamClosed(true); });
    m_steamCloseTimer.start();
    if (!gameId.isEmpty()) {
        m_launchAfterClose = gameId;
        addSteamSession(gameId, steam);
    }
    emit closingSteamChanged();
    return {};
}

bool GameRunner::steamRunning() const
{
    return m_library->store().steamPid() > 0;
}

void GameRunner::onSteamClosed(bool closed)
{
    m_steamCloseTimer.stop();
    m_steamClosing->deleteLater();
    m_steamClosing = nullptr;
    emit closingSteamChanged();
    const QString launchId = std::exchange(m_launchAfterClose, QString());
    if (!closed) {
        qCWarning(lcLaunch) << "Steam didn't close in time; nothing was applied";
        if (m_steamSessions.remove(launchId)) {
            emit runningChanged();
            const Game *game = m_library->game(launchId);
            emit message(tr("Steam didn't close within %1 seconds; nothing was applied, and %2 wasn't launched")
                             .arg(kSteamCloseSeconds)
                             .arg(game ? game->title : launchId));
            return;
        }
        emit message(tr("Steam didn't close within %1 seconds; nothing was applied").arg(kSteamCloseSeconds));
        return;
    }
    const QString error = m_library->store().applySteamQueue();
    const auto session = m_steamSessions.find(launchId);
    QStringList args{QStringLiteral("-silent")};
    if (session != m_steamSessions.end())
        args.append(QStringLiteral("steam://rungameid/%1").arg(session->steam.runId));
    if (const QString started = runSteam(args); !started.isEmpty()) {
        if (m_steamSessions.remove(launchId))
            emit runningChanged();
        emit message(started);
    } else if (session != m_steamSessions.end()) {
        session->since.start();
        m_steamPoll.start();
    }
    if (!error.isEmpty())
        emit message(error);
}

void GameRunner::pollSteamGames()
{
    bool waiting = false;
    const QStringList ids = m_steamSessions.keys();
    for (const QString &id : ids) {
        SteamSession &session = m_steamSessions[id];
        if (session.watch)
            continue;
        const qint64 pid = SteamLocate::gamePid(session.steam.reaperId);
        auto *watch = pid > 0 ? new SteamExitWatch(this) : nullptr;
        if (watch && watch->watch(pid)) {
            qCInfo(lcLaunch).noquote() << QStringLiteral("%1 is running through Steam (pid %2)").arg(id).arg(pid);
            session.watch = watch;
            session.gone.invalidate();
            connect(watch, &SteamExitWatch::exited, this, [this, id] { onSteamProcessExited(id); });
            if (!std::exchange(session.seen, true))
                emit sessionStarted(id);
            continue;
        }
        delete watch; // it exited before the pidfd was opened
        if (session.gone.isValid()) {
            if (session.gone.elapsed() > kSteamGoneSeconds * 1000)
                endSteamSession(id);
            else
                waiting = true;
            continue;
        }
        if (session.since.elapsed() > kSteamStartSeconds * 1000) {
            qCInfo(lcLaunch).noquote() << QStringLiteral("%1 didn't start within %2 s of the launch; not running")
                                              .arg(id)
                                              .arg(kSteamStartSeconds);
            m_steamSessions.remove(id);
            emit runningChanged();
            continue;
        }
        waiting = true;
    }
    if (!waiting)
        m_steamPoll.stop();
}

QString GameRunner::startSteamGame(const QString &gameId, const SteamRef &steam)
{
    // -silent: a running Steam starts the game without bringing up its window.
    if (const QString error = runSteam({QStringLiteral("-silent"), QStringLiteral("steam://rungameid/%1").arg(steam.runId)});
        !error.isEmpty())
        return error;
    addSteamSession(gameId, steam);
    return {};
}

void GameRunner::addSteamSession(const QString &gameId, const SteamRef &steam)
{
    SteamSession session;
    session.steam = steam;
    session.since.start();
    m_steamSessions.insert(gameId, session);
    emit runningChanged();
    m_steamPoll.start();
    pollSteamGames();
}

void GameRunner::onSteamProcessExited(const QString &gameId)
{
    const auto session = m_steamSessions.find(gameId);
    if (session == m_steamSessions.end())
        return;
    session->watch->deleteLater();
    session->watch = nullptr;
    session->gone.start();
    m_steamPoll.start();
    pollSteamGames();
}

void GameRunner::endSteamSession(const QString &gameId)
{
    const SteamSession session = m_steamSessions.take(gameId);
    if (session.watch)
        session.watch->deleteLater();
    qCInfo(lcLaunch).noquote() << QStringLiteral("%1 exited after %2 s").arg(gameId).arg(session.since.elapsed() / 1000);
    emit runningChanged();
    emit sessionEnded(gameId);
}

GameRunner::GameRunner(Library *library, const QString &dataDir, const QString &logDir, QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_dataDir(dataDir)
    , m_logDir(logDir)
{
    s_instance = this;
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_reloadTimer, &QTimer::timeout, this, &GameRunner::loadSettings);
    m_steamPoll.setInterval(1000);
    connect(&m_steamPoll, &QTimer::timeout, this, &GameRunner::pollSteamGames);
    m_steamCloseTimer.setSingleShot(true);
    m_steamCloseTimer.setInterval(kSteamCloseSeconds * 1000);
    connect(&m_steamCloseTimer, &QTimer::timeout, this, [this] { onSteamClosed(false); });
}

GameRunner::~GameRunner()
{
    if (!m_sessions.isEmpty())
        qCInfo(lcLaunch).noquote() << QStringLiteral("quitting with %1 game(s) running; they keep running, but "
                                                     "their play time isn't recorded")
                                          .arg(m_sessions.size());
    if (s_instance == this)
        s_instance = nullptr;
}

GameRunner *GameRunner::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void GameRunner::start()
{
    loadSettings();
}

QString GameRunner::setLaunchValue(const QString &key, const QString &value)
{
    static const QString kWhileRunning = QStringLiteral("while_running");
    static const QString kMinSession = QStringLiteral("min_session_seconds");
    static const QString kDefaultWhileRunning = QStringLiteral("stay");
    static const int kDefaultMinSession = 30;

    if (key == kWhileRunning) {
        if (value != QStringLiteral("stay") && value != QStringLiteral("minimize"))
            return QStringLiteral("while_running must be \"stay\" or \"minimize\"");
        QString error;
        if (value == kDefaultWhileRunning)
            TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("launch"), key, &error);
        else
            TomlPatch::setValueInFile(settingsFile(), QStringLiteral("launch"), key,
                                      u'"' + value + u'"', &error);
        return error;
    }
    if (key == kMinSession) {
        bool ok = false;
        const int seconds = value.toInt(&ok);
        if (!ok || seconds < 0 || seconds > 3600)
            return QStringLiteral("min_session_seconds must be a whole number from 0 to 3600");
        QString error;
        if (seconds == kDefaultMinSession)
            TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("launch"), key, &error);
        else
            TomlPatch::setValueInFile(settingsFile(), QStringLiteral("launch"), key,
                                      QString::number(seconds), &error);
        return error;
    }
    return QStringLiteral("unknown key %1").arg(key);
}

void GameRunner::loadSettings()
{
    // The folder catches the file being replaced (editors, the settings window's saves).
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    for (const QString &path : {settingsFile(), m_dataDir}) {
        if (!watched.contains(path) && QFileInfo::exists(path))
            m_watcher.addPath(path);
    }

    QFile file(settingsFile());
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    const Parsed<LaunchSettings> parsed = parseLaunchSettings(bytes, QStringLiteral("settings.toml"));
    for (const QString &warning : parsed.warnings) {
        if (!m_problems.contains(warning))
            qCWarning(lcLaunch).noquote() << warning;
    }
    const LaunchSettings settings = parsed.ok ? parsed.value : m_settings;
    if (settings == m_settings && parsed.warnings == m_problems)
        return;
    m_settings = settings;
    m_problems = parsed.warnings;
    emit settingsChanged();
}

int GameRunner::sessionMinutes(qint64 elapsedMs)
{
    return int((elapsedMs + 30'000) / 60'000);
}

QString GameRunner::chooseEmulator(const QString &gameId, QString *emulatorId) const
{
    const Game *game = m_library->game(gameId);
    const Platform *platform = m_library->platform(game->platform);
    const QStringList preferred = platform ? platform->emulators : QStringList();
    *emulatorId = Emulators::choose(game->emulator, preferred, m_library->installs());
    if (!emulatorId->isEmpty())
        return {};

    QStringList names;
    for (const QString &id : QStringList{game->emulator} + preferred) {
        const EmulatorDef *e = Emulators::find(m_library->emulators(), id);
        if (e && !names.contains(e->name))
            names.append(e->name);
    }
    if (names.isEmpty())
        return tr("%1 has no emulators; add one to its emulators in platforms.toml").arg(game->platformName);
    return tr("Install %1 to play %2").arg(orList(names), game->title);
}

QString GameRunner::missingEmulator(const QString &gameId) const
{
    const Game *game = m_library->game(gameId);
    if (!game || !game->rom)
        return {};
    QString chosen;
    chooseEmulator(gameId, &chosen);
    if (!chosen.isEmpty())
        return {};
    const Platform *platform = m_library->platform(game->platform);
    for (const QString &id : QStringList{game->emulator} + (platform ? platform->emulators : QStringList())) {
        if (Emulators::find(m_library->emulators(), id))
            return id;
    }
    return {};
}

QString GameRunner::launch(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    if (!game)
        return {};
    if (const SteamRef steam = steamRefOf(*game)) {
        if (!game->installed && steam.shortcut)
            return missingProgram(*game);
        const bool downloading =
            game->steamUpdate == SteamUpdate::Downloading || game->steamUpdate == SteamUpdate::Paused;
        if (!game->installed && !downloading)
            return runSteam({QStringLiteral("steam://install/%1").arg(steam.runId)});
        if (game->steamUpdate != SteamUpdate::None)
            return update(gameId);
        if (m_steamSessions.contains(gameId))
            return {};
        // A new shortcut waits for Steam to close; until then Steam can't launch it.
        const bool running = steamRunning();
        if (steam.shortcut && running
            && m_library->store().snapshot().steamQueued.value(gameId).contains(SteamSync::kAddShortcut))
            return tr("%1 isn't in Steam yet; restart Steam to add it").arg(game->title);
        // With Steam closed, what's queued is written first, so the game starts with it.
        const QString applied = game->pendingSteamEdits && !running ? m_library->store().applySteamQueue() : QString();
        if (const QString error = startSteamGame(gameId, steam); !error.isEmpty())
            return error;
        return applied;
    }
    if (game->platform == kProtonPlatform)
        return tr("%1 has no Steam shortcut; add it with Settings → Steam → Add existing shortcuts").arg(game->title);
    if (game->platform == kLinuxPlatform)
        return launchProgram(*game);
    if (!game->rom)
        return tr("Launching %1 games is not implemented yet").arg(game->platformName);
    if (m_sessions.contains(gameId))
        return {};
    if (!game->installed)
        return missingRom(*game);

    QString emulatorId;
    if (const QString problem = chooseEmulator(gameId, &emulatorId); !problem.isEmpty())
        return problem;
    const EmulatorDef *emulator = Emulators::find(m_library->emulators(), emulatorId);
    const LaunchCommand command = buildLaunchCommand(*emulator, m_library->installs().value(emulatorId), game->romPath,
                                                     game->args);
    return startSession(gameId, emulator->name, command, QFileInfo(game->romPath).absolutePath(), false);
}

QString GameRunner::forceClose(const QString &gameId)
{
    if (const auto session = m_sessions.constFind(gameId); session != m_sessions.constEnd()) {
        qCInfo(lcLaunch).noquote() << QStringLiteral("force closing %1").arg(gameId);
        session->process->kill();
        return {};
    }
    if (const auto session = m_steamSessions.constFind(gameId); session != m_steamSessions.constEnd()) {
        const qint64 pid = SteamLocate::gamePid(session->steam.reaperId);
        if (pid <= 0)
            return tr("The game hasn't started yet");
        qCInfo(lcLaunch).noquote() << QStringLiteral("force closing %1 (pid %2)").arg(gameId).arg(pid);
        DetachedProcess::killTree(pid);
    }
    return {};
}

QString GameRunner::launchProgram(const Game &game)
{
    if (m_sessions.contains(game.id))
        return {};
    if (game.exe.isEmpty())
        return tr("%1 has no program; choose it in its settings").arg(game.title);
    const QFileInfo exe(game.exe);
    if (game.uninstalled || !exe.isFile())
        return missingProgram(game);
    if (!exe.isExecutable())
        return tr("%1's program isn't executable; make it executable in its file properties, or with "
                  "chmod +x \"%2\"")
            .arg(game.title, game.exe);
    const LaunchOptions::Parts options = LaunchOptions::parse(game.launchOptions);
    if (!options.problem.isEmpty())
        return tr("Can't launch %1: %2").arg(game.title, options.problem);

    QString workingDir = exe.absolutePath();
    const LibraryStore &store = m_library->store();
    if (const QString startDir = store.snapshot().content.games.value(game.id).startDir; !startDir.isEmpty()) {
        workingDir = resolveRomFolder(startDir, m_dataDir, store.installLocations().home);
        if (!QFileInfo(workingDir).isDir())
            return tr("%1's start folder isn't found: %2").arg(game.title, workingDir);
    }
    return startSession(game.id, game.title, buildProgramCommand(game.exe, options), workingDir, true);
}

QString GameRunner::startSession(const QString &gameId, const QString &name, const LaunchCommand &command,
                                 const QString &workingDir, bool followSession)
{
    Session session;
    session.emulatorName = name;
    session.logPath = QStringLiteral("%1/launch/%2.log").arg(m_logDir, gameId);
    session.process = new DetachedProcess(this);
    session.process->setFollowSession(followSession);
    session.process->setSessionPollInterval(m_sessionPollMs);
    qCInfo(lcLaunch).noquote() << QStringLiteral("launching %1 with %2: %3").arg(gameId, name, command.toString());
    const QString error = session.process->start(command, workingDir, session.logPath);
    if (!error.isEmpty()) {
        delete session.process;
        qCWarning(lcLaunch).noquote() << QStringLiteral("could not start %1: %2").arg(name, error);
        return tr("Could not start %1: %2").arg(name, error);
    }
    session.timer.start();
    connect(session.process, &DetachedProcess::finished, this,
            [this, gameId](int exitCode, bool crashed) { onFinished(gameId, exitCode, crashed); });
    m_sessions.insert(gameId, session);
    emit runningChanged();
    emit sessionStarted(gameId);
    return {};
}

void GameRunner::onFinished(const QString &gameId, int exitCode, bool crashed)
{
    const Session session = m_sessions.take(gameId);
    session.process->deleteLater();
    const qint64 elapsedMs = session.timer.elapsed();
    const qint64 seconds = elapsedMs / 1000;
    const QString how = crashed ? QStringLiteral("was ended by signal %1").arg(exitCode)
                                : QStringLiteral("exited with code %1").arg(exitCode);
    qCInfo(lcLaunch).noquote() << QStringLiteral("%1 (%2) %3 after %4 s").arg(session.emulatorName, gameId, how).arg(seconds);

    if ((crashed || exitCode != 0) && seconds < kQuickExitSeconds) {
        for (const QString &line : lastLines(session.logPath, 20))
            qCWarning(lcLaunch).noquote() << QStringLiteral("%1: %2").arg(session.emulatorName, line);
        qCWarning(lcLaunch).noquote() << "full output in" << session.logPath;
        emit message(tr("%1 closed right away; see the log").arg(session.emulatorName));
    } else if (seconds < m_settings.minSessionSeconds) {
        qCInfo(lcLaunch).noquote() << QStringLiteral("shorter than min_session_seconds (%1); not counted")
                                          .arg(m_settings.minSessionSeconds);
    } else {
        const QString error = m_library->recordPlaySession(gameId, sessionMinutes(elapsedMs), QDateTime::currentDateTime());
        if (!error.isEmpty())
            emit message(error);
    }
    emit runningChanged();
    emit sessionEnded(gameId);
}

QString GameRunner::openEmulator(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    if (!game)
        return {};
    if (const SteamRef steam = steamRefOf(*game))
        return runSteam({QStringLiteral("steam://nav/games/details/%1").arg(steam.runId)}, true);
    if (game->platform == kLinuxPlatform)
        return tr("A Linux game has no emulator or Steam page to open");
    if (!game->rom)
        return tr("Opening %1 is not implemented yet").arg(game->platformName);
    QString emulatorId;
    if (const QString problem = chooseEmulator(gameId, &emulatorId); !problem.isEmpty())
        return problem;
    const EmulatorDef *emulator = Emulators::find(m_library->emulators(), emulatorId);
    const LaunchCommand command = buildLaunchCommand(*emulator, m_library->installs().value(emulatorId), {});
    qCInfo(lcLaunch).noquote() << QStringLiteral("opening %1: %2").arg(emulator->name, command.toString());

    // Watched only to reap it when it exits; its time isn't the game's.
    auto *process = new DetachedProcess(this);
    const QString error = process->start(command, QDir::homePath(),
                                         QStringLiteral("%1/launch/%2.log").arg(m_logDir, emulatorId));
    if (!error.isEmpty()) {
        delete process;
        return tr("Could not start %1: %2").arg(emulator->name, error);
    }
    connect(process, &DetachedProcess::finished, this, [this, process, gameId] {
        process->deleteLater();
        emit emulatorExited(gameId);
    });
    return {};
}

QString GameRunner::openFolder(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    if (!game)
        return {};
    const bool steam = game->platform == kSteamPlatform && GameId::steamAppId(game->id) > 0;
    const bool proton = runsProgram(game->platform);
    if (!game->rom && !steam && !proton)
        return tr("Opening the folder of %1 games is not implemented yet").arg(game->platformName);
    if (!game->installed)
        return proton ? missingProgram(*game) : steam ? tr("%1 isn't installed").arg(game->title) : missingRom(*game);
    const QString folder = proton ? (!game->installDir.isEmpty() ? game->installDir : QFileInfo(game->exe).absolutePath())
                         : steam  ? game->installDir
                                  : QFileInfo(game->romPath).absolutePath();
    if (proton && !QFileInfo(folder).isDir())
        return tr("%1's folder isn't found: %2").arg(game->title, folder);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
        return tr("Could not open %1").arg(folder);
    return {};
}

QString GameRunner::saveFolder(const QString &gameId) const
{
    return findSaveFolder(gameId, true);
}

QString GameRunner::foundSaveFolder(const QString &gameId) const
{
    return findSaveFolder(gameId, false);
}

QString GameRunner::findSaveFolder(const QString &gameId, bool ownFirst) const
{
    const Game *game = m_library->game(gameId);
    if (!game)
        return {};
    const LibraryStore &store = m_library->store();
    const QString home = store.installLocations().home;
    const auto isDir = [](const QString &path) { return !path.isEmpty() && QFileInfo(path).isDir(); };

    if (ownFirst) {
        const QString own = store.snapshot().content.games.value(gameId).saveFolder;
        if (const QString path = own.isEmpty() ? QString() : resolveRomFolder(own, m_dataDir, home); isDir(path))
            return path;
    }
    if (game->rom) {
        QString emulatorId;
        chooseEmulator(gameId, &emulatorId);
        const EmulatorDef *emulator =
            Emulators::find(m_library->emulators(), emulatorId.isEmpty() ? missingEmulator(gameId) : emulatorId);
        if (!emulator)
            return {};
        if (!emulator->savesBesideRom.isEmpty() && game->installed) {
            if (const QString folder = QFileInfo(game->romPath).absolutePath(); isDir(folder))
                return folder;
        }
        const EmulatorInstall install = m_library->installs().value(emulator->id);
        if (!install.found())
            return {};
        for (const QString &path : Emulators::installPaths(emulator->saves, install, home)) {
            if (isDir(path))
                return path;
        }
        return {};
    }
    const QString user = QStringLiteral("/pfx/drive_c/users/steamuser");
    if (game->platform == kProtonPlatform) {
        const QString prefix = m_library->protonPrefix(game->shortcut);
        return prefix.isEmpty() || !isDir(prefix + user) ? QString() : prefix + user;
    }
    if (const int appId = GameId::steamAppId(game->id); game->platform == kSteamPlatform && appId > 0) {
        for (const QString &steamapps : store.snapshot().steam.libraries) {
            if (const QString path = steamapps + QStringLiteral("/compatdata/%1").arg(appId) + user; isDir(path))
                return path;
        }
    }
    return {};
}

QString GameRunner::openSaveFolder(const QString &gameId)
{
    const Game *game = m_library->game(gameId);
    if (!game)
        return {};
    const QString folder = saveFolder(gameId);
    if (folder.isEmpty())
        return tr("%1's save folder isn't found; set it in its properties").arg(game->title);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
        return tr("Could not open %1").arg(folder);
    return {};
}

QString GameRunner::shownPath(const QString &path) const
{
    return m_library->store().writtenPath(path);
}

QString GameRunner::openDataFolder()
{
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDir)))
        return tr("Could not open %1").arg(m_dataDir);
    return {};
}

QString GameRunner::problemFile(const QString &message) const
{
    static const QRegularExpression pattern(QStringLiteral("^(\\S+\\.toml):\\d+:"));
    const QRegularExpressionMatch match = pattern.match(message);
    if (!match.hasMatch())
        return {};
    const QString path = QDir(m_dataDir).absoluteFilePath(match.captured(1));
    return QFileInfo::exists(path) ? path : QString();
}

QString GameRunner::openFile(const QString &path)
{
    // xdg-open starts a Terminal=true editor without a terminal, so it inherits the launcher's:
    // the Linux console when started from the desktop.
    const QStringList env = programEnvironment(QProcess::systemEnvironment());
    if (const QString entry = defaultDesktopFile(path, env); !entry.isEmpty())
        if (const QStringList command = terminalEntryCommand(entry, path); !command.isEmpty())
            if (startInTerminal(command, env))
                return {};
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        return path;
    return {};
}
