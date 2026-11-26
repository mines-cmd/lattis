#include "detachedprocess.h"
#include "gamerunner.h"
#include "heroicgameconfig.h"
#include "heroicscan.h"
#include "launchcommand.h"
#include "launchoptions.h"
#include "launchsettings.h"
#include "library.h"
#include "restart.h"
#include "appimageenv.h"
#include "steamshortcuts.h"
#include "testfiles.h"

#include <QDirIterator>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <optional>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

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

// The fakes below run for as long as a test needs them by waiting while a file is there (a hold):
// the test makes it before the start and removes it to let the fake go on, instead of guessing a sleep.

// Stands in for an emulator: records what it was given, then runs for FAKE_SECONDS and while
// FAKE_HOLD names a file that exists, and exits with FAKE_EXIT.
const QByteArray kFakeEmulator = R"(#!/bin/sh
dir=$(dirname "$0")
printf '%s\n' "$@" > "$dir/args.txt"
pwd > "$dir/cwd.txt"
echo $$ > "$dir/pid.txt"
if [ -n "$FAKE_FD" ] && [ -e "/proc/$$/fd/$FAKE_FD" ]; then echo leaked > "$dir/leak.txt"; fi
echo "fake emulator: something went wrong" >&2
sleep "${FAKE_SECONDS:-0}"
while [ -n "$FAKE_HOLD" ] && [ -e "$FAKE_HOLD" ]; do sleep 0.02; done
exit "${FAKE_EXIT:-0}"
)";

// Stands in for Steam's command: appends each call's arguments, one call per line. With
// FAKE_GAME, a rungameid URL also starts a stand-in game with the command line Steam's reaper
// has (AppId is the URL's ID, or FAKE_REAPER_ID): a step of Steam's own while step.run is beside
// this script, then the game while game.run is. -shutdown kills FAKE_STEAM_PID.
const QByteArray kFakeSteam = R"(#!/bin/sh
dir=$(dirname "$0")
echo "$*" >> "$dir/steam.txt"
for arg in "$@"; do
    case "$arg" in
    steam:*rungameid*)
        if [ -n "$FAKE_GAME" ]; then
            # Like a Proton game: a step of Steam's own under the same AppId, then the game.
            app="AppId=${FAKE_REAPER_ID:-${arg##*[!0-9]}}"
            hold='while [ -e "$0" ]; do sleep 0.02; done'
            (sh -c "$hold" "$dir/step.run" SteamLaunch "$app" -- iscriptevaluator
             sh -c "$hold" "$dir/game.run" SteamLaunch "$app" -- game) >/dev/null 2>&1 &
        fi ;;
    -shutdown)
        # The running Steam stand-in closes, if the test says which one it is.
        if [ -n "$FAKE_STEAM_PID" ]; then kill "$FAKE_STEAM_PID"; fi ;;
    esac
done
)";

// Stands in for Heroic's command: appends each call's arguments, one call per line, runs while
// FAKE_HEROIC_HOLD names a file that exists, then writes FAKE_HEROIC_PLAYED over timestamp.json (as a
// Heroic that was closed does when the game ends) and exits with FAKE_HEROIC_EXIT.
const QByteArray kFakeHeroic = R"(#!/bin/sh
echo "$*" >> "$(dirname "$0")/heroic.txt"
while [ -n "$FAKE_HEROIC_HOLD" ] && [ -e "$FAKE_HEROIC_HOLD" ]; do sleep 0.02; done
if [ -n "$FAKE_HEROIC_PLAYED" ]; then printf '%s' "$FAKE_HEROIC_PLAYED" > "$FAKE_HEROIC_TIMESTAMP"; fi
exit "${FAKE_HEROIC_EXIT:-0}"
)";

QByteArray heroicTimestamp(int minutes, const QByteArray &lastPlayed)
{
    return R"({"HikeApp":{"firstPlayed":"2026-10-01T10:00:00.000Z","lastPlayed":")" + lastPlayed
           + R"(","totalPlayed":)" + QByteArray::number(minutes) + "}}";
}

// Stands in for a Linux game's launcher script: records what it was given and where, the
// variables the tests look at, then runs for FAKE_SECONDS and while FAKE_HOLD names a file that
// exists, and exits with FAKE_EXIT. With FAKE_CHILD_HOLD, it leaves a child behind that runs while
// that file exists, then writes child.txt.
const QByteArray kFakeGame = R"(#!/bin/sh
dir=$(dirname "$0")
printf '%s\n' "$@" > "$dir/args.txt"
pwd > "$dir/cwd.txt"
printf 'FOO=%s\nAPPDIR=%s\nLD_LIBRARY_PATH=%s\n' "$FOO" "${APPDIR-unset}" "${LD_LIBRARY_PATH-unset}" > "$dir/env.txt"
if [ -n "$FAKE_CHILD_HOLD" ]; then
    (while [ -e "$FAKE_CHILD_HOLD" ]; do sleep 0.02; done; echo done > "$dir/child.txt") &
fi
sleep "${FAKE_SECONDS:-0}"
while [ -n "$FAKE_HOLD" ] && [ -e "$FAKE_HOLD" ]; do sleep 0.02; done
exit "${FAKE_EXIT:-0}"
)";

// Stands in for Heroic's Legendary: appends each call's arguments and LEGENDARY_CONFIG_PATH, one call
// per line, runs while FAKE_LEGENDARY_HOLD names a file that exists, then fails with
// FAKE_LEGENDARY_ERROR as Legendary's last line, or succeeds. `info` answers with a Windows game's
// sizes; `repair` reports FAKE_LEGENDARY_REPAIRED broken files.
const QByteArray kFakeLegendary = R"(#!/bin/sh
echo "$* [$LEGENDARY_CONFIG_PATH]" >> "$(dirname "$0")/legendary.txt"
while [ -n "$FAKE_LEGENDARY_HOLD" ] && [ -e "$FAKE_LEGENDARY_HOLD" ]; do sleep 0.02; done
if [ -n "$FAKE_LEGENDARY_ERROR" ]; then
    echo "[cli] INFO: starting" >&2
    echo "[cli] ERROR: $FAKE_LEGENDARY_ERROR" >&2
    exit 1
fi
if [ "$1" = info ]; then
    echo '{"game": {"platform_versions": {"Windows": "1"}}, "manifest": {"download_size": 1000, "disk_size": 3000}}'
    exit 0
fi
if [ "$1" = repair ] && [ -n "$FAKE_LEGENDARY_REPAIRED" ]; then
    echo "[cli] ERROR: Verification failed, $FAKE_LEGENDARY_REPAIRED file(s) corrupted, 0 file(s) are missing." >&2
fi
echo "[cli] INFO: done" >&2
exit 0
)";

// Stands in for a running program the launcher looks for in its /proc folder: runs while `hold`
// exists, and on SIGTERM writes `hold`.term and exits (or ignores it, `stubborn`). Started, it waits
// until its trap is set (`hold`.ready), so a signal sent at once is still caught.
void startHeld(QProcess &process, const QString &hold, bool stubborn = false)
{
    const QString onTerm = stubborn ? QStringLiteral("''") : QStringLiteral("'echo term > \"$0.term\"; exit 0'");
    process.start(QStringLiteral("sh"),
                  {QStringLiteral("-c"),
                   QStringLiteral("trap %1 TERM; : > \"$0.ready\"; while [ -e \"$0\" ]; do sleep 0.02; done").arg(onTerm),
                   hold});
    QVERIFY(process.waitForStarted());
    QTRY_VERIFY(QFile::exists(hold + QStringLiteral(".ready")));
}

// Whether the process is there and not a zombie nobody has reaped yet.
bool alive(qint64 pid)
{
    const QByteArray stat = readFile(QStringLiteral("/proc/%1/stat").arg(pid));
    return !stat.isEmpty() && !stat.contains(") Z ");
}

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
    void openUrlKeepsNoAppImageEnvironment();
    void platformThemeGoesBack();
    void restartCommand();
    void linuxGameSession();
    void linuxGameRunsWhileItsSessionLives();
    void linuxGameQuickFailure();
    void linuxGameOptionsAndStartDir();
    void linuxGameRefused();
    void linuxGamePage();
    void heroicClosed();
    void heroicAlreadyRunning();
    void heroicUnreadableTimestamp();
    void heroicInstallAndOpen();
    void heroicForceClose();
    void heroicQuickFailure();
    void heroicNotFound();
    void heroicForceCloseKillsTheGame();
    void epicJobsRefused();
    void epicJobsOnAReadOnlyLibrary();
    void epicUpdateAndVerify();
    void epicInstall();
    void epicUninstall();
    void closeHeroicAndApplyRefused();
    void closeHeroicAndApply();
    void heroicDoesNotClose();
    void steamGames();
    void openSteamUrl();
    void steamUpdates();
    void downloadBar();
    void steamGameRunning();
    void steamCommand();
    void closeSteamAndApply();
    void steamForceClose();
    void killTreeEndsOnlyItsTree();
    void launchWithQueuedEdits();
    void openEmulatorAndFolder();
    void saveFolder();
    void protonGames();
    void protonGameRunning();
    void locateProgram();
    void nativeModManager();
    void modManagerRefused();
    void protonModManager();
    void flatpakModManager();
    void openFiles();
    void openSaveFolderAfterTheSearch();
    void gameOutlivesTheLauncher();
    void setLaunchValue_data();
    void setLaunchValue();

