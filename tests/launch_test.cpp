#include "detachedprocess.h"
#include "gamerunner.h"
#include "launchcommand.h"
#include "launchoptions.h"
#include "launchsettings.h"
#include "library.h"
#include "restart.h"

#include <QDirIterator>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = to + source.mid(from.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::copy(source, target);
    }
}

void writeFile(const QString &path, const QByteArray &text, bool executable = false)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
    file.close();
    if (executable)
        file.setPermissions(file.permissions() | QFile::ExeOwner);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

EmulatorDef emulator(const QStringList &command, const QStringList &args = {})
{
    EmulatorDef e;
    e.id = QStringLiteral("test");
    e.name = QStringLiteral("Test");
    e.command = command;
    e.args = args;
    return e;
}

EmulatorInstall installed(EmulatorInstall::Kind kind, const QString &program)
{
    EmulatorInstall i;
    i.kind = kind;
    i.program = program;
    return i;
}

// Stands in for an emulator: records what it was given, then runs for FAKE_SECONDS and
// exits with FAKE_EXIT.
const QByteArray kFakeEmulator = R"(#!/bin/sh
dir=$(dirname "$0")
printf '%s\n' "$@" > "$dir/args.txt"
pwd > "$dir/cwd.txt"
echo $$ > "$dir/pid.txt"
if [ -n "$FAKE_FD" ] && [ -e "/proc/$$/fd/$FAKE_FD" ]; then echo leaked > "$dir/leak.txt"; fi
echo "fake emulator: something went wrong" >&2
sleep "${FAKE_SECONDS:-0}"
exit "${FAKE_EXIT:-0}"
)";

// Stands in for Steam's command: appends each call's arguments, one call per line. With
// FAKE_GAME_SECONDS, a rungameid URL also starts a stand-in game with the command
// line Steam's reaper has, for that long: AppId is the URL's ID, or FAKE_REAPER_ID.
// -shutdown kills FAKE_STEAM_PID.
const QByteArray kFakeSteam = R"(#!/bin/sh
echo "$*" >> "$(dirname "$0")/steam.txt"
for arg in "$@"; do
    case "$arg" in
    steam:*rungameid*)
        if [ -n "$FAKE_GAME_SECONDS" ]; then
            # Like a Proton game: a short step of Steam's own under the same AppId, then the game.
            app="AppId=${FAKE_REAPER_ID:-${arg##*[!0-9]}}"
            (sh -c 'sleep 2; true' SteamLaunch "$app" -- iscriptevaluator; sleep 1
             sh -c 'sleep "$0"; true' "$FAKE_GAME_SECONDS" SteamLaunch "$app" -- game) >/dev/null 2>&1 &
        fi ;;
    -shutdown)
        # The running Steam stand-in closes, if the test says which one it is.
        if [ -n "$FAKE_STEAM_PID" ]; then kill "$FAKE_STEAM_PID"; fi ;;
    esac
done
)";

// Stands in for a Linux game's launcher script: records what it was given and where, the
// variables the tests look at, then runs for FAKE_SECONDS and exits with FAKE_EXIT. With
// FAKE_CHILD_SECONDS, it leaves a child behind that runs that long, then writes child.txt.
const QByteArray kFakeGame = R"(#!/bin/sh
dir=$(dirname "$0")
printf '%s\n' "$@" > "$dir/args.txt"
pwd > "$dir/cwd.txt"
printf 'FOO=%s\nAPPDIR=%s\nLD_LIBRARY_PATH=%s\n' "$FOO" "${APPDIR-unset}" "${LD_LIBRARY_PATH-unset}" > "$dir/env.txt"
if [ -n "$FAKE_CHILD_SECONDS" ]; then
    (sleep "$FAKE_CHILD_SECONDS"; echo done > "$dir/child.txt") &
fi
sleep "${FAKE_SECONDS:-0}"
exit "${FAKE_EXIT:-0}"
)";

// Stands in for a wrapper like mangohud: records its arguments, then runs them.
const QByteArray kFakeWrapper = R"(#!/bin/sh
printf '%s\n' "$@" > "$(dirname "$0")/wrapper.txt"
exec "$@"
)";

} // namespace

class LaunchTest : public QObject
{
    Q_OBJECT

private slots:
    void settings_data();
    void settings();
    void settingsDefaults();

    void command_data();
    void command();
    void commandWithGameArgs();
    void commandToString();
    void launchOptions_data();
    void launchOptions();
    void chooseEmulator();
    void sessionMinutes();

    void processExitCode();
    void processStartError();

    void init();
    void cleanup();
    void launchRecordsSession();
    void shortSessionIsNotCounted();
    void quickFailureIsReported();
    void sameGameDoesNotStartTwice();
    void launchRomKeyGame();
    void cannotLaunch();
    void programEnvironment();
    void restartCommand();
    void linuxGameSession();
    void linuxGameRunsWhileItsSessionLives();
    void linuxGameQuickFailure();
    void linuxGameOptionsAndStartDir();
    void linuxGameRefused();
    void linuxGamePage();
    void steamGames();
    void steamUpdates();
    void downloadBar();
    void steamGameRunning();
    void steamCommand();
    void closeSteamAndApply();
    void launchWithQueuedEdits();
    void openEmulatorAndFolder();
    void protonGames();
    void protonGameRunning();
    void locateProgram();
    void gameOutlivesTheLauncher();
    void setLaunchValue_data();
    void setLaunchValue();

private:
    void openLibrary(const QByteArray &settings = {});
    // A new GameRunner on the open library, handing Steam's commands to the fake one.
    void startRunner();
    // A copy of the tests' fake Steam (it has a user's localconfig.vdf; the demo's doesn't)
    // that can run: a copy of `sleep` named like its binary. Returns its folder.
    QString copyFakeSteam();
    // Starts that copy as the running Steam (in steam.pid).
    void startFakeSteam(const QString &root, QProcess &steam);
    QString fakeDir() const { return m_dir->filePath(QStringLiteral("fake")); }

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_data;
    QString m_stateDir; // the Steam sync's; none unless a test sets it
    std::unique_ptr<Library> m_library;
    std::unique_ptr<GameRunner> m_runner;
};

void LaunchTest::settings_data()
{
    QTest::addColumn<QByteArray>("toml");
    QTest::addColumn<bool>("minimize");
    QTest::addColumn<int>("minSeconds");
    QTest::addColumn<QString>("warning");

    QTest::newRow("both") << QByteArray("[launch]\nwhile_running = \"minimize\"\nmin_session_seconds = 5\n") << true
                          << 5 << "";
    QTest::newRow("other sections") << QByteArray("[appearance]\ntheme = \"x\"\n[launch]\nwhile_running = \"stay\"\n")
                                    << false << 30 << "";
    QTest::newRow("bad mode") << QByteArray("[launch]\nwhile_running = \"minimise\"\n") << false << 30
                              << "settings.toml:2: while_running must be \"stay\" or \"minimize\" — did you mean \"minimize\"?";
    QTest::newRow("bad seconds") << QByteArray("[launch]\nmin_session_seconds = -1\n") << false << 30
                                 << "settings.toml:2: min_session_seconds must be a whole number from 0 to 3600";
    QTest::newRow("unknown key") << QByteArray("[launch]\nmin_sesion_seconds = 5\n") << false << 30
                                 << "settings.toml:2: unknown key min_sesion_seconds — did you mean \"min_session_seconds\"?";
    QTest::newRow("not a section") << QByteArray("launch = 1\n") << false << 30
                                   << "settings.toml:1: launch must be a [launch] section";
}

void LaunchTest::settings()
{
    QFETCH(QByteArray, toml);
    QFETCH(bool, minimize);
    QFETCH(int, minSeconds);
    QFETCH(QString, warning);

    const Parsed<LaunchSettings> r = parseLaunchSettings(toml, QStringLiteral("settings.toml"));
    QVERIFY(r.ok);
    QCOMPARE(r.value.whileRunning == LaunchSettings::WhileRunning::Minimize, minimize);
    QCOMPARE(r.value.minSessionSeconds, minSeconds);
    QCOMPARE(r.warnings, warning.isEmpty() ? QStringList() : QStringList{warning});
}

void LaunchTest::settingsDefaults()
{
    const Parsed<LaunchSettings> empty = parseLaunchSettings({}, QStringLiteral("settings.toml"));
    QVERIFY(empty.ok);
    QCOMPARE(empty.value, LaunchSettings());
    const Parsed<LaunchSettings> broken = parseLaunchSettings("[launch\n", QStringLiteral("settings.toml"));
    QVERIFY(!broken.ok);
    QVERIFY(broken.warnings.first().startsWith(QStringLiteral("settings.toml:1:")));
}