private:
    void openLibrary(const QByteArray &settings = {});
    // A new GameRunner on the open library, handing Steam's and Heroic's commands to the fake ones.
    void startRunner();
    QString heroicRoot() const { return m_dir->filePath(QStringLiteral("home/.config/heroic")); }
    QString heroicLog() const { return fakeDir() + QStringLiteral("/heroic.txt"); }
    // A Heroic with A Short Hike installed and played, and Fez App not installed.
    void openWithHeroic();
    // A copy of the tests' fake Steam (it has a user's localconfig.vdf; the demo's doesn't)
    // that can run: a copy of `sleep` named like its binary. Returns its folder.
    QString copyFakeSteam();
    // Starts that copy as the running Steam (in steam.pid).
    void startFakeSteam(const QString &root, QProcess &steam);
    QString fakeDir() const { return m_dir->filePath(QStringLiteral("fake")); }
    // A Heroic with A Short Hike installed in the scratch home (a leftover file in its folder, and an
    // update waiting unless not asked for, as Launch then updates it) and Fez not installed, and the fake
    // Legendary.
    void openWithEpic(bool updateWaiting = true);
    void writeEpicUpdate();
    QString hikeFolder() const { return m_dir->filePath(QStringLiteral("home/Games/Heroic/Hike")); }
    QString legendaryLog() const { return fakeDir() + QStringLiteral("/legendary.txt"); }
    // Makes a hold file (see the fakes) and returns its path.
    QString hold(const QString &name);
    // The runner's /proc: empty until a test lists a process of its own there.
    QString procDir() const { return m_dir->filePath(QStringLiteral("proc")); }
    void listProcess(qint64 pid, const QList<QByteArray> &cmdline);

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
    QTest::newRow("romdir") << QStringList{"{exe}", "{args}", "-rompath", "{romdir}", "{rom}"} << QStringList{}
                            << int(Kind::Executable) << "/roms/arcade games/sf2.zip"
                            << QStringList{"/usr/bin/emu", "-rompath", "/roms/arcade games", "/roms/arcade games/sf2.zip"};
    QTest::newRow("romdir inside an argument") << QStringList{"{exe}", "--path={romdir}", "{rom}"} << QStringList{}
                                               << int(Kind::Executable) << "/r/a b/x.zip"
                                               << QStringList{"/usr/bin/emu", "--path=/r/a b", "/r/a b/x.zip"};
    QTest::newRow("a folder named like a placeholder") << QStringList{"{exe}", "{romdir}", "{rom}"} << QStringList{}
                                                       << int(Kind::Executable) << "/r/{rom}/x.zip"
                                                       << QStringList{"/usr/bin/emu", "/r/{rom}", "/r/{rom}/x.zip"};
    QTest::newRow("no rom, romdir") << QStringList{"{exe}", "{args}", "-rompath", "{romdir}", "{rom}"} << QStringList{"-f"}
                                    << int(Kind::Executable) << "" << QStringList{"/usr/bin/emu", "-f"};
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

void LaunchTest::openUrlKeepsNoAppImageEnvironment()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dump = dir.filePath(QStringLiteral("env.txt"));
    writeFile(dir.filePath(QStringLiteral("xdg-open")),
              AppImageEnv::dumpScript(dump, "echo \"$1\" > '" + dir.filePath(QStringLiteral("url.txt")).toUtf8() + "'\n"),
              true);
    const AppImageEnv appImage;
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", dir.path().toUtf8() + ':' + path);
    const bool started = ::openUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/a folder")));
    QVERIFY(started);
    QTRY_VERIFY(QFile::exists(dump));
    qputenv("PATH", path);
    AppImageEnv::noBundleIn(dump);
    QTRY_VERIFY(QFile::exists(dir.filePath(QStringLiteral("url.txt"))));
    QFile url(dir.filePath(QStringLiteral("url.txt")));
    QVERIFY(url.open(QIODevice::ReadOnly));
    QCOMPARE(url.readAll().trimmed(), QByteArray("file:///tmp/a%20folder"));
}

void LaunchTest::platformThemeGoesBack()
{
    const QStringList env = {"HOME=/home/u", "QT_QPA_PLATFORMTHEME=xdgdesktopportal"};
    QCOMPARE(::programEnvironment(env), env);

    // main() set it: a child gets what the user had, or nothing.
    ::restorePlatformTheme(QStringLiteral("gtk3"));
    QCOMPARE(::programEnvironment(env), (QStringList{"HOME=/home/u", "QT_QPA_PLATFORMTHEME=gtk3"}));
    ::restorePlatformTheme(QString());
    QCOMPARE(::programEnvironment(env), (QStringList{"HOME=/home/u"}));
    // The command's own value still wins.
    QCOMPARE(::programEnvironment(env, {{"QT_QPA_PLATFORMTHEME", "qt6ct"}}),
             (QStringList{"HOME=/home/u", "QT_QPA_PLATFORMTHEME=qt6ct"}));
    ::restorePlatformTheme(std::nullopt);
    QCOMPARE(::programEnvironment(env), env);
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
    const QString child = hold(QStringLiteral("child"));
    qputenv("FAKE_CHILD_HOLD", QFile::encodeName(child));

    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(kLinuxGame), QString());
    // Not uninstalled while it runs, even once its script has exited: nothing moved.
    QTest::qWait(500);
    QCOMPARE(ended.count(), 0);
    QCOMPARE(m_runner->running(), QStringList{kLinuxGame});
    bool trashed = false;
    m_library->setTrash([&trashed](const QString &, QString *) { return trashed = true; });
    QCOMPARE(m_runner->uninstallProton(kLinuxGame, true, false, false, QStringLiteral("remove"), false)
                 .value("error").toString(),
             QStringLiteral("SuperTuxKart is running; close it first"));
    QVERIFY(!trashed);
    QVERIFY(QFile::exists(m_data + "/library/games/linux-supertuxkart.toml"));
    QTest::qWait(300);
    QCOMPARE(ended.count(), 0);
    // The game ends: so does the session.
    QFile::remove(child);
    QVERIFY(ended.wait(10000));
    QVERIFY(QFile::exists(m_data + "/linux-games/SuperTuxKart/child.txt"));
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
    for (const char *name : {"FAKE_FD", "FAKE_HOLD", "FAKE_GAME", "FAKE_REAPER_ID", "FAKE_STEAM_PID", "FAKE_CHILD_HOLD",
                             "FAKE_HEROIC_HOLD", "FAKE_LEGENDARY_HOLD", "FAKE_LEGENDARY_ERROR", "FAKE_LEGENDARY_REPAIRED"})
        qunsetenv(name);
    qunsetenv("FAKE_HEROIC_PLAYED");
    qunsetenv("FAKE_HEROIC_TIMESTAMP");
    qunsetenv("FAKE_HEROIC_EXIT");
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
    m_library->store().setHeroicLocations({nowhere.home, nowhere.home + QStringLiteral("/.config"), {}, {}});
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
    writeFile(fakeDir() + "/heroic.sh", kFakeHeroic, true);
    m_runner->setHeroicProgram({fakeDir() + "/heroic.sh", {QStringLiteral("--no-gui")}, true});
    // Nothing the user runs (their Heroic, their games, their Legendary) can be taken for the test's,
    // and so signalled or started.
    QVERIFY(QDir().mkpath(procDir()));
    m_runner->setProcDir(procDir());
    m_runner->setLegendaryProgram(QString());
    m_runner->start();
}

QString LaunchTest::hold(const QString &name)
{
    const QString path = m_dir->filePath(QStringLiteral("holds/") + name);
    writeFile(path, {});
    return path;
}

void LaunchTest::listProcess(qint64 pid, const QList<QByteArray> &cmdline)
{
    writeFile(procDir() + QStringLiteral("/%1/cmdline").arg(pid), cmdline.join('\0') + '\0');
}

void LaunchTest::writeEpicUpdate()
{
    writeFile(HeroicScan::assetsPath(heroicRoot()), R"({"Windows":[{"app_name":"HikeApp","build_version":"2"}]})");
}

void LaunchTest::openWithEpic(bool updateWaiting)
{
    writeFile(hikeFolder() + "/vkd3d-proton.cache", "cache");
    writeFile(HeroicScan::libraryPath(heroicRoot()),
              R"({"library":[{"app_name":"HikeApp","title":"A Short Hike","install":{}},
                             {"app_name":"Fez App","title":"Fez","install":{}}]})");
    writeFile(HeroicScan::installedPath(heroicRoot()),
              R"({"HikeApp":{"title":"A Short Hike","install_path":")" + QFile::encodeName(hikeFolder())
                  + R"(","install_size":1000,"version":"1","platform":"Windows"}})");
    if (updateWaiting)
        writeEpicUpdate();
    writeFile(HeroicScan::playTimePath(heroicRoot()), heroicTimestamp(5, "2026-11-01T10:00:00.000Z"));
    qputenv("FAKE_HEROIC_TIMESTAMP", HeroicScan::playTimePath(heroicRoot()).toLocal8Bit());
    writeFile(fakeDir() + "/legendary", kFakeLegendary, true);
    openLibrary();
    m_runner->setLegendaryProgram(fakeDir() + "/legendary");
}

void LaunchTest::openWithHeroic()
{
    writeFile(HeroicScan::libraryPath(heroicRoot()),
              R"({"library":[{"app_name":"HikeApp","title":"A Short Hike","install":{}},
                             {"app_name":"Fez App","title":"Fez","install":{}}]})");
    writeFile(HeroicScan::installedPath(heroicRoot()),
              R"({"HikeApp":{"title":"A Short Hike","install_path":"/games/hike","install_size":1000}})");
    writeFile(HeroicScan::playTimePath(heroicRoot()), heroicTimestamp(5, "2026-11-01T10:00:00.000Z"));
    qputenv("FAKE_HEROIC_TIMESTAMP", HeroicScan::playTimePath(heroicRoot()).toLocal8Bit());
    openLibrary();
}