void LaunchTest::command_data()
{
    using Kind = EmulatorInstall::Kind;
    QTest::addColumn<QStringList>("command");
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("rom");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("appimage") << QStringList{"{exe}", "{args}", "{rom}"} << QStringList{"--fullscreen"}
                              << int(Kind::AppImage) << "/roms/a b.nsp"
                              << QStringList{"/apps/emu.AppImage", "--fullscreen", "/roms/a b.nsp"};
    QTest::newRow("flatpak") << QStringList{"{exe}", "{args}", "{rom}"} << QStringList{} << int(Kind::Flatpak)
                             << "/roms/a.nsp" << QStringList{"flatpak", "run", "io.github.ryubing.Ryujinx", "/roms/a.nsp"};
    QTest::newRow("args spliced") << QStringList{"{exe}", "-f", "{args}", "-e", "{rom}"} << QStringList{"-a", "-b"}
                                  << int(Kind::Executable) << "/r.iso"
                                  << QStringList{"/usr/bin/emu", "-f", "-a", "-b", "-e", "/r.iso"};
    QTest::newRow("rom inside an argument") << QStringList{"{exe}", "--game={rom}"} << QStringList{}
                                            << int(Kind::Path) << "/r.iso" << QStringList{"/opt/emu", "--game=/r.iso"};
    // Without a ROM, the arguments for it go too: those between {args} and {rom}.
    QTest::newRow("no rom") << QStringList{"{exe}", "{args}", "-e", "{rom}", "--game={rom}"} << QStringList{"-f"}
                            << int(Kind::Executable) << "" << QStringList{"/usr/bin/emu", "-f"};
    QTest::newRow("no rom, batch") << QStringList{"{exe}", "{args}", "-batch", "--", "{rom}"} << QStringList{}
                                   << int(Kind::Executable) << "" << QStringList{"/usr/bin/emu"};
    QTest::newRow("no rom, after it") << QStringList{"{exe}", "-g", "{rom}", "--quit"} << QStringList{}
                                      << int(Kind::Executable) << "" << QStringList{"/usr/bin/emu", "--quit"};
}

void LaunchTest::command()
{
    QFETCH(QStringList, command);
    QFETCH(QStringList, args);
    QFETCH(int, kind);
    QFETCH(QString, rom);
    QFETCH(QStringList, expected);

    const auto k = EmulatorInstall::Kind(kind);
    const QString program = k == EmulatorInstall::Kind::Flatpak ? QStringLiteral("io.github.ryubing.Ryujinx")
                          : k == EmulatorInstall::Kind::AppImage ? QStringLiteral("/apps/emu.AppImage")
                          : k == EmulatorInstall::Kind::Path     ? QStringLiteral("/opt/emu")
                                                                 : QStringLiteral("/usr/bin/emu");
    const LaunchCommand c = buildLaunchCommand(emulator(command, args), installed(k, program), rom);
    QCOMPARE(QStringList{c.program} + c.arguments, expected);
}

void LaunchTest::commandWithGameArgs()
{
    const EmulatorDef e = emulator({"{exe}", "{args}", "-g", "{rom}"}, {"-f"});
    const EmulatorInstall install = installed(EmulatorInstall::Kind::Executable, "/usr/bin/emu");
    const QStringList gameArgs = {"--mod", "a b"};
    const LaunchCommand c = buildLaunchCommand(e, install, "/r.nsp", gameArgs);
    QCOMPARE(QStringList{c.program} + c.arguments, (QStringList{"/usr/bin/emu", "-f", "--mod", "a b", "-g", "/r.nsp"}));
}

void LaunchTest::commandToString()
{
    const LaunchCommand c{QStringLiteral("/apps/emu.AppImage"),
                          {QStringLiteral("--fullscreen"), QStringLiteral("/roms/Mario Kart [0100].nsp"),
                           QStringLiteral("it's")}};
    QCOMPARE(c.toString(), QStringLiteral("/apps/emu.AppImage --fullscreen '/roms/Mario Kart [0100].nsp' 'it'\\''s'"));
}

using Env = QList<std::pair<QString, QString>>;

void LaunchTest::launchOptions_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<Env>("env");
    QTest::addColumn<QStringList>("wrapper");
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<QString>("problem");

    QTest::newRow("empty") << "" << Env{} << QStringList{} << QStringList{} << "";
    QTest::newRow("spaces only") << "  \t " << Env{} << QStringList{} << QStringList{} << "";
    QTest::newRow("command only") << "%command%" << Env{} << QStringList{} << QStringList{} << "";
    QTest::newRow("variables") << "A=1 B= %command%" << Env{{"A", "1"}, {"B", ""}} << QStringList{} << QStringList{}
                               << "";
    QTest::newRow("wrapper") << "gamemoderun mangohud %command%" << Env{} << QStringList{"gamemoderun", "mangohud"}
                             << QStringList{} << "";
    QTest::newRow("arguments") << "%command% -windowed --fps 60" << Env{} << QStringList{}
                               << QStringList{"-windowed", "--fps", "60"} << "";
    QTest::newRow("all together") << "MANGOHUD=1 DXVK_HUD=fps gamemoderun %command% -x"
                                  << Env{{"MANGOHUD", "1"}, {"DXVK_HUD", "fps"}} << QStringList{"gamemoderun"}
                                  << QStringList{"-x"} << "";
    // Only the leading assignments are variables: after the wrapper, one is its argument.
    QTest::newRow("assignment after the wrapper") << "A=1 env B=2 %command%" << Env{{"A", "1"}}
                                                  << QStringList{"env", "B=2"} << QStringList{} << "";
    QTest::newRow("not a variable") << "1A=x %command%" << Env{} << QStringList{"1A=x"} << QStringList{} << "";
    // Quotes keep spaces in one word, and go.
    QTest::newRow("quotes") << R"(WINEDLLOVERRIDES="a,b=n" 'my wrapper' %command% --name "Tux the \"Penguin\"" it\'s)"
                            << Env{{"WINEDLLOVERRIDES", "a,b=n"}} << QStringList{"my wrapper"}
                            << QStringList{"--name", "Tux the \"Penguin\"", "it's"} << "";
    QTest::newRow("variable with spaces") << "TITLE='a b' %command%" << Env{{"TITLE", "a b"}} << QStringList{}
                                          << QStringList{} << "";
    // Without %command%, Steam puts the whole string after the game's command.
    QTest::newRow("no command") << "A=1 -windowed" << Env{} << QStringList{} << QStringList{"A=1", "-windowed"} << "";
    QTest::newRow("unclosed double quote") << "%command% --name \"Tux" << Env{} << QStringList{}
                                           << QStringList{"--name", "Tux"}
                                           << "the launch options have a \" quote that isn't closed";
    QTest::newRow("unclosed single quote") << "'mango %command%" << Env{} << QStringList{}
                                           << QStringList{"mango %command%"}
                                           << "the launch options have a ' quote that isn't closed";
}

void LaunchTest::launchOptions()
{
    QFETCH(QString, options);
    QFETCH(Env, env);
    QFETCH(QStringList, wrapper);
    QFETCH(QStringList, args);
    QFETCH(QString, problem);
    const LaunchOptions::Parts parts = LaunchOptions::parse(options);
    QCOMPARE(parts.env, env);
    QCOMPARE(parts.wrapper, wrapper);
    QCOMPARE(parts.args, args);
    QCOMPARE(parts.problem, problem);
}

void LaunchTest::programEnvironment()
{
    // Not an AppImage: the environment as it is.
    const QStringList plain = {"HOME=/home/u", "PATH=/usr/bin:/bin", "LD_LIBRARY_PATH=/opt/lib"};
    QCOMPARE(::programEnvironment(plain), plain);

    // An AppImage: its own variables go, and its folders from the others; a variable left
    // with nothing goes too.
    const QStringList appImage = {"HOME=/home/u",
                                  "APPDIR=/tmp/.mount_launch/",
                                  "APPIMAGE=/home/u/Launcher.AppImage",
                                  "ARGV0=./Launcher.AppImage",
                                  "OWD=/home/u",
                                  "PATH=/tmp/.mount_launch/usr/bin:/usr/bin:/bin",
                                  "LD_LIBRARY_PATH=/tmp/.mount_launch/usr/lib",
                                  "QT_PLUGIN_PATH=/tmp/.mount_launch/usr/plugins",
                                  "XDG_DATA_DIRS=/tmp/.mount_launch/usr/share:/usr/share",
                                  "QT_QPA_PLATFORM=wayland",
                                  "OTHER=/tmp/.mount_launcher/x"};
    QCOMPARE(::programEnvironment(appImage), (QStringList{"HOME=/home/u", "PATH=/usr/bin:/bin",
                                                          "XDG_DATA_DIRS=/usr/share", "QT_QPA_PLATFORM=wayland",
                                                          "OTHER=/tmp/.mount_launcher/x"}));

    // The command's variables win.
    QCOMPARE(::programEnvironment(plain, {{"PATH", "/x"}, {"NEW", "a b"}}),
             (QStringList{"HOME=/home/u", "LD_LIBRARY_PATH=/opt/lib", "PATH=/x", "NEW=a b"}));
}