void LaunchTest::heroicClosed()
{
    openWithHeroic();
    const QString id = QStringLiteral("epic-a-short-hike");
    const QString gameFile = m_data + QStringLiteral("/library/games/epic-a-short-hike.toml");
    QVERIFY(readFile(gameFile).contains("play_minutes = 5"));
    const QString heroic = hold(QStringLiteral("heroic"));
    qputenv("FAKE_HEROIC_HOLD", QFile::encodeName(heroic));
    qputenv("FAKE_HEROIC_PLAYED", heroicTimestamp(7, "2026-11-02T10:00:00.000Z"));
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);

    QCOMPARE(m_runner->launch(id), QString());
    QCOMPARE(m_runner->running(), QStringList{id});
    QCOMPARE(m_runner->launch(id), QString()); // already running: no second one
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(heroicLog()).isEmpty(), 5000);
    QCOMPARE(ended.count(), 0);
    QFile::remove(heroic);
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
    QCOMPARE(m_runner->running(), QStringList());
    QCOMPARE(readFile(heroicLog()), QByteArray("--no-gui heroic://launch?appName=HikeApp&runner=legendary&gui=false\n"));
    // Only Heroic's total counts, and the end of the session makes the scan that reads it.
    QTRY_VERIFY_WITH_TIMEOUT(readFile(gameFile).contains("play_minutes = 7"), 5000);
}

void LaunchTest::heroicAlreadyRunning()
{
    openWithHeroic();
    const QString id = QStringLiteral("epic-a-short-hike");
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(id), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(heroicLog()).isEmpty(), 5000);

    // Our process passed the URL on and is gone; the game runs on.
    QTest::qWait(1500);
    QCOMPARE(m_runner->running(), QStringList{id});

    // Another game's entry changing isn't this game closing.
    writeFile(HeroicScan::playTimePath(heroicRoot()),
              heroicTimestamp(5, "2026-11-01T10:00:00.000Z").chopped(1)
                  + R"(,"Fez App":{"lastPlayed":"2026-11-03T10:00:00.000Z","totalPlayed":1}})");
    QTest::qWait(1500);
    QCOMPARE(m_runner->running(), QStringList{id});

    // A session under a minute: only lastPlayed moves.
    writeFile(HeroicScan::playTimePath(heroicRoot()), heroicTimestamp(5, "2026-11-02T10:00:00.000Z"));
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
    QCOMPARE(m_runner->running(), QStringList());
}

void LaunchTest::heroicUnreadableTimestamp()
{
    openWithHeroic();
    const QString id = QStringLiteral("epic-a-short-hike");
    QCOMPARE(m_runner->launch(id), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(heroicLog()).isEmpty(), 5000);
    // Caught halfway through a save: not a change.
    writeFile(HeroicScan::playTimePath(heroicRoot()), "{\"HikeApp\":{");
    QTest::qWait(1500);
    QCOMPARE(m_runner->running(), QStringList{id});
}

void LaunchTest::heroicInstallAndOpen()
{
    openWithHeroic();
    // Not installed: Heroic opens, with no game URL, and there's no session to wait for.
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QSignalSpy copied(m_runner.get(), &GameRunner::copyRequested);
    QCOMPARE(m_runner->launch(QStringLiteral("epic-fez")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(heroicLog()), QByteArray("\n"), 5000);
    QCOMPARE(m_runner->running(), QStringList());
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().first().toString(), QStringLiteral("Fez copied; paste it in Heroic's search"));
    QCOMPARE(copied.size(), 1);
    QCOMPARE(copied.first().first().toString(), QStringLiteral("Fez"));

    QCOMPARE(m_runner->openHeroic(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(heroicLog()).count('\n'), 2, 5000);
    QCOMPARE(readFile(heroicLog()), QByteArray("\n\n"));
    QCOMPARE(m_runner->openEmulator(QStringLiteral("epic-fez")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(heroicLog()).count('\n'), 3, 5000);

    QCOMPARE(m_runner->missingEmulator(QStringLiteral("epic-fez")), QString());
    QCOMPARE(m_runner->openFolder(QStringLiteral("epic-fez")), QStringLiteral("Fez isn't installed"));
    m_library->setInstalledOnly(false);
    QCOMPARE(m_library->gameAt(m_library->rowOfGame(QStringLiteral("epic-fez"))).value("launchTip").toString(),
             QStringLiteral("Install through Heroic"));
    QCOMPARE(m_library->gameAt(m_library->rowOfGame(QStringLiteral("epic-a-short-hike"))).value("launchTip").toString(),
             QStringLiteral("Launch through Heroic"));
}

void LaunchTest::heroicForceClose()
{
    openWithHeroic();
    const QString id = QStringLiteral("epic-a-short-hike");

    // Our process is the Heroic that runs the game: it goes, with what's in its session.
    qputenv("FAKE_HEROIC_HOLD", QFile::encodeName(hold(QStringLiteral("heroic"))));
    QCOMPARE(m_runner->launch(id), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(heroicLog()).isEmpty(), 5000);
    QCOMPARE(m_runner->forceClose(id), QString());
    QTRY_COMPARE_WITH_TIMEOUT(m_runner->running(), QStringList(), 5000);

    // It already exited: a launch that stopped early would otherwise stay Running.
    qunsetenv("FAKE_HEROIC_HOLD");
    QCOMPARE(m_runner->launch(id), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(heroicLog()).count('\n'), 2, 5000);
    QTest::qWait(300);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->forceClose(id), QString());
    QCOMPARE(ended.count(), 1);
    QCOMPARE(m_runner->running(), QStringList());
}

void LaunchTest::heroicQuickFailure()
{
    openWithHeroic();
    qputenv("FAKE_HEROIC_EXIT", "1");
    QSignalSpy message(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->launch(QStringLiteral("epic-a-short-hike")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(message.count(), 1, 5000);
    QCOMPARE(message.first().first().toString(), QStringLiteral("Heroic closed right away; see the log"));
    QCOMPARE(m_runner->running(), QStringList());
}

void LaunchTest::heroicNotFound()
{
    openWithHeroic();
    const QByteArray settings = readFile(m_data + "/settings.toml");
    m_runner.reset();
    m_library.reset();
    openLibrary(settings + "\n[heroic]\nenabled = false\n");
    QCOMPARE(m_runner->launch(QStringLiteral("epic-a-short-hike")), QStringLiteral("Heroic isn't found"));
    QCOMPARE(m_runner->running(), QStringList());
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
    const QString emulator = hold(QStringLiteral("emulator"));
    qputenv("FAKE_HOLD", QFile::encodeName(emulator));
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(m_runner->launch(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(started.size(), 1);
    QFile::remove(emulator);
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

void LaunchTest::saveFolder()
{
    const QString home = m_dir->filePath(QStringLiteral("home"));
    const QString steamapps = m_data + QStringLiteral("/steam/steamapps");
    const QString user = QStringLiteral("/pfx/drive_c/users/steamuser");
    const QString ryubingSaves = home + QStringLiteral("/.config/Ryujinx/bis/user/save");
    const QString ffxivSaves = steamapps + QStringLiteral("/compatdata/2458135266") + user;
    const QString hadesSaves = steamapps + QStringLiteral("/compatdata/1145360") + user;
    for (const QString &folder : {ryubingSaves, ffxivSaves, hadesSaves, home + QStringLiteral("/Saves/Kart")})
        QVERIFY(QDir().mkpath(folder));
    writeFile(m_data + "/library/games/switch-mario-kart-8-deluxe.toml",
              readFile(m_data + "/library/games/switch-mario-kart-8-deluxe.toml") + "save_folder = \"~/Saves/Kart\"\n");
    writeFile(m_data + "/library/games/switch-pikmin-4.toml",
              readFile(m_data + "/library/games/switch-pikmin-4.toml") + "save_paths = [\"~/Saves/Gone\"]\n");
    openLibrary();
    m_library->setInstalledOnly(false);

    // The older save_folder key still counts, with ~ for the home folder.
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-mario-kart-8-deluxe")), home + QStringLiteral("/Saves/Kart"));
    // A location that isn't there is passed over; the emulator's save folder for its install is next.
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-pikmin-4")), ryubingSaves);
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-metroid-dread")), ryubingSaves);
    // A Proton game's prefix, and a Steam game's in compatdata; none for a Steam game without one.
    QCOMPARE(m_runner->saveFolder(QStringLiteral("proton-final-fantasy-xiv-online")), ffxivSaves);
    QCOMPARE(m_runner->saveFolder(QStringLiteral("steam-1145360")), hadesSaves);
    QCOMPARE(m_runner->saveFolder(QStringLiteral("steam-730")), QString());
    QCOMPARE(m_runner->saveFolder(QStringLiteral("proton-diablo-iv")), QString());
    // A native Linux game has none of its own.
    QCOMPARE(m_runner->saveFolder(QStringLiteral("linux-supertuxkart")), QString());
    QCOMPARE(m_runner->openSaveFolder(QStringLiteral("linux-supertuxkart")),
             QStringLiteral("SuperTuxKart's save folder isn't found; add its save location in its properties"));
    QCOMPARE(m_runner->shownPath(ryubingSaves), QStringLiteral("~/.config/Ryujinx/bis/user/save"));

    // An emulator that saves next to the ROM: the ROM's folder comes before its save folder.
    writeFile(m_data + "/library/emulators.toml",
              "[[emulator]]\nid = \"ryubing\"\npath = \"" + QFile::encodeName(fakeDir()) + "/emulator.sh\"\n"
              "saves_beside_rom = [\"sav\"]\n");
    m_runner.reset();
    m_library.reset();
    openLibrary();
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-metroid-dread")), m_data + QStringLiteral("/roms/switch"));
    // Not when the ROM isn't found.
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-mario-tennis-aces")), ryubingSaves);
    // A game's own folder still comes first.
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-mario-kart-8-deluxe")), home + QStringLiteral("/Saves/Kart"));

    // Properties writes locations in the home folder with ~ and drops the older key; a save file opens its folder.
    writeFile(home + "/Saves/Kart/slot.sav", "");
    QCOMPARE(m_library->saveGame(QStringLiteral("switch-mario-kart-8-deluxe"),
                                 {{"save_paths", QStringList{home + "/Saves/Gone", home + "/Saves/Kart/slot.sav"}},
                                  {"save_folder", QVariant()}}),
             QString());
    const QString written = readFile(m_data + "/library/games/switch-mario-kart-8-deluxe.toml");
    QVERIFY(written.contains("\"~/Saves/Kart/slot.sav\""));
    QVERIFY(!written.contains("save_folder"));
    QCOMPARE(m_runner->saveFolder(QStringLiteral("switch-mario-kart-8-deluxe")), home + QStringLiteral("/Saves/Kart"));
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
    // The fake Steam's game is a process of this test's, found among all of them.
    m_runner->setProcDir(QStringLiteral("/proc"));
    m_runner->setSteamGoneTimeout(1500);
    writeFile(fakeDir() + "/step.run", {});
    qputenv("FAKE_GAME", "1");
    qputenv("FAKE_REAPER_ID", "2458135266");
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    QCOMPARE(m_runner->launch(ffxiv), QString());
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    QFile::remove(fakeDir() + "/step.run");
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 12000);
    QVERIFY(m_runner->running().isEmpty());
    // Steam records the play time, not the launcher.
    QVERIFY(!readFile(m_data + "/library/games/" + ffxiv + ".toml").contains("play_minutes"));
}

void LaunchTest::nativeModManager()
{
    writeFile(m_data + "/mods/run.sh", kFakeGame, true);
    writeFile(m_data + "/linux-games/SuperTuxKart/run_game.sh", kFakeGame, true);
    writeFile(m_data + "/library/mod-managers/tool.toml",
              "name = \"Tool\"\nexe = \"" + QFile::encodeName(m_data) + "/mods/run.sh\"\n"
              "launch_options = \"--mods\"\ngames = [\"linux-supertuxkart\"]\n");
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    m_runner->setSessionPollInterval(100);
    const QString running = hold(QStringLiteral("running"));
    qputenv("FAKE_HOLD", QFile::encodeName(running));
    const QByteArray gameFile = readFile(m_data + "/library/games/linux-supertuxkart.toml");

    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launchModManager(QStringLiteral("tool")), QString());
    QCOMPARE(m_runner->running(), QStringList{QStringLiteral("mod-manager:tool")});
    QCOMPARE(m_runner->runningManagers(), QStringList{QStringLiteral("tool")});
    QCOMPARE(m_runner->runningTitle(QStringLiteral("mod-manager:tool")), QStringLiteral("Tool"));
    QCOMPARE(m_runner->launchModManager(QStringLiteral("tool")), QString()); // not twice
    QCOMPARE(m_runner->running().size(), 1);

    // Run directly, in its program's folder, with its launch options.
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_data + "/mods/cwd.txt"), 5000);
    QCOMPARE(readFile(m_data + "/mods/args.txt"), QByteArray("--mods\n"));
    QCOMPARE(readFile(m_data + "/mods/cwd.txt").trimmed(), QFile::encodeName(m_data + "/mods"));

    // A linked game isn't uninstalled while its manager runs; the game itself still launches.
    bool trashed = false;
    m_library->setTrash([&trashed](const QString &, QString *) { return trashed = true; });
    QCOMPARE(m_runner->uninstallProton(kLinuxGame, true, false, false, QStringLiteral("remove"), false)
                 .value("error").toString(),
             QStringLiteral("Tool is running; close it first"));
    QVERIFY(!trashed);
    QCOMPARE(m_runner->launch(kLinuxGame), QString());
    QCOMPARE(m_runner->running().size(), 2);

    QFile::remove(running);
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 2, 10000);
    QVERIFY(m_runner->running().isEmpty());

    // No play time, last played or history for the manager; the game's own run is recorded as usual.
    QVERIFY(!QFile::exists(m_data + "/library/history/mod-manager:tool.toml"));
    QVERIFY(!QDir(m_data + "/library/history").entryList({QStringLiteral("*tool*")}).size());
    QVERIFY(readFile(m_data + "/library/games/linux-supertuxkart.toml") != gameFile);
    QVERIFY(readFile(m_data + "/library/mod-managers/tool.toml").contains("name = \"Tool\""));
    QVERIFY(!readFile(m_data + "/library/mod-managers/tool.toml").contains("play"));
}

void LaunchTest::modManagerRefused()
{
    writeFile(m_data + "/mods/plain.sh", "#!/bin/sh\n", false);
    writeFile(m_data + "/library/mod-managers/gone.toml", "name = \"Gone\"\nexe = \"/nowhere/gone.sh\"\n");
    writeFile(m_data + "/library/mod-managers/plain.toml",
              "name = \"Plain\"\nexe = \"" + QFile::encodeName(m_data) + "/mods/plain.sh\"\n");
    writeFile(m_data + "/library/mod-managers/wrapped.toml",
              "name = \"Wrapped\"\nexe = \"" + QFile::encodeName(m_data) + "/mods/plain.sh\"\n"
              "launch_options = \"A=1\"\n");
    writeFile(m_data + "/library/mod-managers/folder.toml",
              "name = \"Folder\"\nexe = \"" + QFile::encodeName(m_data) + "/mods/plain.sh\"\nstart_dir = \"/nowhere\"\n");
    openLibrary();

    QCOMPARE(m_runner->launchModManager(QStringLiteral("nothing")), QString());
    QCOMPARE(m_runner->launchModManager(QStringLiteral("gone")),
             QStringLiteral("Gone's program isn't found: /nowhere/gone.sh"));
    QVERIFY(m_runner->launchModManager(QStringLiteral("plain")).contains("isn't executable"));
    QCOMPARE(m_runner->launchModManager(QStringLiteral("folder")),
             QStringLiteral("Folder's start folder isn't found: /nowhere"));
    QVERIFY(m_runner->running().isEmpty());
}

void LaunchTest::protonModManager()
{
    const QString exe = m_data + "/mods/MO2/ModOrganizer.exe";
    writeFile(exe, {});
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    writeFile(m_data + "/library/mod-managers/mo2.toml",
              "name = \"Mod Organizer 2\"\nexe = \"" + QFile::encodeName(exe) + "\"\n"
              "games = [\"" + ffxiv.toUtf8() + "\"]\nprefix = \"game\"\nshortcut = 3193880877\n");
    writeFile(m_data + "/library/mod-managers/nosteam.toml",
              "name = \"Not in Steam\"\nexe = \"" + QFile::encodeName(exe) + "\"\nshortcut = 3193880878\n"
              "in_steam = false\n");
    openLibrary();
    const QString steamLog = fakeDir() + "/steam.txt";
    const QString compatdata = m_data + "/steam/steamapps/compatdata/2458135266";

    // The game's prefix doesn't exist until the game has run once: never let Proton make one.
    QCOMPARE(m_runner->missingPrefixGame(QStringLiteral("mo2")), ffxiv);
    QCOMPARE(m_runner->launchModManager(QStringLiteral("mo2")),
             QStringLiteral("Run Final Fantasy XIV Online once first, so Proton creates its prefix"));
    QVERIFY(!QFile::exists(steamLog));
    QVERIFY(m_runner->running().isEmpty());

    QCOMPARE(m_runner->launchModManager(QStringLiteral("nosteam")),
             QStringLiteral("Not in Steam isn't in Steam; locate it in Settings → Mod managers"));

    // With the prefix there, it goes through Steam by the shortcut's game ID, as a Proton game does.
    QVERIFY(QDir().mkpath(compatdata));
    QCOMPARE(m_runner->missingPrefixGame(QStringLiteral("mo2")), QString());
    m_runner->launchModManager(QStringLiteral("mo2"));
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).contains("-silent steam://rungameid/"
                                                         + QByteArray::number(SteamShortcuts::gameId(3193880877))),
                             5000);
    QCOMPARE(m_runner->running(), QStringList{QStringLiteral("mod-manager:mo2")});
    QCOMPARE(m_runner->runningManagers(), QStringList{QStringLiteral("mo2")});

    // Its game can't be uninstalled while it runs.
    bool trashed = false;
    m_library->setTrash([&trashed](const QString &, QString *) { return trashed = true; });
    QCOMPARE(m_runner->uninstallProton(ffxiv, true, true, true, QStringLiteral("remove"), false)
                 .value("error").toString(),
             QStringLiteral("Mod Organizer 2 is running; close it first"));
    QVERIFY(!trashed);

    // Closing before Steam ran it stops waiting for it.
    QCOMPARE(m_runner->forceCloseModManager(QStringLiteral("mo2")), QString());
    QVERIFY(m_runner->running().isEmpty());
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
    // Steam's properties window, for an installed game or not.
    QCOMPARE(m_runner->openSteamProperties(QStringLiteral("steam-1245620")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamLog).endsWith("--prefix steam://gameproperties/1245620\n"), 5000);
    QCOMPARE(m_runner->openSteamProperties(QStringLiteral("switch-tetris")), QString());
    QCOMPARE(m_runner->openFolder(QStringLiteral("steam-1245620")), QStringLiteral("Elden Ring isn't installed"));
    QCOMPARE(readFile(steamLog).count('\n'), 5);
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"}); // still waiting for it to start

    QCOMPARE(m_runner->missingEmulator(QStringLiteral("steam-1145360")), QString());
    const QVariantMap hades = m_library->gameAt(m_library->rowOfGame(QStringLiteral("steam-1145360")));
    QCOMPARE(hades.value("launchTip").toString(), QStringLiteral("Launch through Steam"));
    m_library->setInstalledOnly(false);
    const QVariantMap elden = m_library->gameAt(m_library->rowOfGame(QStringLiteral("steam-1245620")));
    QCOMPARE(elden.value("launchTip").toString(), QStringLiteral("Install through Steam; it asks where"));
}