void LaunchTest::restartCommand()
{
    // The same program and arguments, then the flag.
    const QStringList args = {"./build/lattis", "-platform", "wayland", "extra"};
    QVERIFY(!Restart::waits(args));
    Restart::Command c = Restart::command(args, "/src/build/lattis", {});
    QCOMPARE(c.program, "/src/build/lattis");
    QCOMPARE(c.arguments, (QStringList{"-platform", "wayland", "extra", Restart::kWaitFlag}));

    // Restarted again: the flag once.
    QStringList again = {c.program};
    again += c.arguments;
    QVERIFY(Restart::waits(again));
    c = Restart::command(again, "/src/build/lattis", {});
    QCOMPARE(c.arguments, (QStringList{"-platform", "wayland", "extra", Restart::kWaitFlag}));

    // An AppImage: the file, not the program inside its mount.
    c = Restart::command({"/tmp/.mount_launch/usr/bin/lattis"}, "/tmp/.mount_launch/usr/bin/lattis",
                         "/home/u/Launcher.AppImage");
    QCOMPARE(c.program, "/home/u/Launcher.AppImage");
    QCOMPARE(c.arguments, QStringList{Restart::kWaitFlag});
}

namespace {

const QString kLinuxGame = QStringLiteral("linux-supertuxkart");

} // namespace

void LaunchTest::linuxGameSession()
{
    writeFile(m_data + "/linux-games/SuperTuxKart/run_game.sh", kFakeGame, true);
    openLibrary("[launch]\nmin_session_seconds = 1\n");
    qputenv("FAKE_SECONDS", "1");
    const QByteArray before = readFile(m_data + "/library/games/linux-supertuxkart.toml");

    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->launch(kLinuxGame), QString());
    QCOMPARE(started.size(), 1);
    QCOMPARE(m_runner->running(), QStringList{kLinuxGame});
    QCOMPARE(m_runner->launch(kLinuxGame), QString()); // not twice
    QCOMPARE(started.size(), 1);
    QVERIFY(ended.wait(10000));
    QVERIFY(m_runner->running().isEmpty());
    QVERIFY(messages.isEmpty());

    // Run directly, without arguments, in its program's folder; its output in the log.
    const QString dir = m_data + "/linux-games/SuperTuxKart";
    QCOMPARE(readFile(dir + "/args.txt"), QByteArray("\n")); // printf with no arguments
    QCOMPARE(readFile(dir + "/cwd.txt").trimmed(), QFile::encodeName(dir));
    QVERIFY(QFile::exists(m_dir->filePath("cache/launch/linux-supertuxkart.log")));

    // Recorded in its file: 1 s adds 0 minutes to 640, and sets last played.
    const QByteArray file = readFile(m_data + "/library/games/linux-supertuxkart.toml");
    QVERIFY2(file.contains("play_minutes = 640\n"), file.constData());
    QVERIFY(file != before);
    QVERIFY(!file.contains("last_played = 2026-09-20T21:14:00-03:00"));
    QVERIFY(m_library->game(kLinuxGame)->lastPlayed > QDateTime::currentDateTime().addSecs(-60));
}

void LaunchTest::linuxGameRunsWhileItsSessionLives()
{
    // The script starts the game and exits at once: running until the game ends too.
    writeFile(m_data + "/linux-games/SuperTuxKart/run_game.sh", kFakeGame, true);
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    m_runner->setSessionPollInterval(100);
    qputenv("FAKE_CHILD_SECONDS", "2");

    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QElapsedTimer timer;
    timer.start();
    QCOMPARE(m_runner->launch(kLinuxGame), QString());
    // Not uninstalled while it runs, even once its script has exited: nothing moved.
    QTest::qWait(500);
    bool trashed = false;
    m_library->setTrash([&trashed](const QString &, QString *) { return trashed = true; });
    QCOMPARE(m_runner->uninstallProton(kLinuxGame, true, false, false, QStringLiteral("remove"), false)
                 .value("error").toString(),
             QStringLiteral("SuperTuxKart is running; close it first"));
    QVERIFY(!trashed);
    QVERIFY(QFile::exists(m_data + "/library/games/linux-supertuxkart.toml"));
    QVERIFY(ended.wait(10000));
    QVERIFY2(timer.elapsed() >= 1900, qPrintable(QString::number(timer.elapsed())));
    QVERIFY(QFile::exists(m_data + "/linux-games/SuperTuxKart/child.txt"));
    qunsetenv("FAKE_CHILD_SECONDS");
}

void LaunchTest::linuxGameQuickFailure()
{
    writeFile(m_data + "/linux-games/SuperTuxKart/run_game.sh", kFakeGame, true);
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    qputenv("FAKE_EXIT", "3");
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    const QByteArray before = readFile(m_data + "/library/games/linux-supertuxkart.toml");
    QCOMPARE(m_runner->launch(kLinuxGame), QString());
    QVERIFY(ended.wait(10000));
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().first().toString(), QStringLiteral("SuperTuxKart closed right away; see the log"));
    QCOMPARE(readFile(m_data + "/library/games/linux-supertuxkart.toml"), before);
}

void LaunchTest::linuxGameOptionsAndStartDir()
{
    // Variables, a wrapper and arguments from the launch options, in start_dir, without the
    // launcher's AppImage variables.
    const QString dir = m_data + "/linux-games/SuperTuxKart";
    writeFile(dir + "/run_game.sh", kFakeGame, true);
    writeFile(dir + "/wrap per.sh", kFakeWrapper, true);
    const QString game = m_data + "/library/games/linux-supertuxkart.toml";
    writeFile(game, readFile(game) + "launch_options = 'FOO=\"a b\" \"" + QFile::encodeName(dir)
                        + "/wrap per.sh\" %command% --x \"y z\"'\nstart_dir = \"linux-games\"\n");
    qputenv("APPDIR", QFile::encodeName(m_dir->filePath("mount")));
    qputenv("LD_LIBRARY_PATH", QFile::encodeName(m_dir->filePath("mount/usr/lib")));
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    const QString error = m_runner->launch(kLinuxGame);
    qunsetenv("APPDIR");
    qunsetenv("LD_LIBRARY_PATH");
    QCOMPARE(error, QString());
    QVERIFY(ended.wait(10000));

    QCOMPARE(readFile(dir + "/wrapper.txt"), QFile::encodeName(dir) + "/run_game.sh\n--x\ny z\n");
    QCOMPARE(readFile(dir + "/args.txt"), QByteArray("--x\ny z\n"));
    QCOMPARE(readFile(dir + "/cwd.txt").trimmed(), QFile::encodeName(m_data + "/linux-games"));
    QCOMPARE(readFile(dir + "/env.txt"), QByteArray("FOO=a b\nAPPDIR=unset\nLD_LIBRARY_PATH=unset\n"));
}

void LaunchTest::linuxGameRefused()
{
    const QString exe = m_data + "/linux-games/SuperTuxKart/run_game.sh";
    writeFile(exe, kFakeGame, false);
    QVERIFY(QFile::setPermissions(exe, QFile::ReadOwner | QFile::WriteOwner));
    writeFile(m_data + "/library/games/linux-quote.toml",
              "title = \"Quote\"\nplatform = \"linux\"\nexe = \"linux-games/SuperTuxKart/wrap.sh\"\n"
              "launch_options = \"%command% --name \\\"Tux\"\n");
    writeFile(m_data + "/linux-games/SuperTuxKart/wrap.sh", kFakeWrapper, true);
    writeFile(m_data + "/library/games/linux-nowhere.toml",
              "title = \"Nowhere\"\nplatform = \"linux\"\nexe = \"linux-games/SuperTuxKart/wrap.sh\"\n"
              "start_dir = \"no/such/folder\"\n");
    writeFile(m_data + "/library/games/linux-bare.toml", "title = \"Bare\"\nplatform = \"linux\"\n");
    openLibrary();
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);

    QCOMPARE(m_runner->launch(kLinuxGame),
             "SuperTuxKart's program isn't executable; make it executable in its file properties, or with chmod +x \""
                 + exe + "\"");
    QCOMPARE(m_runner->launch(QStringLiteral("linux-factorio")),
             QStringLiteral("Factorio's program isn't found; locate it to play it"));
    QCOMPARE(m_runner->launch(QStringLiteral("linux-quote")),
             QStringLiteral("Can't launch Quote: the launch options have a \" quote that isn't closed"));
    QCOMPARE(m_runner->launch(QStringLiteral("linux-nowhere")),
             "Nowhere's start folder isn't found: " + m_data + "/no/such/folder");
    QCOMPARE(m_runner->launch(QStringLiteral("linux-bare")),
             QStringLiteral("Bare has no program; choose it in its settings"));
    QVERIFY(started.isEmpty());
}