void LaunchTest::openSteamUrl()
{
    openLibrary();
    const QString steamLog = fakeDir() + "/steam.txt";
    QCOMPARE(m_runner->openSteamUrl(QStringLiteral("steam://friends/status/away")), QString());
    QCOMPARE(m_runner->openSteamUrl(QStringLiteral("steam://open/friends")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(steamLog),
                              QByteArray("--prefix steam://friends/status/away\n--prefix steam://open/friends\n"), 5000);
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
    // The fake Steam's game is a process of this test's, found among all of them.
    m_runner->setProcDir(QStringLiteral("/proc"));
    m_runner->setSteamGoneTimeout(1500);
    writeFile(fakeDir() + "/step.run", {});
    writeFile(fakeDir() + "/game.run", {});
    qputenv("FAKE_GAME", "1");
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1145360")), QString());
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"});

    // Found once Steam has started it, then watched until it exits.
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    QCOMPARE(started.first().first().toString(), QStringLiteral("steam-1145360"));
    // Launch again while it runs: nothing new is handed to Steam.
    QCOMPARE(m_runner->launch(QStringLiteral("steam-1145360")), QString());
    // Steam's setup step exits, and the game takes over: still one session, past the next look for it.
    QFile::remove(fakeDir() + "/step.run");
    QTest::qWait(1200);
    QCOMPARE(ended.count(), 0);
    QCOMPARE(m_runner->running(), QStringList{"steam-1145360"});
    QFile::remove(fakeDir() + "/game.run");
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 12000);
    QCOMPARE(started.count(), 1);
    QVERIFY(m_runner->running().isEmpty());
    QCOMPARE(readFile(fakeDir() + "/steam.txt").count('\n'), 1);
    // Steam records the play time, not the launcher.
    QVERIFY(!QFile::exists(m_data + "/library/games/steam-1145360.toml")
            || !readFile(m_data + "/library/games/steam-1145360.toml").contains("play_minutes"));
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
    qputenv("FAKE_HOLD", QFile::encodeName(hold(QStringLiteral("emulator"))));
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

namespace {

const QString kHike = QStringLiteral("epic-a-short-hike");
const QString kFez = QStringLiteral("epic-fez");

// Swaps environment variables for one test and puts them back however it ends.
class EnvSwap
{
public:
    void set(const char *name, const QByteArray &value)
    {
        keep(name);
        qputenv(name, value);
    }
    void unset(const char *name)
    {
        keep(name);
        qunsetenv(name);
    }
    ~EnvSwap()
    {
        for (const auto &[name, value] : std::as_const(m_saved)) {
            if (value)
                qputenv(name, *value);
            else
                qunsetenv(name);
        }
    }

private:
    void keep(const char *name)
    {
        for (const auto &saved : std::as_const(m_saved))
            if (qstrcmp(saved.first, name) == 0)
                return;
        m_saved.append({name, qEnvironmentVariableIsSet(name) ? std::optional(qgetenv(name)) : std::nullopt});
    }
    QList<std::pair<const char *, std::optional<QByteArray>>> m_saved;
};

} // namespace

void LaunchTest::heroicForceCloseKillsTheGame()
{
    openWithEpic(false);
    qputenv("FAKE_HEROIC_HOLD", QFile::encodeName(hold(QStringLiteral("heroic"))));
    QCOMPARE(m_runner->launch(kHike), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(heroicLog()).isEmpty(), 5000);

    // The game Heroic started, with a child of its own, is found by its -epicapp argument.
    QProcess game;
    const QString child = m_dir->filePath(QStringLiteral("game-child.pid"));
    game.start(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("sleep 30 & echo $! > \"$0\"; wait"), child});
    QVERIFY(game.waitForStarted());
    QTRY_VERIFY(!readFile(child).trimmed().isEmpty());
    const qint64 childPid = readFile(child).trimmed().toLongLong();
    const qint64 gamePid = game.processId();
    listProcess(gamePid, {"C:\\Hike\\Hike.exe", "-epicapp=HikeApp", "-epicenv=Prod"});

    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);
    QCOMPARE(m_runner->forceClose(kHike), QString());
    QVERIFY(game.waitForFinished(5000));
    QCOMPARE(game.exitStatus(), QProcess::CrashExit);
    QTRY_VERIFY(!alive(childPid));
    // Heroic ends the session once it writes the game's finish time; until then it runs.
    QCOMPARE(ended.count(), 0);
    QCOMPARE(m_runner->running(), QStringList{kHike});

    // Without the game, our Heroic process goes instead.
    QVERIFY(QDir(procDir() + QStringLiteral("/%1").arg(gamePid)).removeRecursively());
    QCOMPARE(m_runner->forceClose(kHike), QString());
    QTRY_COMPARE_WITH_TIMEOUT(m_runner->running(), QStringList(), 5000);
}

void LaunchTest::epicJobsRefused()
{
    openWithEpic();
    const QString folder = m_dir->filePath(QStringLiteral("home/Games/Epic"));
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QSignalSpy uninstalled(m_runner.get(), &GameRunner::epicUninstalled);

    // No Legendary: each says where to do it instead.
    m_runner->setLegendaryProgram(QString());
    QVERIFY(!m_runner->legendaryFound());
    QCOMPARE(m_runner->update(kHike), QString());
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(),
             QStringLiteral("Heroic's Legendary isn't found; update A Short Hike in Heroic"));
    QCOMPARE(m_runner->verifyEpic(kHike), QStringLiteral("Heroic's Legendary isn't found; verify A Short Hike in Heroic"));
    QCOMPARE(m_runner->installEpic(kFez, folder), QStringLiteral("Heroic's Legendary isn't found; install Fez in Heroic"));
    QCOMPARE(m_runner->uninstallEpic(kHike, true, false, false),
             QStringLiteral("Heroic's Legendary isn't found; uninstall the game in Heroic"));
    QCOMPARE(m_runner->epicInstallPlan(kFez).value("sizeState").toString(), QStringLiteral("unknown"));

    // Not an Epic game, or not in the state the job needs.
    m_runner->setLegendaryProgram(fakeDir() + "/legendary");
    QVERIFY(m_runner->legendaryFound());
    QCOMPARE(m_runner->verifyEpic(QStringLiteral("switch-tetris")), QStringLiteral("Heroic isn't found"));
    QCOMPARE(m_runner->installEpic(QStringLiteral("switch-tetris"), folder), QStringLiteral("Heroic isn't found"));
    QCOMPARE(m_runner->uninstallEpic(QStringLiteral("switch-tetris"), true, false, false),
             QStringLiteral("Heroic isn't found"));
    QCOMPARE(m_runner->verifyEpic(kFez), QStringLiteral("Fez isn't installed"));
    QCOMPARE(m_runner->installEpic(kHike, folder), QStringLiteral("A Short Hike is installed already"));
    QCOMPARE(m_runner->installEpic(kFez, QStringLiteral("  ")), QStringLiteral("Choose a folder to install into"));
    QCOMPARE(m_runner->update(kFez), QString()); // no update waiting: nothing to do, nothing said

    // Running (launched before Heroic found the update, as Launch updates a game that has one): nothing is
    // started, and Heroic isn't closed under it.
    const auto rescan = [this] {
        m_library->store().refresh();
        m_library->store().waitForIdle();
        QCoreApplication::processEvents();
    };
    QVERIFY(QFile::remove(HeroicScan::assetsPath(heroicRoot())));
    rescan();
    qputenv("FAKE_HEROIC_HOLD", QFile::encodeName(hold(QStringLiteral("heroic"))));
    QCOMPARE(m_runner->launch(kHike), QString());
    QCOMPARE(m_runner->running(), QStringList{kHike});
    writeEpicUpdate();
    rescan();
    QCOMPARE(m_library->gameAt(m_library->rowOfGame(kHike)).value("steamUpdate").toString(), QStringLiteral("waiting"));
    QCOMPARE(m_runner->update(kHike), QString());
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("A Short Hike is running; close it first"));
    QCOMPARE(m_runner->verifyEpic(kHike), QStringLiteral("A Short Hike is running; close it first"));
    QCOMPARE(m_runner->uninstallEpic(kHike, true, true, false), QStringLiteral("A Short Hike is running; close it first"));
    QCOMPARE(m_runner->uninstallEpic(kHike, false, true, false), QStringLiteral("A Short Hike is running; close it first"));
    QCOMPARE(m_runner->forceClose(kHike), QString());
    QTRY_COMPARE_WITH_TIMEOUT(m_runner->running(), QStringList(), 5000);

    QVERIFY(!QFile::exists(legendaryLog()));
    QVERIFY(m_runner->legendary()->jobs().isEmpty());
    QVERIFY(uninstalled.isEmpty());
    QVERIFY(messages.isEmpty());
    QCOMPARE(readFile(hikeFolder() + "/vkd3d-proton.cache"), QByteArray("cache"));
}