void LaunchTest::linuxGamePage()
{
    openLibrary();
    m_library->setInstalledOnly(false);
    auto row = [this](const QString &id) { return m_library->gameAt(m_library->rowOfGame(id)); };
    const QString factorio = QStringLiteral("linux-factorio");
    const QString factorioFile = m_data + "/library/games/linux-factorio.toml";

    // Launch's tooltip: "Launch", or through the wrapper by its program's name.
    QCOMPARE(row(kLinuxGame).value("launchTip").toString(), QStringLiteral("Launch"));
    QVERIFY(!row(kLinuxGame).value("gameNotFound").toBool());
    QCOMPARE(m_library->setGameValue(kLinuxGame, QStringLiteral("launch_options"),
                                     QStringLiteral("DXVK_HUD=1 /usr/bin/gamemoderun mangohud %command% -f")),
             QString());
    QTRY_COMPARE(row(kLinuxGame).value("launchTip").toString(), QStringLiteral("Launch with gamemoderun"));
    QCOMPARE(m_library->setGameValue(kLinuxGame, QStringLiteral("launch_options"), QStringLiteral("--fullscreen")),
             QString());
    QTRY_COMPARE(row(kLinuxGame).value("launchTip").toString(), QStringLiteral("Launch"));
    // Its editor's presets leave out those only Proton reads.
    QStringList presets;
    for (const QVariant &p : m_library->gameForEditing(kLinuxGame).value("steamPresets").toList())
        presets.append(p.toMap().value("id").toString());
    QVERIFY(presets.contains(QStringLiteral("mangohud")));
    QVERIFY(!presets.contains(QStringLiteral("wayland")));

    // "Game not found": located, and nothing to open until then.
    QVERIFY(row(factorio).value("gameNotFound").toBool());
    QVERIFY(m_library->gameForEditing(factorio).value("gameNotFound").toBool());
    QCOMPARE(row(factorio).value("launchTip").toString(), QStringLiteral("Locate the game's program"));
    QCOMPARE(m_runner->openFolder(factorio), QStringLiteral("Factorio's program isn't found; locate it to play it"));
    QCOMPARE(m_runner->openEmulator(kLinuxGame), QStringLiteral("A Linux game has no emulator or Steam page to open"));

    // Refused: not a program, a shared library; nothing written.
    const QString dir = m_dir->filePath(QStringLiteral("games/factorio/bin/x64/"));
    writeFile(dir + "notes.txt", "hello");
    QByteArray library(128, '\0');
    library.replace(0, 4, "\x7f" "ELF");
    library[4] = 2;
    library[5] = 1;
    library[16] = 3;  // ET_DYN
    library[18] = 62; // x86-64
    writeFile(dir + "libfoo.so", library);
    const QByteArray before = readFile(factorioFile);
    QCOMPARE(m_library->locateProgram(factorio, dir + "notes.txt"), QStringLiteral("notes.txt isn't a Linux program"));
    QCOMPARE(m_library->locateProgram(factorio, dir + "libfoo.so"),
             QStringLiteral("libfoo.so is a shared library, not a program"));
    QCOMPARE(m_library->saveGame(factorio, {{"exe", dir + "notes.txt"}}), QStringLiteral("notes.txt isn't a Linux program"));
    QCOMPARE(readFile(factorioFile), before);

    // A script without its executable bit: made executable, and the game is found.
    const QString script = dir + "factorio";
    writeFile(script, "#!/bin/sh\nsleep 1\n", false);
    QCOMPARE(m_library->locateProgram(factorio, QUrl::fromLocalFile(script).toString()), QString());
    QVERIFY(QFileInfo(script).isExecutable());
    QVERIFY(readFile(factorioFile).contains("exe = \"" + QFile::encodeName(script) + "\"\n"));
    QTRY_VERIFY(row(factorio).value("installed").toBool());
    QVERIFY(!row(factorio).value("gameNotFound").toBool());
    QCOMPARE(row(factorio).value("launchTip").toString(), QStringLiteral("Launch"));
    // Changed in the editor: the same checks, and the bit set.
    const QString other = dir + "other.sh";
    writeFile(other, "#!/bin/sh\n", false);
    QCOMPARE(m_library->saveGame(factorio, {{"exe", other}}), QString());
    QVERIFY(QFileInfo(other).isExecutable());
    // Its install folder is gone: Open folder says so.
    QCOMPARE(m_library->setGameValue(factorio, QStringLiteral("install_dir"), m_dir->filePath(QStringLiteral("gone"))),
             QString());
    QTRY_VERIFY(m_runner->openFolder(factorio).startsWith(QStringLiteral("Factorio's folder isn't found")));
}

void LaunchTest::chooseEmulator()
{
    using Kind = EmulatorInstall::Kind;
    const QHash<QString, EmulatorInstall> installs = {
        {"azahar", installed(Kind::AppImage, "/a")},
        {"citra", installed(Kind::Path, "/c")},
        {"lime", EmulatorInstall()}, // not found
    };
    QCOMPARE(Emulators::choose("citra", {"azahar", "citra"}, installs), QStringLiteral("citra")); // pinned
    QCOMPARE(Emulators::choose("", {"azahar", "citra"}, installs), QStringLiteral("azahar"));     // preferred
    QCOMPARE(Emulators::choose("", {"lime", "citra"}, installs), QStringLiteral("citra"));        // fallback
    QCOMPARE(Emulators::choose("lime", {"lime", "citra"}, installs), QStringLiteral("citra"));    // pin not installed
    QCOMPARE(Emulators::choose("", {"lime", "nope"}, installs), QString());                       // none
}