void LaunchTest::epicJobsOnAReadOnlyLibrary()
{
    writeFile(m_data + "/library/library.toml", "version = 99\n");
    openWithEpic();
    QVERIFY(m_library->readOnly());
    QSignalSpy uninstalled(m_runner.get(), &GameRunner::epicUninstalled);
    bool trashed = false;
    m_library->setTrash([&trashed](const QString &, QString *) { return trashed = true; });

    QCOMPARE(m_runner->verifyEpic(kHike), QStringLiteral("The library is read-only; nothing was verified"));
    QCOMPARE(m_runner->installEpic(kFez, m_dir->filePath(QStringLiteral("home/Games/Epic"))),
             QStringLiteral("The library is read-only; nothing was installed"));
    QCOMPARE(m_runner->uninstallEpic(kHike, true, true, false), QStringLiteral("The library is read-only; nothing was deleted"));
    QCOMPARE(m_runner->uninstallEpic(kHike, false, true, false), QStringLiteral("The library is read-only; nothing was deleted"));
    QVERIFY(!QFile::exists(legendaryLog()));
    QVERIFY(uninstalled.isEmpty());
    QVERIFY(!trashed);
    QVERIFY(QFile::exists(hikeFolder() + "/vkd3d-proton.cache"));
    QVERIFY(m_library->epicInstallFolder().isEmpty());
}

void LaunchTest::epicUpdateAndVerify()
{
    openWithEpic();
    const QByteArray config = " [" + QFile::encodeName(heroicRoot()) + "/legendaryConfig/legendary]";
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QSignalSpy finished(m_runner->legendary(), &LegendaryJobs::finished);
    QCOMPARE(m_library->gameAt(m_library->rowOfGame(kHike)).value("steamUpdate").toString(), QStringLiteral("waiting"));

    // Failures say which game and why, and the job stays as failed until it's tried again.
    qputenv("FAKE_LEGENDARY_ERROR", "The disk is full");
    QCOMPARE(m_runner->update(kHike), QString());
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("Couldn't update A Short Hike: The disk is full"));
    QCOMPARE(m_runner->epicJob(kHike).value("state").toString(), QStringLiteral("failed"));
    QCOMPARE(m_runner->verifyEpic(kHike), QString());
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("Couldn't verify A Short Hike: The disk is full"));
    QCOMPARE(readFile(legendaryLog()), "update HikeApp -y" + config + "\nrepair HikeApp -y" + config + "\n");

    // A cancel is the user's own: nothing said.
    qunsetenv("FAKE_LEGENDARY_ERROR");
    const QString running = hold(QStringLiteral("legendary"));
    qputenv("FAKE_LEGENDARY_HOLD", QFile::encodeName(running));
    QCOMPARE(m_runner->update(kHike), QString());
    QCOMPARE(m_runner->epicJobTitles(), QStringList{QStringLiteral("A Short Hike")});
    QTRY_VERIFY(readFile(legendaryLog()).count('\n') == 3);
    m_runner->legendary()->cancel(QStringLiteral("HikeApp"));
    QTRY_COMPARE(finished.count(), 3);
    QVERIFY(messages.isEmpty());
    QVERIFY(m_runner->epicJob(kHike).isEmpty());
    qunsetenv("FAKE_LEGENDARY_HOLD");

    // Done: an update says nothing (the list shows it); a check says what it found.
    QCOMPARE(m_runner->update(kHike), QString());
    QTRY_COMPARE(finished.count(), 4);
    QVERIFY(messages.isEmpty());
    QVERIFY(m_runner->epicJob(kHike).isEmpty());
    QCOMPARE(m_runner->verifyEpic(kHike), QString());
    QTRY_COMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("A Short Hike: all files are fine"));
    qputenv("FAKE_LEGENDARY_REPAIRED", "1");
    QCOMPARE(m_runner->verifyEpic(kHike), QString());
    QTRY_COMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("A Short Hike: fixed 1 file"));
    qputenv("FAKE_LEGENDARY_REPAIRED", "3");
    QCOMPARE(m_runner->verifyEpic(kHike), QString());
    QTRY_COMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("A Short Hike: fixed 3 files"));
}

void LaunchTest::epicInstall()
{
    openWithEpic();
    const QString folder = m_dir->filePath(QStringLiteral("home/Games/Epic"));
    QSignalSpy info(m_runner.get(), &GameRunner::epicInstallInfo);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QSignalSpy finished(m_runner->legendary(), &LegendaryJobs::finished);

    // The size is asked of Legendary; when it can't say, the dialog is told it's unknown and it asks again next time.
    qputenv("FAKE_LEGENDARY_ERROR", "Could not find the game");
    QVariantMap plan = m_runner->epicInstallPlan(kFez);
    QCOMPARE(plan.value("appName").toString(), QStringLiteral("Fez App"));
    QCOMPARE(plan.value("sizeState").toString(), QStringLiteral("checking"));
    QTRY_COMPARE(info.count(), 1);
    QCOMPARE(info.first().at(0).toString(), QStringLiteral("Fez App"));
    QCOMPARE(info.first().at(1).toMap().value("sizeState").toString(), QStringLiteral("unknown"));
    QCOMPARE(info.first().at(1).toMap().value("downloadSize").toDouble(), -1.0);
    qunsetenv("FAKE_LEGENDARY_ERROR");
    QCOMPARE(m_runner->epicInstallPlan(kFez).value("sizeState").toString(), QStringLiteral("checking"));
    QTRY_COMPARE(info.count(), 2);
    plan = info.last().at(1).toMap();
    QCOMPARE(plan.value("sizeState").toString(), QStringLiteral("known"));
    QCOMPARE(plan.value("downloadSize").toDouble(), 1000.0);
    QCOMPARE(plan.value("diskSize").toDouble(), 3000.0);
    QCOMPARE(plan.value("platform").toString(), QStringLiteral("Windows"));
    // Known for the rest of the run: not asked again.
    QCOMPARE(m_runner->epicInstallPlan(kFez).value("sizeState").toString(), QStringLiteral("known"));
    QCOMPARE(readFile(legendaryLog()).count("info Fez App --json"), 2);

    // A failed install says why.
    qputenv("FAKE_LEGENDARY_ERROR", "The disk is full");
    QCOMPARE(m_runner->installEpic(kFez, folder), QString());
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("Couldn't install Fez: The disk is full"));
    QVERIFY(readFile(legendaryLog())
                .endsWith("install Fez App --base-path " + QFile::encodeName(folder) + " --platform Windows --skip-sdl -y ["
                          + QFile::encodeName(heroicRoot()) + "/legendaryConfig/legendary]\n"));
    // The folder is remembered for the next install.
    QCOMPARE(m_library->epicInstallFolder(), folder);

    qunsetenv("FAKE_LEGENDARY_ERROR");
    QCOMPARE(m_runner->installEpic(kFez, folder), QString());
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), QStringLiteral("Fez installed"));
}

void LaunchTest::epicUninstall()
{
    openWithEpic();
    QStringList trashed;
    m_library->setTrash([&trashed](const QString &path, QString *) {
        trashed.append(path);
        return true;
    });
    QSignalSpy uninstalled(m_runner.get(), &GameRunner::epicUninstalled);
    const QString leftover = hikeFolder() + "/vkd3d-proton.cache";

    // Legendary fails: the error, and the folder stays.
    qputenv("FAKE_LEGENDARY_ERROR", "Permission denied");
    const QString running = hold(QStringLiteral("legendary"));
    qputenv("FAKE_LEGENDARY_HOLD", QFile::encodeName(running));
    QCOMPARE(m_runner->uninstallEpic(kHike, true, false, false), QString());
    // One at a time, and quitting is asked about.
    QCOMPARE(m_runner->uninstallEpic(kHike, true, false, false), QStringLiteral("A Short Hike is being uninstalled"));
    QCOMPARE(m_runner->epicUninstallTitles(), QStringList{QStringLiteral("A Short Hike")});
    QVERIFY(m_runner->epicJobTitles().isEmpty()); // not a job the list shows
    QFile::remove(running);
    QTRY_COMPARE(uninstalled.count(), 1);
    QCOMPARE(uninstalled.first().at(0).toString(), kHike);
    QCOMPARE(uninstalled.first().at(1).toMap(),
             (QVariantMap{{"error", QStringLiteral("Legendary could not uninstall it: Permission denied")}}));
    QVERIFY(trashed.isEmpty());
    QVERIFY(QFile::exists(leftover));
    QVERIFY(m_runner->epicUninstallTitles().isEmpty());

    // Legendary that doesn't start: why, alone.
    qunsetenv("FAKE_LEGENDARY_ERROR");
    m_runner->setLegendaryProgram(m_dir->filePath(QStringLiteral("nowhere/legendary")));
    QCOMPARE(m_runner->uninstallEpic(kHike, true, false, false), QString());
    QTRY_COMPARE(uninstalled.count(), 2);
    const QString error = uninstalled.last().at(1).toMap().value("error").toString();
    QVERIFY2(error.startsWith(QStringLiteral("Could not start")), qPrintable(error));
    QVERIFY(trashed.isEmpty());

    // Done: what Legendary leaves in the folder (a shader cache it didn't install) goes too.
    m_runner->setLegendaryProgram(fakeDir() + "/legendary");
    QCOMPARE(m_runner->uninstallEpic(kHike, true, false, false), QString());
    QTRY_COMPARE(uninstalled.count(), 3);
    QCOMPARE(uninstalled.last().at(1).toMap(), QVariantMap());
    QCOMPARE(trashed, QStringList{hikeFolder()});
    QVERIFY(readFile(legendaryLog()).endsWith("uninstall HikeApp -y [" + QFile::encodeName(heroicRoot())
                                              + "/legendaryConfig/legendary]\n"));

    // Files kept: Legendary isn't asked, and a prefix it doesn't have is said so.
    const qsizetype calls = readFile(legendaryLog()).count('\n');
    QCOMPARE(m_runner->uninstallEpic(kHike, false, true, false), QString());
    QCOMPARE(uninstalled.count(), 4);
    QEXPECT_FAIL("", "New, not a finding of the review: removeEpicPrefix takes the title from the game files, "
                     "which an Epic game has none of, so the message starts with a blank", Continue);
    QCOMPARE(uninstalled.last().at(1).toMap(),
             (QVariantMap{{"error", QStringLiteral("A Short Hike has no Wine prefix of its own")}}));
    QCOMPARE(readFile(legendaryLog()).count('\n'), calls);
    QCOMPARE(trashed.size(), 1);
}

void LaunchTest::closeHeroicAndApplyRefused()
{
    openWithEpic(false);
    QSignalSpy closing(m_runner.get(), &GameRunner::closingHeroicChanged);
    QProcess heroic;
    const QString heroicHold = hold(QStringLiteral("heroic-main"));
    startHeld(heroic, heroicHold);
    listProcess(heroic.processId(), {"/opt/Heroic/heroic", "--no-sandbox"});

    // A game it runs for us.
    qputenv("FAKE_HEROIC_HOLD", QFile::encodeName(hold(QStringLiteral("heroic"))));
    QCOMPARE(m_runner->launch(kHike), QString());
    QCOMPARE(m_runner->closeHeroicAndApply(), QStringLiteral("A Short Hike is running; close it first"));
    QCOMPARE(m_runner->forceClose(kHike), QString());
    QTRY_COMPARE_WITH_TIMEOUT(m_runner->running(), QStringList(), 5000);

    // A game started from Heroic itself, which the launcher only sees in the process list.
    listProcess(4'000'000'000, {"C:\\Hike\\Hike.exe", "-epicapp=HikeApp"});
    QCOMPARE(m_runner->closeHeroicAndApply(), QStringLiteral("A Short Hike is running; close it first"));

    QVERIFY(!m_runner->closingHeroic());
    QCOMPARE(closing.count(), 0);
    QTest::qWait(100);
    QCOMPARE(heroic.state(), QProcess::Running);
    QVERIFY(!QFile::exists(heroicHold + ".term"));
    QFile::remove(heroicHold);
    QVERIFY(heroic.waitForFinished(5000));
}

void LaunchTest::closeHeroicAndApply()
{
    openWithEpic();
    const QString config = HeroicGameConfig::path(heroicRoot(), QStringLiteral("HikeApp"));
    QProcess heroic;
    const QString heroicHold = hold(QStringLiteral("heroic-main"));
    m_library->setHeroicRunningCheck([&heroic] { return heroic.state() != QProcess::NotRunning; });
    HeroicGameEdits edits;
    edits.launcherArgs = QStringLiteral("-windowed");

    // Heroic isn't running: an edit is written at once, and there's nothing to close.
    QCOMPARE(m_library->queueHeroicEdits(QStringLiteral("HikeApp"), edits), QString());
    QVERIFY(readFile(config).contains("\"launcherArgs\": \"-windowed\""));
    QCOMPARE(m_runner->closeHeroicAndApply(), QString());
    QVERIFY(!m_runner->closingHeroic());

    // Heroic runs: its main process gets SIGTERM, its helpers don't, and the edit waits for it to go.
    startHeld(heroic, heroicHold);
    QProcess helper;
    const QString helperHold = hold(QStringLiteral("heroic-helper"));
    startHeld(helper, helperHold);
    listProcess(heroic.processId(), {"/opt/Heroic/heroic", "--no-sandbox"});
    listProcess(helper.processId(), {"/opt/Heroic/heroic", "--type=renderer"});
    edits.launcherArgs = QStringLiteral("-fullscreen");
    QCOMPARE(m_library->queueHeroicEdits(QStringLiteral("HikeApp"), edits), QString());
    QVERIFY(!readFile(config).contains("-fullscreen"));

    QSignalSpy closing(m_runner.get(), &GameRunner::closingHeroicChanged);
    QCOMPARE(m_runner->closeHeroicAndApply(), QString());
    QVERIFY(m_runner->closingHeroic());
    QCOMPARE(closing.count(), 1);
    QCOMPARE(m_runner->closeHeroicAndApply(), QString()); // already closing: not sent again
    QVERIFY(heroic.waitForFinished(5000));
    QCOMPARE(readFile(heroicHold + ".term"), QByteArray("term\n"));
    QTRY_VERIFY_WITH_TIMEOUT(!m_runner->closingHeroic(), 5000);
    QCOMPARE(closing.count(), 2);
    QVERIFY(readFile(config).contains("\"launcherArgs\": \"-fullscreen\""));
    QCOMPARE(helper.state(), QProcess::Running);
    QVERIFY(!QFile::exists(helperHold + ".term"));
    QFile::remove(helperHold);
    QVERIFY(helper.waitForFinished(5000));
}

void LaunchTest::heroicDoesNotClose()
{
    openWithEpic();
    const QString config = HeroicGameConfig::path(heroicRoot(), QStringLiteral("HikeApp"));
    QProcess heroic;
    const QString heroicHold = hold(QStringLiteral("heroic-main"));
    startHeld(heroic, heroicHold, true);
    listProcess(heroic.processId(), {"/opt/Heroic/heroic"});
    m_library->setHeroicRunningCheck([&heroic] { return heroic.state() != QProcess::NotRunning; });
    HeroicGameEdits edits;
    edits.launcherArgs = QStringLiteral("-windowed");
    QCOMPARE(m_library->queueHeroicEdits(QStringLiteral("HikeApp"), edits), QString());

    m_runner->setHeroicCloseTimeout(300);
    QSignalSpy messages(m_runner.get(), &GameRunner::message);
    QCOMPARE(m_runner->closeHeroicAndApply(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(messages.count(), 1, 5000);
    QCOMPARE(messages.first().first().toString(), QStringLiteral("Heroic didn't close within 15 seconds; nothing was applied"));
    QVERIFY(!m_runner->closingHeroic());
    QCOMPARE(heroic.state(), QProcess::Running);
    QVERIFY(!readFile(config).contains("-windowed"));
    QFile::remove(heroicHold);
    QVERIFY(heroic.waitForFinished(5000));
}

void LaunchTest::steamForceClose()
{
    openLibrary();
    m_runner->setSteamGoneTimeout(60'000);
    const QString hades = QStringLiteral("steam-1145360");
    QSignalSpy started(m_runner.get(), &GameRunner::sessionStarted);
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);

    // Not started yet: closing only stops waiting for it.
    QCOMPARE(m_runner->launch(hades), QString());
    QCOMPARE(m_runner->running(), QStringList{hades});
    QCOMPARE(m_runner->forceClose(hades), QString());
    QVERIFY(m_runner->running().isEmpty());
    QCOMPARE(ended.count(), 0);

    // Running: the game and what it started are killed.
    QProcess game;
    const QString child = m_dir->filePath(QStringLiteral("game-child.pid"));
    game.start(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("sleep 30 & echo $! > \"$0\"; wait"), child});
    QVERIFY(game.waitForStarted());
    QTRY_VERIFY(!readFile(child).trimmed().isEmpty());
    const qint64 childPid = readFile(child).trimmed().toLongLong();
    const qint64 gamePid = game.processId();
    listProcess(gamePid, {"reaper", "SteamLaunch", "AppId=1145360", "--", "Hades.exe"});
    QCOMPARE(m_runner->launch(hades), QString());
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    QCOMPARE(m_runner->forceClose(hades), QString());
    QVERIFY(game.waitForFinished(5000));
    QCOMPARE(game.exitStatus(), QProcess::CrashExit);
    QTRY_VERIFY(!alive(childPid));

    // Gone and not back: closing again ends the session without waiting longer.
    QVERIFY(QDir(procDir() + QStringLiteral("/%1").arg(gamePid)).removeRecursively());
    QCOMPARE(ended.count(), 0);
    QCOMPARE(m_runner->forceClose(hades), QString());
    QCOMPARE(ended.count(), 1);
    QVERIFY(m_runner->running().isEmpty());
}

void LaunchTest::killTreeEndsOnlyItsTree()
{
    // Each tree: a root, its child, and the child's child, each writing its pid.
    const QString script = m_dir->filePath(QStringLiteral("tree.sh"));
    writeFile(script,
              "#!/bin/sh\n"
              "sh -c 'echo $$ > \"$0.child\"; sleep 30 & echo $! > \"$0.grandchild\"; wait' \"$1\" &\n"
              "wait\n",
              true);
    const auto start = [&](QProcess &root, const QString &name) {
        root.start(script, {m_dir->filePath(name)});
        QVERIFY(root.waitForStarted());
        QTRY_VERIFY(!readFile(m_dir->filePath(name + ".grandchild")).trimmed().isEmpty());
    };
    const auto pids = [&](QProcess &root, const QString &name) {
        return QList<qint64>{root.processId(), readFile(m_dir->filePath(name + ".child")).trimmed().toLongLong(),
                             readFile(m_dir->filePath(name + ".grandchild")).trimmed().toLongLong()};
    };
    QProcess tree;
    QProcess bystander;
    start(tree, QStringLiteral("tree"));
    start(bystander, QStringLiteral("bystander"));
    const QList<qint64> doomed = pids(tree, QStringLiteral("tree"));
    const QList<qint64> spared = pids(bystander, QStringLiteral("bystander"));
    for (const qint64 pid : doomed + spared)
        QVERIFY(alive(pid));

    DetachedProcess::killTree(tree.processId());
    QVERIFY(tree.waitForFinished(5000));
    QCOMPARE(tree.exitStatus(), QProcess::CrashExit);
    for (const qint64 pid : doomed)
        QTRY_VERIFY(!alive(pid));
    for (const qint64 pid : spared)
        QVERIFY(alive(pid));

    DetachedProcess::killTree(bystander.processId());
    QVERIFY(bystander.waitForFinished(5000));
    for (const qint64 pid : spared)
        QTRY_VERIFY(!alive(pid));
}

void LaunchTest::flatpakModManager()
{
    // `flatpak` is the fake on a PATH with nothing else on it.
    const QString bin = m_dir->filePath(QStringLiteral("bin"));
    const QByteArray out = QFile::encodeName(fakeDir());
    writeFile(bin + "/flatpak",
              "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + out + "/flatpak.txt'\npwd > '" + out + "/flatpak-cwd.txt'\n", true);
    writeFile(fakeDir() + "/wrap.sh", "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + out + "/wrapper.txt'\nexec \"$@\"\n", true);
    writeFile(m_data + "/library/mod-managers/flat.toml",
              "name = \"Flat\"\nexe = \"com.example.ModTool\"\nlaunch_options = \"--portable\"\n");
    writeFile(m_data + "/library/mod-managers/wrapped.toml",
              "name = \"Wrapped\"\nexe = \"com.example.ModTool\"\nlaunch_options = '\"" + out
                  + "/wrap.sh\" %command% --verbose'\n");
    QVERIFY(QDir().mkpath(m_dir->filePath(QStringLiteral("home"))));
    EnvSwap env;
    env.set("PATH", QFile::encodeName(bin));
    openLibrary("[launch]\nmin_session_seconds = 0\n");
    QSignalSpy ended(m_runner.get(), &GameRunner::sessionEnded);

    // Run as `flatpak run <app id>` with its options, from the home folder.
    QCOMPARE(m_runner->launchModManager(QStringLiteral("flat")), QString());
    QCOMPARE(m_runner->runningManagers(), QStringList{QStringLiteral("flat")});
    QVERIFY(ended.wait(10000));
    QCOMPARE(readFile(fakeDir() + "/flatpak.txt"), QByteArray("run\ncom.example.ModTool\n--portable\n"));
    QCOMPARE(readFile(fakeDir() + "/flatpak-cwd.txt").trimmed(), QFile::encodeName(m_dir->filePath(QStringLiteral("home"))));

    // A wrapper goes first, and runs flatpak with the rest.
    QCOMPARE(m_runner->launchModManager(QStringLiteral("wrapped")), QString());
    QVERIFY(ended.wait(10000));
    QCOMPARE(readFile(fakeDir() + "/wrapper.txt"), QByteArray("flatpak\nrun\ncom.example.ModTool\n--verbose\n"));
    QCOMPARE(readFile(fakeDir() + "/flatpak.txt"), QByteArray("run\ncom.example.ModTool\n--verbose\n"));
}

void LaunchTest::openFiles()
{
    // xdg-open, xdg-mime and a terminal are fakes on a PATH with nothing else on it, and the desktop
    // entries are the test's own.
    const QString bin = m_dir->filePath(QStringLiteral("bin"));
    const QString share = m_dir->filePath(QStringLiteral("share"));
    const QByteArray out = QFile::encodeName(fakeDir());
    const QString opened = fakeDir() + "/opened.txt";
    writeFile(bin + "/xdg-open", "#!/bin/sh\necho \"$1\" >> '" + out + "/opened.txt'\n", true);
    writeFile(bin + "/xdg-mime",
              "#!/bin/sh\necho \"$3\" >> '" + out + "/mime.txt'\n"
              "if [ \"$1 $2\" = 'query default' ] && [ -n \"$FAKE_MIME_DEFAULT\" ]; then echo \"$FAKE_MIME_DEFAULT\"; fi\n",
              true);
    writeFile(bin + "/xdg-terminal-exec", "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + out + "/terminal.txt'\n", true);
    writeFile(share + "/applications/lattis-test-vim.desktop",
              "[Desktop Entry]\nType=Application\nName=Vim\nExec=vim --remote-tab %F\nTerminal=true\n"
              "[Desktop Action new]\nExec=vim --new\n");
    writeFile(share + "/applications/lattis-test-gedit.desktop",
              "[Desktop Entry]\nType=Application\nName=Gedit\nExec=gedit %U\nTerminal=false\n");
    const QString home = m_dir->filePath(QStringLiteral("home"));
    QVERIFY(QDir().mkpath(home + "/Saves"));
    EnvSwap env;
    env.set("PATH", QFile::encodeName(bin));
    env.set("HOME", QFile::encodeName(home));
    env.set("XDG_DATA_HOME", QFile::encodeName(share));
    env.set("XDG_DATA_DIRS", QFile::encodeName(m_dir->filePath(QStringLiteral("no-share"))));
    env.unset("TERMINAL");
    env.unset("FAKE_MIME_DEFAULT");
    openLibrary();
    const QString settings = m_data + "/library/games/linux-supertuxkart.toml";
    const auto lastOpened = [&] { return readFile(opened).trimmed().split('\n').last(); };

    // A problem's file, when the message names one that is there.
    QCOMPARE(m_runner->problemFile(QStringLiteral("library/games/linux-supertuxkart.toml:3: expected a value")), settings);
    QCOMPARE(m_runner->problemFile(QStringLiteral("library/games/gone.toml:3: expected a value")), QString());
    QCOMPARE(m_runner->problemFile(QStringLiteral("Steam isn't found")), QString());

    // A text file with an editor that runs in a terminal: in a terminal, not through xdg-open.
    env.set("FAKE_MIME_DEFAULT", "lattis-test-vim.desktop");
    QCOMPARE(m_runner->openFile(settings), QString());
    QTRY_COMPARE(readFile(fakeDir() + "/terminal.txt"), "vim\n--remote-tab\n" + QFile::encodeName(settings) + "\n");
    QVERIFY(!QFile::exists(opened));
    // One that doesn't, or none at all: xdg-open.
    env.set("FAKE_MIME_DEFAULT", "lattis-test-gedit.desktop");
    QCOMPARE(m_runner->openFile(settings), QString());
    QTRY_COMPARE(lastOpened(), QUrl::fromLocalFile(settings).toEncoded());
    env.unset("FAKE_MIME_DEFAULT");
    QCOMPARE(m_runner->openFile(m_data + "/settings.toml"), QString());
    QTRY_COMPARE(lastOpened(), QUrl::fromLocalFile(m_data + "/settings.toml").toEncoded());
    // With no default for the file's own type, text/plain's was asked for too.
    QVERIFY2(readFile(fakeDir() + "/mime.txt").contains("text/plain\n"), readFile(fakeDir() + "/mime.txt").constData());

    // Folders and links.
    QCOMPARE(m_runner->openDataFolder(), QString());
    QTRY_COMPARE(lastOpened(), QUrl::fromLocalFile(m_data).toEncoded());
    QCOMPARE(m_runner->openPath(QStringLiteral("~/Saves")), QString());
    QTRY_COMPARE(lastOpened(), QUrl::fromLocalFile(home + "/Saves").toEncoded());
    QCOMPARE(m_runner->openPath(QStringLiteral("~/Later")), QStringLiteral("~/Later doesn't exist yet"));
    QVERIFY(m_runner->openUrl(QUrl(QStringLiteral("https://example.org/a b"))));
    QTRY_COMPARE(lastOpened(), QByteArray("https://example.org/a%20b"));
    const qsizetype count = readFile(opened).count('\n');

    // No xdg-open: each says what it couldn't open.
    QVERIFY(QFile::remove(bin + "/xdg-open"));
    QCOMPARE(m_runner->openDataFolder(), "Could not open " + m_data);
    QCOMPARE(m_runner->openPath(QStringLiteral("~/Saves")), QStringLiteral("Could not open ~/Saves"));
    QVERIFY(!m_runner->openUrl(QUrl(QStringLiteral("https://example.org"))));
    QCOMPARE(m_runner->openFile(settings), settings);
    QCOMPARE(readFile(opened).count('\n'), count);
}

void LaunchTest::openSaveFolderAfterTheSearch()
{
    const QString bin = m_dir->filePath(QStringLiteral("bin"));
    const QString opened = fakeDir() + "/opened.txt";
    writeFile(bin + "/xdg-open", "#!/bin/sh\necho \"$1\" >> '" + QFile::encodeName(opened) + "'\n", true);
    const QString saves = m_dir->filePath(QStringLiteral("home/.local/share/supertuxkart"));
    writeFile(saves + "/config.xml", "<config/>");
    EnvSwap env;
    env.set("PATH", QFile::encodeName(bin));
    openLibrary();
    QStringList found;
    QStringList asked;
    m_runner->setFindSaves([&](const QString &gameId, std::function<void(const QStringList &)> done) {
        asked.append(gameId);
        QTimer::singleShot(0, m_runner.get(), [&found, done] { done(found); });
    });
    QSignalSpy messages(m_runner.get(), &GameRunner::message);

    // Found: the folder of the first save that is there opens, a moment later.
    found = {m_dir->filePath(QStringLiteral("home/gone/save.dat")), saves + "/config.xml"};
    QCOMPARE(m_runner->openSaveFolder(kLinuxGame), QString());
    QCOMPARE(asked, QStringList{kLinuxGame});
    QTRY_COMPARE(readFile(opened).trimmed(), QUrl::fromLocalFile(saves).toEncoded());
    QVERIFY(messages.isEmpty());

    // Nothing found, and a Linux game has no folder to guess: said so.
    found.clear();
    QCOMPARE(m_runner->openSaveFolder(kLinuxGame), QString());
    QTRY_COMPARE(messages.count(), 1);
    QCOMPARE(messages.takeFirst().first().toString(),
             QStringLiteral("SuperTuxKart's save folder isn't found; add its save location in its properties"));

    // Found, but nothing opens it.
    QVERIFY(QFile::remove(bin + "/xdg-open"));
    found = {saves + "/config.xml"};
    QCOMPARE(m_runner->openSaveFolder(kLinuxGame), QString());
    QTRY_COMPARE(messages.count(), 1);
    QCOMPARE(messages.takeFirst().first().toString(), "Could not open " + saves);
    QCOMPARE(readFile(opened).count('\n'), 1);
}

QTEST_GUILESS_MAIN(LaunchTest)
#include "launch_test.moc"