void LaunchTest::sessionMinutes()
{
    QCOMPARE(GameRunner::sessionMinutes(0), 0);
    QCOMPARE(GameRunner::sessionMinutes(29'999), 0);
    QCOMPARE(GameRunner::sessionMinutes(30'000), 1);
    QCOMPARE(GameRunner::sessionMinutes(89'999), 1);
    QCOMPARE(GameRunner::sessionMinutes(90'000), 2);
    QCOMPARE(GameRunner::sessionMinutes(3'600'000), 60);
}

void LaunchTest::processExitCode()
{
    QTemporaryDir dir;
    DetachedProcess process;
    QSignalSpy finished(&process, &DetachedProcess::finished);
    const QString error = process.start({QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("pwd; exit 7")}},
                                        dir.path(), dir.filePath(QStringLiteral("out/log.txt")));
    QCOMPARE(error, QString());
    QVERIFY(process.pid() > 0);
    QVERIFY(finished.wait(5000));
    QCOMPARE(finished.first().at(0).toInt(), 7);
    QCOMPARE(finished.first().at(1).toBool(), false);
    QCOMPARE(readFile(dir.filePath(QStringLiteral("out/log.txt"))).trimmed(), QFile::encodeName(dir.path()));

    DetachedProcess killed;
    QSignalSpy killedFinished(&killed, &DetachedProcess::finished);
    QCOMPARE(killed.start({QStringLiteral("sleep"), {QStringLiteral("10")}}, dir.path(), dir.filePath("k.txt")), QString());
    ::kill(pid_t(killed.pid()), SIGTERM);
    QVERIFY(killedFinished.wait(5000));
    QCOMPARE(killedFinished.first().at(0).toInt(), SIGTERM);
    QCOMPARE(killedFinished.first().at(1).toBool(), true);
}

void LaunchTest::processStartError()
{
    QTemporaryDir dir;
    DetachedProcess process;
    const QString error = process.start({QStringLiteral("/nonexistent/emulator"), {}}, dir.path(), dir.filePath("l.txt"));
    QVERIFY2(error.contains(QStringLiteral("No such file")), qPrintable(error));
}

void LaunchTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_data = m_dir->filePath(QStringLiteral("data"));
    copyTree(QStringLiteral(DEMO_DATA_DIR), m_data);
    writeFile(fakeDir() + "/emulator.sh", kFakeEmulator, true);
    writeFile(m_data + "/library/emulators.toml",
              "[[emulator]]\nid = \"ryubing\"\npath = \"" + QFile::encodeName(fakeDir()) + "/emulator.sh\"\n"
              "args = [\"--fullscreen\"]\n");
    qputenv("FAKE_SECONDS", "0");
    qputenv("FAKE_EXIT", "0");
    qunsetenv("FAKE_FD");
    qunsetenv("FAKE_GAME_SECONDS");
    qunsetenv("FAKE_STEAM_PID");
    qunsetenv("FAKE_CHILD_SECONDS");
    m_stateDir.clear();
}

void LaunchTest::cleanup()
{
    m_runner.reset();
    m_library.reset();
    m_dir.reset();
}

void LaunchTest::openLibrary(const QByteArray &settings)
{
    if (!settings.isEmpty())
        writeFile(m_data + "/settings.toml", settings);
    m_library = std::make_unique<Library>(m_data, m_dir->filePath(QStringLiteral("cache")));
    InstallLocations nowhere;
    nowhere.home = m_dir->filePath(QStringLiteral("home"));
    m_library->store().setInstallLocations(nowhere);
    m_library->store().setSteamLocations({m_dir->filePath(QStringLiteral("no-home")), {}});
    m_library->store().setStateDir(m_stateDir);
    m_library->startRefresh();
    m_library->store().waitForIdle();
    QCoreApplication::processEvents();
    startRunner();
}

void LaunchTest::startRunner()
{
    m_runner = std::make_unique<GameRunner>(m_library.get(), m_data, m_dir->filePath(QStringLiteral("cache")));
    // The demo has a fake Steam, but `steam` on the PATH is the real one: never run it.
    writeFile(fakeDir() + "/steam.sh", kFakeSteam, true);
    m_runner->setSteamProgram({fakeDir() + "/steam.sh", QStringLiteral("--prefix")});
    m_runner->start();
}

QString LaunchTest::copyFakeSteam()
{
    const QString root = m_dir->filePath(QStringLiteral("steam"));
    copyTree(QStringLiteral(STEAM_DATA_DIR), root);
    const QString exe = root + QStringLiteral("/ubuntu12_32/steam");
    QDir().mkpath(root + QStringLiteral("/ubuntu12_32"));
    QFile::copy(QStandardPaths::findExecutable(QStringLiteral("sleep")), exe);
    QFile::setPermissions(exe, QFile::permissions(exe) | QFile::ExeUser);
    return root;
}

void LaunchTest::startFakeSteam(const QString &root, QProcess &steam)
{
    steam.start(root + QStringLiteral("/ubuntu12_32/steam"), {QStringLiteral("60")});
    QVERIFY(steam.waitForStarted());
    writeFile(m_dir->filePath(QStringLiteral("no-home/.steam/steam.pid")), QByteArray::number(steam.processId()));
}

void LaunchTest::launchRecordsSession()
{
    openLibrary("[launch]\nmin_session_seconds = 1\n");
    qputenv("FAKE_SECONDS", "1");
    // A descriptor the launcher has open, without close-on-exec, like a lock file.
    const int fd = ::open("/dev/null", O_RDONLY);
    QVERIFY(::dup2(fd, 200) == 200);
    qputenv("FAKE_FD", "200");

    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    ::close(fd);
    ::close(200);
    QCOMPARE(started.size(), 1);
    QCOMPARE(m_runner->running(), QStringList{QStringLiteral("switch-tetris")});
    QVERIFY(ended.wait(10000));
    QVERIFY(m_runner->running().isEmpty());

    const QString rom = m_data + QStringLiteral("/roms/switch/Homebrew/Tetris.nro");
    QCOMPARE(readFile(fakeDir() + "/args.txt"), QByteArray("--fullscreen\n" + QFile::encodeName(rom) + "\n"));
    QCOMPARE(readFile(fakeDir() + "/cwd.txt").trimmed(), QFile::encodeName(m_data + "/roms/switch/Homebrew"));
    QVERIFY2(!QFile::exists(fakeDir() + "/leak.txt"), "the launcher's descriptors reached the game");
    QVERIFY(readFile(m_dir->filePath("cache/launch/switch-tetris.log")).contains("something went wrong"));

    // Recorded: 1 s rounds to 0 minutes, but it counts as played.
    const QByteArray file = readFile(m_data + "/library/games/switch-tetris.toml");
    QVERIFY2(file.contains("play_minutes = 0\n"), file.constData());
    QVERIFY2(file.contains("last_played = "), file.constData());
    const QVariantMap tetris = m_library->gameAt(m_library->rowOfGame(QStringLiteral("switch-tetris")));
    QVERIFY(!tetris.value("lastPlayedText").toString().isEmpty());
}

void LaunchTest::shortSessionIsNotCounted()
{
    openLibrary(); // min_session_seconds = 30
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QVERIFY(ended.wait(10000));
    QVERIFY(messages.isEmpty());
    QVERIFY(!QFile::exists(m_data + "/library/games/switch-tetris.toml"));
}

void LaunchTest::quickFailureIsReported()
{
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    qputenv("FAKE_EXIT", "3");
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QVERIFY(ended.wait(10000));
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().first().toString(), QStringLiteral("Ryubing closed right away; see the log"));
    QVERIFY(!QFile::exists(m_data + "/library/games/switch-tetris.toml"));
}

void LaunchTest::sameGameDoesNotStartTwice()
{
    openLibrary();
    qputenv("FAKE_SECONDS", "1");
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(started.size(), 1);
    QVERIFY(ended.wait(10000));
}

void LaunchTest::launchRomKeyGame()
{
    // A game added from outside the ROM folders: the path from its file, its args after the
    // emulator's.
    const QString rom = m_dir->filePath(QStringLiteral("home/Downloads/Celeste.nsp"));
    writeFile(rom, {});
    writeFile(m_data + "/library/games/switch-celeste.toml",
              "title = \"Celeste\"\nplatform = \"switch\"\nrom = \"~/Downloads/Celeste.nsp\"\n"
              "args = [\"--mod\"]\n");
    writeFile(m_data + "/library/games/switch-gone.toml",
              "title = \"Gone\"\nplatform = \"switch\"\nrom = \"~/Downloads/Gone.nsp\"\n");
    openLibrary();
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(QStringLiteral("switch-celeste")), QString());
    QVERIFY(ended.wait(10000));
    QCOMPARE(readFile(fakeDir() + "/args.txt"), QByteArray("--fullscreen\n--mod\n" + QFile::encodeName(rom) + "\n"));
    QCOMPARE(readFile(fakeDir() + "/cwd.txt").trimmed(), QFile::encodeName(QFileInfo(rom).absolutePath()));

    const QVariantMap gone = m_library->gameAt(m_library->rowOfGame(QStringLiteral("switch-gone")));
    QVERIFY(!gone.value("installed").toBool());
    QCOMPARE(m_runner->launch(QStringLiteral("switch-gone")), QStringLiteral("Gone's ROM file isn't found"));
    QCOMPARE(m_runner->openFolder(QStringLiteral("switch-gone")), QStringLiteral("Gone's ROM file isn't found"));
}

void LaunchTest::cannotLaunch()
{
    openLibrary();
    QCOMPARE(m_runner->launch(QStringLiteral("switch-mario-tennis-aces")),
             QStringLiteral("Mario Tennis Aces's ROM isn't in the ROM folders"));
    // Azahar isn't installed in this test (nothing is detected), and no path is set for it.
    QCOMPARE(m_runner->launch(QStringLiteral("3ds-kid-icarus-uprising")),
             QStringLiteral("Install Azahar to play Kid Icarus: Uprising"));
    QVERIFY(m_runner->running().isEmpty());
    // Which emulator the "Install Azahar" toast links to.
    QCOMPARE(m_runner->missingEmulator(QStringLiteral("3ds-kid-icarus-uprising")), QStringLiteral("azahar"));
    QCOMPARE(m_runner->missingEmulator(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(m_runner->missingEmulator(QStringLiteral("steam-1145360")), QString());

    const QVariantMap kid = m_library->gameAt(m_library->rowOfGame(QStringLiteral("3ds-kid-icarus-uprising")));
    QCOMPARE(kid.value("launchTip").toString(), QStringLiteral("No emulator for this game is installed"));
    const QVariantMap tetris = m_library->gameAt(m_library->rowOfGame(QStringLiteral("switch-tetris")));
    QCOMPARE(tetris.value("launchTip").toString(), QStringLiteral("Launch with Ryubing (your path)"));
}

void LaunchTest::openEmulatorAndFolder()
{
    openLibrary();
    QCOMPARE(m_runner->openEmulator(QStringLiteral("switch-tetris")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(fakeDir() + "/cwd.txt"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(readFile(fakeDir() + "/args.txt"), QByteArray("--fullscreen\n"), 5000); // no ROM
    QVERIFY(m_runner->running().isEmpty()); // not a play session

    QCOMPARE(m_runner->openFolder(QStringLiteral("switch-mario-tennis-aces")),
             QStringLiteral("Mario Tennis Aces's ROM isn't in the ROM folders"));
    QCOMPARE(m_runner->openFolder(QStringLiteral("proton-diablo-iv")),
             QStringLiteral("Diablo IV's program isn't found; locate it to play it"));
}

void LaunchTest::protonGames()
{
    openLibrary();
    m_library->setInstalledOnly(false);
    const QString steamLog = fakeDir() + "/steam.txt";
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    // Launched through its shortcut's 64-bit game ID, without Steam's window.
    QCOMPARE(m_runner->launch(ffxiv), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(steamLog), QByteArray("--prefix -silent steam://rungameid/10557610576647815168\n"),
                              5000);
    QCOMPARE(m_runner->running(), QStringList{ffxiv});
    // Its page in Steam, by the same ID.
    QCOMPARE(m_runner->openEmulator(ffxiv), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).endsWith("--prefix steam://nav/games/details/10557610576647815168\n"), 5000);
    // Uninstalling a Proton game isn't Steam's.
    QCOMPARE(m_runner->uninstall(ffxiv), QString());
    // Nor done while it runs: nothing moved, nothing written.
    bool trashed = false;
    m_library->setTrash([&trashed](const QString &, QString *) { return trashed = true; });
    const QByteArray file = readFile(m_data + "/library/games/" + ffxiv + ".toml");
    QCOMPARE(m_runner->uninstallProton(ffxiv, true, true, true, QStringLiteral("remove"), false).value("error").toString(),
             QStringLiteral("Final Fantasy XIV Online is running; close it first"));
    QVERIFY(!trashed);
    QCOMPARE(readFile(m_data + "/library/games/" + ffxiv + ".toml"), file);
    // "Game not found": nothing to hand to Steam.
    QCOMPARE(m_runner->launch(QStringLiteral("proton-diablo-iv")),
             QStringLiteral("Diablo IV's program isn't found; locate it to play it"));
    QCOMPARE(m_runner->restartSteamAndLaunch(QStringLiteral("proton-diablo-iv")),
             QStringLiteral("Diablo IV's program isn't found; locate it to play it"));
    QCOMPARE(readFile(steamLog).count('\n'), 2);
    QCOMPARE(m_runner->running(), QStringList{ffxiv});

    const QVariantMap game = m_library->gameAt(m_library->rowOfGame(ffxiv));
    QCOMPARE(game.value("launchTip").toString(), QStringLiteral("Launch through Steam"));
    const QVariantMap diablo = m_library->gameAt(m_library->rowOfGame(QStringLiteral("proton-diablo-iv")));
    QCOMPARE(diablo.value("launchTip").toString(), QStringLiteral("Locate the game's program"));
}

void LaunchTest::protonGameRunning()
{
    // Steam runs a shortcut's reaper under its app ID, not the game ID in the URL.
    openLibrary();
    qputenv("FAKE_GAME_SECONDS", "2");
    qputenv("FAKE_REAPER_ID", "2458135266");
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    QCOMPARE(m_runner->launch(ffxiv), QString());
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 12000);
    QVERIFY(m_runner->running().isEmpty());
    // Steam records the play time, not the launcher.
    QVERIFY(!readFile(m_data + "/library/games/" + ffxiv + ".toml").contains("play_minutes"));
    qunsetenv("FAKE_GAME_SECONDS");
    qunsetenv("FAKE_REAPER_ID");
}

void LaunchTest::locateProgram()
{
    openLibrary();
    m_library->setInstalledOnly(false);
    const QString exe = m_dir->filePath(QStringLiteral("games/Diablo IV/Diablo IV Launcher.exe"));
    writeFile(exe, {});
    const QString diablo = m_data + "/library/games/proton-diablo-iv.toml";
    auto row = [this](const QString &id) { return m_library->gameAt(m_library->rowOfGame(id)); };

    // "Game not found": exe points at the new file, as a file: URL from the picker.
    QVERIFY(!row(QStringLiteral("proton-diablo-iv")).value("installed").toBool());
    QCOMPARE(m_library->locateProgram(QStringLiteral("proton-diablo-iv"), QUrl::fromLocalFile(exe).toString()), QString());
    QVERIFY(readFile(diablo).contains("exe = \"" + QFile::encodeName(exe) + "\"\n"));
    QVERIFY(!readFile(diablo).contains("start_dir"));
    QTRY_VERIFY(row(QStringLiteral("proton-diablo-iv")).value("installed").toBool());
    QCOMPARE(m_runner->launch(QStringLiteral("proton-diablo-iv")), QString());

    // Kept by Uninstall, with a start dir: installed comes back, start_dir follows the program.
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    const QString ffxivFile = m_data + "/library/games/" + ffxiv + ".toml";
    QCOMPARE(m_library->setGameValue(ffxiv, QStringLiteral("installed"), false), QString());
    QCOMPARE(m_library->setGameValue(ffxiv, QStringLiteral("start_dir"), m_dir->filePath(QStringLiteral("old"))), QString());
    QTRY_VERIFY(!row(ffxiv).value("installed").toBool());
    QCOMPARE(m_runner->launch(ffxiv), QStringLiteral("Final Fantasy XIV Online isn't installed; locate its program to play it"));
    QCOMPARE(m_library->locateProgram(ffxiv, exe), QString());
    QVERIFY(!readFile(ffxivFile).contains("installed"));
    QVERIFY(readFile(ffxivFile).contains("start_dir = \"" + QFile::encodeName(QFileInfo(exe).absolutePath()) + "\"\n"));
    QTRY_VERIFY(row(ffxiv).value("installed").toBool());

    // Refused: a file that isn't there, a game that isn't a Proton game.
    QCOMPARE(m_library->locateProgram(ffxiv, m_dir->filePath(QStringLiteral("nope.exe"))),
             m_dir->filePath(QStringLiteral("nope.exe")) + QStringLiteral(" isn't found"));
    QCOMPARE(m_library->locateProgram(QStringLiteral("switch-tetris"), exe),
             QStringLiteral("Only Proton and Linux games have a program to locate"));
}

void LaunchTest::steamGames()
{
    openLibrary();
    const QString steamLog = fakeDir() + "/steam.txt";
    // Installed: launched through Steam, without its window. Running while Steam starts it.
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1145360")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(steamLog), QByteArray("--prefix -silent steam://rungameid/1145360\n"), 5000);
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"});
    // Not installed (Elden Ring isn't in the demo's Steam): Launch installs it.
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1245620")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).endsWith("--prefix steam://install/1245620\n"), 5000);
    // Its page in Steam.
    QCOMPARE(m_runner->openEmulator(QStringLiteral("steam-1145360")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).endsWith("--prefix steam://nav/games/details/1145360\n"), 5000);
    // Uninstalled by Steam, which asks first; only an installed Steam game.
    QCOMPARE(m_runner->uninstall(QStringLiteral("steam-1145360")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).endsWith("--prefix steam://uninstall/1145360\n"), 5000);
    QCOMPARE(m_runner->uninstall(QStringLiteral("steam-1245620")), QStringLiteral("Elden Ring isn't installed"));
    QCOMPARE(m_runner->uninstall(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(m_runner->openFolder(QStringLiteral("steam-1245620")), QStringLiteral("Elden Ring isn't installed"));
    QCOMPARE(readFile(steamLog).count('\n'), 4);
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"}); // still waiting for it to start

    QCOMPARE(m_runner->missingEmulator(QStringLiteral("steam-1145360")), QString());
    const QVariantMap hades = m_library->gameAt(m_library->rowOfGame(QStringLiteral("steam-1145360")));
    QCOMPARE(hades.value("launchTip").toString(), QStringLiteral("Launch through Steam"));
    m_library->setInstalledOnly(false);
    const QVariantMap elden = m_library->gameAt(m_library->rowOfGame(QStringLiteral("steam-1245620")));
    QCOMPARE(elden.value("launchTip").toString(), QStringLiteral("Install through Steam; it asks where"));
}

void LaunchTest::steamUpdates()
{
    openLibrary();
    const QString steamLog = fakeDir() + "/steam.txt";
    const QString id = QStringLiteral("steam-1145360");
    const auto setManifest = [&](const QByteArray &fields) {
        writeFile(m_data + "/steam/steamapps/appmanifest_1145360.acf",
                  "\"AppState\"\n{\n\t\"appid\"\t\t\"1145360\"\n\t\"name\"\t\t\"Hades\"\n\t\"installdir\"\t\t\"Hades\"\n"
                  "\t\"SizeOnDisk\"\t\t\"15032385536\"\n\t\"buildid\"\t\t\"100\"\n" + fields + "}\n");
        m_library->store().refresh();
        m_library->store().waitForIdle();
        QCoreApplication::processEvents();
        return m_library->gameAt(m_library->rowOfGame(id));
    };

    // A manifest only says an update started; Steam's log says whether it runs.
    const auto writeStateLog = [&](bool running) {
        writeFile(m_data + "/steam/logs/content_log.txt",
                  QByteArray("[2026-10-09 08:52:31] AppID 1145360 state changed : Update Required,Fully Installed,")
                      + (running ? "Update Running," : "") + "Update Started,\n");
    };
    QVariantMap row = setManifest("\t\"StateFlags\"\t\t\"6\"\n\t\"BytesToDownload\"\t\t\"1288490188\"\n");
    QCOMPARE(row.value("steamUpdate").toString(), QStringLiteral("waiting"));
    QVERIFY(row.value("installed").toBool());
    QVERIFY(row.value("steamUpdateText").toString().startsWith(QStringLiteral("Waiting · ")));
    QCOMPARE(row.value("launchTip").toString(), QStringLiteral("Open the game's page in Steam to update it"));
    QCOMPARE(m_runner->launch(id), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(steamLog), QByteArray("--prefix steam://nav/games/details/1145360\n"), 5000);

    writeStateLog(true);
    row = setManifest("\t\"StateFlags\"\t\t\"1286\"\n\t\"BytesToDownload\"\t\t\"1000\"\n\t\"BytesDownloaded\"\t\t\"450\"\n");
    QCOMPARE(row.value("steamUpdate").toString(), QStringLiteral("downloading"));
    QVERIFY(row.value("installed").toBool());
    QCOMPARE(row.value("steamUpdateText").toString(), QStringLiteral("Downloading · 45%"));
    QCOMPARE(row.value("steamUpdatePercent").toInt(), 45);
    QCOMPARE(row.value("launchTip").toString(), QStringLiteral("Steam is updating the game; open its downloads"));
    QCOMPARE(m_runner->update(id), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).endsWith("--prefix steam://open/downloads\n"), 5000);

    writeStateLog(false);
    row = setManifest("\t\"StateFlags\"\t\t\"1538\"\n\t\"BytesToDownload\"\t\t\"1000\"\n\t\"BytesDownloaded\"\t\t\"450\"\n");
    // The log's watch fires after the manifest's rescan.
    QTRY_COMPARE_WITH_TIMEOUT(m_library->gameAt(m_library->rowOfGame(id)).value("steamUpdate").toString(),
                              QStringLiteral("paused"), 5000);
    row = m_library->gameAt(m_library->rowOfGame(id));
    QCOMPARE(row.value("steamUpdateText").toString(), QStringLiteral("Paused · 45%"));
    QCOMPARE(row.value("launchTip").toString(), QStringLiteral("The update is paused; open Steam's downloads"));
    QCOMPARE(m_runner->launch(id), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(steamLog).count('\n'), 3, 5000);

    // Up to date again: the leftover byte counts mean nothing, and Launch launches.
    row = setManifest("\t\"StateFlags\"\t\t\"4\"\n\t\"BytesToDownload\"\t\t\"1000\"\n\t\"BytesDownloaded\"\t\t\"1000\"\n");
    QCOMPARE(row.value("steamUpdate").toString(), QStringLiteral("none"));
    QVERIFY(row.value("steamUpdateText").toString().isEmpty());
    QCOMPARE(row.value("launchTip").toString(), QStringLiteral("Launch through Steam"));
    QCOMPARE(m_runner->update(id), QString());
    QCOMPARE(readFile(steamLog).count('\n'), 3);

    // A paused first install opens Steam's downloads, not steam://install.
    const QString firstInstall = QStringLiteral("steam-367520");
    writeFile(m_data + "/steam/steamapps/appmanifest_367520.acf",
              "\"AppState\"\n{\n\t\"appid\"\t\t\"367520\"\n\t\"name\"\t\t\"Hollow Knight\"\n\t\"installdir\"\t\t\"Hollow "
              "Knight\"\n\t\"SizeOnDisk\"\t\t\"1000\"\n\t\"StateFlags\"\t\t\"1538\"\n\t\"buildid\"\t\t\"0\"\n}\n");
    m_library->store().refresh();
    m_library->store().waitForIdle();
    QCoreApplication::processEvents();
    row = m_library->gameAt(m_library->rowOfGame(firstInstall));
    QVERIFY(!row.value("installed").toBool());
    QCOMPARE(row.value("steamUpdate").toString(), QStringLiteral("paused"));
    QCOMPARE(row.value("launchTip").toString(), QStringLiteral("The install is paused; open Steam's downloads"));
    QCOMPARE(m_runner->launch(firstInstall), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(steamLog).count('\n'), 4, 5000);
    QVERIFY(readFile(steamLog).endsWith("--prefix steam://open/downloads\n"));
}

void LaunchTest::downloadBar()
{
    openLibrary();
    const auto setManifest = [&](int appId, const QByteArray &name, const QByteArray &fields) {
        writeFile(m_data + QString("/steam/steamapps/appmanifest_%1.acf").arg(appId),
                  "\"AppState\"\n{\n\t\"appid\"\t\t\"" + QByteArray::number(appId) + "\"\n\t\"name\"\t\t\"" + name
                      + "\"\n\t\"installdir\"\t\t\"" + name + "\"\n\t\"SizeOnDisk\"\t\t\"1000\"\n" + fields + "}\n");
    };
    const auto refresh = [&] {
        m_library->store().refresh();
        m_library->store().waitForIdle();
        QCoreApplication::processEvents();
    };
    const auto writeStateLog = [&](bool hadesRuns) {
        writeFile(m_data + "/steam/logs/content_log.txt",
                  QByteArray("[2026-10-09 08:52:31] AppID 1145360 state changed : Update Required,Fully Installed,")
                      + (hadesRuns ? "Update Running," : "") + "Update Started,\n");
    };
    QSignalSpy changed(m_library.get(), &Library::downloadChanged);
    QVERIFY(m_library->download().isEmpty());

    // Waiting moves nothing.
    setManifest(1145360, "Hades", "\t\"StateFlags\"\t\t\"6\"\n\t\"buildid\"\t\t\"100\"\n\t\"BytesToDownload\"\t\t\"1000\"\n");
    refresh();
    QVERIFY(m_library->download().isEmpty());
    QCOMPARE(changed.count(), 0);

    // An update, running.
    writeStateLog(true);
    setManifest(1145360, "Hades",
                "\t\"StateFlags\"\t\t\"1286\"\n\t\"buildid\"\t\t\"100\"\n\t\"BytesToDownload\"\t\t\"1000\"\n"
                "\t\"BytesDownloaded\"\t\t\"450\"\n");
    refresh();
    QVariantMap download = m_library->download();
    QCOMPARE(download.value("gameId").toString(), QStringLiteral("steam-1145360"));
    QCOMPARE(download.value("name").toString(), QStringLiteral("Hades"));
    QCOMPARE(download.value("state").toString(), QStringLiteral("downloading"));
    QCOMPARE(download.value("percent").toInt(), 45);
    QVERIFY(!download.value("installing").toBool());
    QVERIFY(download.value("icon").toUrl().isEmpty());
    const int afterStart = changed.count();
    QVERIFY(afterStart > 0);

    // The same state read again changes nothing.
    refresh();
    QCOMPARE(changed.count(), afterStart);

    // A first install of another game that is paused: the running one stays.
    setManifest(367520, "Hollow Knight", "\t\"StateFlags\"\t\t\"1538\"\n\t\"buildid\"\t\t\"0\"\n");
    refresh();
    download = m_library->download();
    QCOMPARE(download.value("gameId").toString(), QStringLiteral("steam-1145360"));

    // Hades pauses too: the first of the two in library order is shown.
    writeStateLog(false);
    QTRY_COMPARE_WITH_TIMEOUT(m_library->download().value("state").toString(), QStringLiteral("paused"), 5000);
    QVERIFY(changed.count() > afterStart);

    // Updated: only the first install remains, with an icon once it has one.
    setManifest(1145360, "Hades", "\t\"StateFlags\"\t\t\"4\"\n\t\"buildid\"\t\t\"100\"\n");
    writeFile(m_data + "/art/steam-367520/icon.png", "x");
    refresh();
    download = m_library->download();
    QCOMPARE(download.value("gameId").toString(), QStringLiteral("steam-367520"));
    QVERIFY(download.value("installing").toBool());
    QVERIFY(!download.value("icon").toUrl().isEmpty());

    setManifest(367520, "Hollow Knight", "\t\"StateFlags\"\t\t\"4\"\n\t\"buildid\"\t\t\"1\"\n");
    refresh();
    QVERIFY(m_library->download().isEmpty());
}

void LaunchTest::steamGameRunning()
{
    openLibrary();
    qputenv("FAKE_GAME_SECONDS", "2");
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1145360")), QString());
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"});

    // Found once Steam has started it, then watched until it exits.
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    QCOMPARE(started.first().first().toString(), QStringLiteral("steam-1145360"));
    // Launch again while it runs: nothing new is handed to Steam.
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1145360")), QString());
    // Steam's setup step exits, and the game takes over a second later: still one session.
    QTest::qWait(3500);
    QCOMPARE(ended.count(), 0);
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"});
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 12000);
    QCOMPARE(started.count(), 1);
    QVERIFY(m_runner->running().isEmpty());
    QCOMPARE(readFile(fakeDir() + "/steam.txt").count('\n'), 1);
    // Steam records the play time, not the launcher.
    QVERIFY(!QFile::exists(m_data + "/library/games/steam-1145360.toml")
            || !readFile(m_data + "/library/games/steam-1145360.toml").contains("play_minutes"));
    qunsetenv("FAKE_GAME_SECONDS");
}

void LaunchTest::steamCommand()
{
    QCOMPARE(GameRunner::steamCommand(SteamInstall::Kind::Flatpak),
             (QStringList{"flatpak", "run", "com.valvesoftware.Steam"}));
    // Native: `steam` from the PATH, or nothing when it isn't there.
    const QByteArray path = qgetenv("PATH");
    writeFile(fakeDir() + "/bin/steam", kFakeSteam, true);
    qputenv("PATH", QFile::encodeName(fakeDir() + "/bin"));
    QCOMPARE(GameRunner::steamCommand(SteamInstall::Kind::Native), QStringList{fakeDir() + "/bin/steam"});
    qputenv("PATH", QFile::encodeName(m_dir->filePath(QStringLiteral("nowhere"))));
    QVERIFY(GameRunner::steamCommand(SteamInstall::Kind::Native).isEmpty());
    qputenv("PATH", path);

    // No Steam found: nothing to hand the game to.
    openLibrary("[steam]\nenabled = false\n");
    m_runner->setSteamProgram({});
    m_library->setInstalledOnly(false);
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1145360")), QStringLiteral("Steam isn't found"));
}

void LaunchTest::closeSteamAndApply()
{
    const QString root = copyFakeSteam();
    QProcess steam;
    startFakeSteam(root, steam);
    qputenv("FAKE_STEAM_PID", QByteArray::number(steam.processId())); // closes on -shutdown
    m_stateDir = m_dir->filePath(QStringLiteral("state"));
    openLibrary("[steam]\npath = \"" + QFile::encodeName(root) + "\"\n");
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    const QString pizza = QStringLiteral("steam-2231450");
    const auto queued = [&] { return m_library->store().snapshot().steamQueued.contains(pizza); };

    QCOMPARE(m_library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("mangohud %command%")),
             QString());
    QVERIFY(queued());
    QCOMPARE(m_runner->closeSteamAndApply(), QString());
    QVERIFY(m_runner->closingSteam());
    // Steam is asked to shut down; once it's gone, the edit is written and Steam starts again.
    QTRY_VERIFY_WITH_TIMEOUT(!m_runner->closingSteam(), 5000);
    QVERIFY(readFile(localConfig).contains("\"LaunchOptions\"\t\t\"mangohud %command%\""));
    QVERIFY(!queued());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(fakeDir() + "/steam.txt"), QByteArray("--prefix -shutdown\n--prefix -silent\n"),
                              5000);
    steam.waitForFinished();

    // Steam doesn't close in time: a message, and nothing written until it does.
    qunsetenv("FAKE_STEAM_PID");
    QProcess stubborn;
    startFakeSteam(root, stubborn);
    QCOMPARE(m_library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("gamemoderun %command%")),
             QString());
    m_runner->setSteamCloseTimeout(200);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->closeSteamAndApply(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(messages.size(), 1, 5000);
    QCOMPARE(messages.first().first().toString(), QStringLiteral("Steam didn't close within 30 seconds; nothing was applied"));
    QVERIFY(!m_runner->closingSteam());
    QVERIFY(queued());
    QVERIFY(readFile(localConfig).contains("\"LaunchOptions\"\t\t\"mangohud %command%\""));
    QVERIFY(readFile(fakeDir() + "/steam.txt").endsWith("--prefix -shutdown\n"));
    stubborn.kill();
    stubborn.waitForFinished();
    QTRY_VERIFY_WITH_TIMEOUT(!queued(), 5000);
    QVERIFY(readFile(localConfig).contains("\"LaunchOptions\"\t\t\"gamemoderun %command%\""));

    // With Steam closed, it's written right away (already done here: nothing left to write),
    // and Steam isn't started.
    QCOMPARE(m_runner->closeSteamAndApply(), QString());
    QVERIFY(!m_runner->closingSteam());
    QVERIFY(readFile(fakeDir() + "/steam.txt").endsWith("--prefix -shutdown\n"));
}

void LaunchTest::launchWithQueuedEdits()
{
    const QString root = copyFakeSteam();
    m_stateDir = m_dir->filePath(QStringLiteral("state"));
    openLibrary("[steam]\npath = \"" + QFile::encodeName(root) + "\"\n");
    const QString log = fakeDir() + "/steam.txt";
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    const QString tomb = QStringLiteral("steam-8000");
    const QString pizza = QStringLiteral("steam-2231450");
    const auto queued = [&](const QString &id) { return m_library->store().snapshot().steamQueued.contains(id); };

    // Steam closed: written first, then launched, without asking.
    QCOMPARE(m_library->setGameValue(tomb, QStringLiteral("launch_options"), QStringLiteral("gamemoderun %command%")),
             QString());
    QVERIFY(queued(tomb));
    QCOMPARE(m_runner->launch(tomb), QString());
    QVERIFY(readFile(localConfig).contains("\"LaunchOptions\"\t\t\"gamemoderun %command%\""));
    QVERIFY(!queued(tomb));
    QTRY_COMPARE_WITH_TIMEOUT(readFile(log), QByteArray("--prefix -silent steam://rungameid/8000\n"), 5000);

    // Steam runs from here. "Launch with the old options": handed to Steam as it is, the edit
    // still queued.
    QProcess steam;
    startFakeSteam(root, steam);
    qputenv("FAKE_STEAM_PID", QByteArray::number(steam.processId())); // closes on -shutdown
    QCOMPARE(m_library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("mangohud %command%")),
             QString());
    QCOMPARE(m_runner->launch(pizza), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(log).endsWith("\n--prefix -silent steam://rungameid/2231450\n"), 5000);
    QVERIFY(queued(pizza));
    QVERIFY(!readFile(localConfig).contains("mangohud"));

    // "Restart Steam and launch" (a new runner: in this one Pizza Tower is starting already).
    // Starting from the click; Steam shuts down, the edit is written, and Steam starts again
    // with the game.
    startRunner();
    QCOMPARE(m_runner->restartSteamAndLaunch(pizza), QString());
    QVERIFY(m_runner->closingSteam());
    QCOMPARE(m_runner->running(), QStringList{pizza});
    QTRY_VERIFY_WITH_TIMEOUT(!m_runner->closingSteam(), 5000);
    QVERIFY(readFile(localConfig).contains("\"LaunchOptions\"\t\t\"mangohud %command%\""));
    QVERIFY(!queued(pizza));
    QTRY_VERIFY_WITH_TIMEOUT(readFile(log).endsWith("\n--prefix -shutdown\n--prefix -silent steam://rungameid/2231450\n"),
                             5000);
    QCOMPARE(m_runner->running(), QStringList{pizza});
    steam.waitForFinished();

    // Steam doesn't close in time: nothing written, the game not launched, and said so.
    qunsetenv("FAKE_STEAM_PID");
    QProcess stubborn;
    startFakeSteam(root, stubborn);
    startRunner();
    m_runner->setSteamCloseTimeout(200);
    QCOMPARE(m_library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("gamemoderun %command%")),
             QString());
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->restartSteamAndLaunch(pizza), QString());
    QCOMPARE(m_runner->running(), QStringList{pizza});
    QTRY_COMPARE_WITH_TIMEOUT(messages.size(), 1, 5000);
    QCOMPARE(messages.first().first().toString(),
             QStringLiteral("Steam didn't close within 30 seconds; nothing was applied, and Pizza Tower wasn't launched"));
    QVERIFY(m_runner->running().isEmpty());
    QVERIFY(queued(pizza));
    QVERIFY(readFile(log).endsWith("\n--prefix -shutdown\n"));
    stubborn.kill();
    stubborn.waitForFinished();
}

void LaunchTest::gameOutlivesTheLauncher()
{
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    qputenv("FAKE_SECONDS", "3");
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(fakeDir() + "/pid.txt").isEmpty(), 5000);
    const pid_t pid = readFile(fakeDir() + "/pid.txt").trimmed().toInt();
    m_runner.reset(); // the launcher quits

    // Still running, not a zombie.
    const QByteArray stat = readFile(QStringLiteral("/proc/%1/stat").arg(pid));
    QVERIFY(!stat.isEmpty());
    QVERIFY2(!stat.contains(") Z "), stat.constData());
    QVERIFY(!QFile::exists(m_data + "/library/games/switch-tetris.toml")); // not recorded

    // Tidy up: this test process is still its parent.
    ::kill(pid, SIGTERM);
    ::waitpid(pid, nullptr, 0);
}

void LaunchTest::setLaunchValue_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("isDefault"); // true = expect key removed, not written
    QTest::addColumn<bool>("isError");

    QTest::newRow("while_running minimize") << "while_running" << "minimize" << false << false;
    QTest::newRow("while_running stay (default)") << "while_running" << "stay" << true << false;
    QTest::newRow("min_session_seconds 60") << "min_session_seconds" << "60" << false << false;
    QTest::newRow("min_session_seconds 30 (default)") << "min_session_seconds" << "30" << true << false;
    QTest::newRow("min_session_seconds 0") << "min_session_seconds" << "0" << false << false;
    QTest::newRow("min_session_seconds 3600") << "min_session_seconds" << "3600" << false << false;
    QTest::newRow("bad while_running") << "while_running" << "hidden" << false << true;
    QTest::newRow("bad min_session_seconds") << "min_session_seconds" << "-1" << false << true;
    QTest::newRow("out of range") << "min_session_seconds" << "3601" << false << true;
    QTest::newRow("unknown key") << "foo" << "bar" << false << true;
}

void LaunchTest::setLaunchValue()
{
    QFETCH(QString, key);
    QFETCH(QString, value);
    QFETCH(bool, isDefault);
    QFETCH(bool, isError);

    openLibrary("# comment\n");

    QSignalSpy spy(m_runner.get(), &GameRunner::settingsChanged);
    const QByteArray originalFile = readFile(m_data + "/settings.toml");

    const QString error = m_runner->setLaunchValue(key, value);
    QCOMPARE(!error.isEmpty(), isError);

    if (isError) {
        // File unchanged on error.
        QCOMPARE(readFile(m_data + "/settings.toml"), originalFile);
        return;
    }

    // Watcher fires; wait a moment for settingsChanged.
    QCoreApplication::processEvents();
    QTest::qWait(200);

    const QByteArray newFile = readFile(m_data + "/settings.toml");
    if (isDefault) {
        // Default: key is absent.
        QVERIFY(!newFile.contains(key.toUtf8()));
    } else {
        // Non-default: key is present.
        QVERIFY(newFile.contains(key.toUtf8()));
    }

    // Re-parse and check the live value matches what was written.
    const auto parsed = parseLaunchSettings(newFile, QStringLiteral("settings.toml"));
    QVERIFY(parsed.ok);
    if (key == QLatin1String("while_running")) {
        const bool wantMinimize = (value == QLatin1String("minimize"));
        QCOMPARE(parsed.value.whileRunning == LaunchSettings::WhileRunning::Minimize, wantMinimize);
        QCOMPARE(m_runner->minimizeWhileRunning(), wantMinimize);
    } else if (key == QLatin1String("min_session_seconds")) {
        QCOMPARE(parsed.value.minSessionSeconds, value.toInt());
    }
}

QTEST_GUILESS_MAIN(LaunchTest)
#include "launch_test.moc"
