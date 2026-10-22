#include "dateformat.h"
#include "backup/backups.h"
#include "install/installer.h"
#include "core/apppaths.h"
#include "core/gamepagemigration.h"
#include "core/general.h"
#include "core/oldname.h"
#include "core/restart.h"
#include "core/singleinstance.h"
#include "core/windowstate.h"
#include "input/actions.h"
#include "launch/gamerunner.h"
#include "launch/launchcommand.h"
#include "library/artimage.h"
#include "library/library.h"
#include "library/uninstall.h"
#include "metadata/bulklookup.h"
#include "metadata/scanlookup.h"
#include "metadata/thumbnailcache.h"
#include "theme/theme.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QLocale>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

namespace {
void logMemory()
{
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly))
        return;
    qint64 rssKb = 0, peakKb = 0;
    for (const QByteArray &line : status.readAll().split('\n')) {
        if (line.startsWith("VmRSS:"))
            rssKb = line.mid(6).trimmed().split(' ').value(0).toLongLong();
        else if (line.startsWith("VmHWM:"))
            peakKb = line.mid(6).trimmed().split(' ').value(0).toLongLong();
    }
    qInfo().noquote() << QStringLiteral("memory: %1 MB, peak %2 MB")
                             .arg(rssKb / 1024.0, 0, 'f', 1)
                             .arg(peakKb / 1024.0, 0, 'f', 1);
}

void logHook(const char *name, const QString &argument = {})
{
    qInfo().noquote() << "hook:" << name << argument;
}
}

int main(int argc, char *argv[])
{
    QElapsedTimer startup;
    startup.start();

    // As given, for a restart: QGuiApplication takes Qt's own options out of argv.
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lattis"));
    app.setApplicationDisplayName(QStringLiteral("Lattis"));
    app.setApplicationVersion(QStringLiteral(LATTIS_VERSION));
    app.setDesktopFileName(QStringLiteral("lattis"));
    // The desktop finds the icon through lattis.desktop; this is for X11 and for Wayland
    // compositors that take a window's own icon.
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QStringLiteral(":/qt/qml/Launcher/resources/logo/icons/lattis-%1.png").arg(size), QSize(size, size));
    app.setWindowIcon(icon);
    // Inter is the schema's default font.family; a failure leaves the text in the system font.
    if (QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Launcher/resources/fonts/InterVariable.ttf")) < 0)
        qWarning() << "can't load the bundled Inter font; the text falls back to the system font";
    // Art the library copies is shrunk, but art put in art/ by hand can be huge. Images are
    // shown through sourceSize, so only the scaled copy stays in memory.
    ArtImage::allowLargeImages();

    if (qEnvironmentVariableIsEmpty("LATTIS_DATA")) {
        const auto moved = OldName::move(OldName::folders(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)));
        for (const QString &line : moved)
            qInfo().noquote() << "old name:" << line;
    }

    const QString dataDir = AppPaths::dataDir();
    QDir().mkpath(dataDir);
    qInfo().noquote() << "data dir:" << dataDir;

    SingleInstance instance(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation),
                            AppPaths::dataDirKey(dataDir));
    const bool locked = Restart::waits(arguments) ? instance.waitForLock(Restart::kWaitMs) : instance.tryLock();
    if (!locked) {
        if (instance.activateRunning()) {
            qInfo() << "already running on this data dir; showing its window";
            return 0;
        }
        qCritical() << "another copy of Lattis holds this data dir but doesn't answer";
        return 1;
    }

    // All are exposed to QML as singletons (see their create()); they must outlive the engine.
    // Theme first: it creates settings.toml, which Actions and General read too.
    Theme theme(dataDir);
    {
        QString migrationError;
        const QString marker = AppPaths::stateDir() + QStringLiteral("/game-page-migrated-") + AppPaths::dataDirKey(dataDir);
        GamePageMigration::run(dataDir + QStringLiteral("/settings.toml"), marker, theme.legacyGamePage(), &migrationError);
        if (!migrationError.isEmpty())
            qWarning().noquote() << "can't move the game page settings out of the style:" << migrationError;
    }
    General general(dataDir);
    if (!general.systemLocale())
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    DateFormat::setSystemLocale(general.systemLocale());
    Actions actions(dataDir);
    Library library(dataDir, AppPaths::cacheDir(), NewLibrary::WithSetup);
    library.store().setStateDir(AppPaths::stateDir());
    library.setUndoDir(AppPaths::stateDir() + QStringLiteral("/undo"));
    GameRunner runner(&library, dataDir, AppPaths::cacheDir());
    WindowState windowState(AppPaths::stateDir(), AppPaths::dataDirKey(dataDir));

    BulkLookup::setLibrary([&library] { return library.lookupGames(); },
                           [&library](const auto &games) { return library.saveGames(games); },
                           [&library](const auto &art) { return library.setArts(art); },
                           [&library] { return library.tagMappings(); });
    ScanLookup scanLookup([&library] { return library.lookupGames(); },
                          [&library](const auto &games) { return library.saveGames(games); },
                          [&library](const auto &art) { return library.setArts(art); },
                          dataDir + QStringLiteral("/settings.toml"), nullptr, nullptr,
                          [&library] { return library.tagMappings(); });
    QObject::connect(&library.store(), &LibraryStore::gamesAdded, &scanLookup, &ScanLookup::add);
    Backups::setup({dataDir, AppPaths::stateDir(), AppPaths::dataDirKey(dataDir),
                    [&library] {
                        const LibraryStore &store = library.store();
                        const LibrarySnapshot &snapshot = store.snapshot();
                        BackupPlan::Sources s;
                        s.dataDir = store.dataDir();
                        s.home = store.installLocations().home;
                        s.library = {store.versionStatus().version, store.versionStatus().id};
                        s.platforms = snapshot.content.platforms;
                        s.games = library.allGames();
                        s.emulators = library.emulators();
                        s.installs = library.installs();
                        if (!snapshot.steam.libraries.isEmpty())
                            s.compatdata = snapshot.steam.libraries.first() + QStringLiteral("/compatdata");
                        return s;
                    },
                    [&library, &runner] {
                        QStringList titles;
                        for (const QString &id : runner.running()) {
                            const Game *game = library.game(id);
                            titles.append(game ? game->title : id);
                        }
                        return titles;
                    },
                    [&library](bool on) { library.store().hold(on); },
                    [&library, arguments] {
                        library.store().forgetCache();
                        QString error;
                        if (Restart::start(arguments, &error))
                            qInfo() << "restarting on the restored library";
                        else
                            qCritical().noquote() << "can't start the app again after the import:" << error;
                        QCoreApplication::quit();
                    }});
    Installer::setup({AppPaths::stateDir(), AppPaths::cacheDir(), [&library] { return library.emulators(); },
                      [&library] { return library.store().installLocations(); }, [&library] { library.scanNow(); },
                      [] { return programEnvironment(QProcessEnvironment::systemEnvironment().toStringList()); },
                      [](const QString &path, QString *error) { return Uninstall::systemTrash(path, error); },
                      [&library, &runner](const QString &emulatorName) {
                          const QString id = runner.runningIn(emulatorName);
                          const Game *game = id.isEmpty() ? nullptr : library.game(id);
                          return game ? game->title : id;
                      }});
    ThumbnailCache thumbnails(AppPaths::cacheDir());
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&thumbnails);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Launcher", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
        QObject::connect(
            window, &QQuickWindow::frameSwapped, window,
            [&startup, &library, &runner, &general, &instance, window] {
                qInfo() << "first frame after" << startup.elapsed() << "ms";
                window->setProperty("startedUp", true);
                QTimer::singleShot(0, &library, &Library::startRefresh);
                QTimer::singleShot(0, &runner, &GameRunner::start);
                QTimer::singleShot(0, &general, &General::refreshAutostart);
                if (!instance.listen())
                    qWarning() << "can't listen for other copies of the app starting";
            },
            Qt::SingleShotConnection);

        QObject::connect(&instance, &SingleInstance::activateRequested, window, [window] {
            // On Wayland the compositor may only mark the window as urgent instead (focus
            // stealing prevention; Hyprland: misc:focus_on_activate).
            window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
            window->show();
            window->raise();
            window->requestActivate();
        });

        const QString screenshot = qEnvironmentVariable("LATTIS_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            bool ok = false;
            const int delay = qEnvironmentVariableIntValue("LATTIS_SCREENSHOT_DELAY", &ok);
            const int actualDelay = ok ? delay : 1500;
            if (const QString game = qEnvironmentVariable("LATTIS_SCREENSHOT_LAUNCH"); !game.isEmpty()) {
                QTimer::singleShot(actualDelay / 2, window, [window, game] {
                    logHook("selectAndLaunch", game);
                    QMetaObject::invokeMethod(window, "selectAndLaunch", Q_ARG(QVariant, game));
                });
            }
            const QStringList select =
                qEnvironmentVariable("LATTIS_SELECT_GAME").split(u',', Qt::SkipEmptyParts);
            for (qsizetype i = 0; i < select.size(); ++i) {
                // Precise: a timer of 2 s or more is coarse by default, and Qt fires neighbouring
                // ones together, so the games would be selected in pairs.
                const auto when = std::chrono::milliseconds(actualDelay / 2 + int(i) * 500);
                QTimer::singleShot(when, Qt::PreciseTimer, window, [window, game = select[i]] {
                    logHook("selectGame", game);
                    QMetaObject::invokeMethod(window, "selectGame", Q_ARG(QVariant, game));
                });
            }
            if (const QString game = qEnvironmentVariable("LATTIS_EDIT_GAME"); !game.isEmpty()) {
                QTimer::singleShot(actualDelay / 2, window, [window, game] {
                    logHook("selectAndEdit", game);
                    QMetaObject::invokeMethod(window, "selectAndEdit", Q_ARG(QVariant, game));
                });
            }
            if (const QString file = qEnvironmentVariable("LATTIS_ADD_GAME"); !file.isEmpty()) {
                QTimer::singleShot(actualDelay / 2, window, [window, file] {
                    logHook("addGameFile", file);
                    QMetaObject::invokeMethod(window, "addGameFile", Q_ARG(QVariant, file));
                });
            }
            if (const QString change = qEnvironmentVariable("LATTIS_UNDO"); !change.isEmpty()) {
                QTimer::singleShot(actualDelay * 5 / 8, window, [window, change] {
                    logHook("makeUndoableChange", change);
                    QMetaObject::invokeMethod(window, "makeUndoableChange", Q_ARG(QVariant, change));
                });
            }
            if (const QString spec = qEnvironmentVariable("LATTIS_DRAG_PLATFORM"); !spec.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, spec] {
                    logHook("dragPlatformForScreenshot", spec);
                    QMetaObject::invokeMethod(window, "dragPlatformForScreenshot", Q_ARG(QVariant, spec));
                });
            }
            if (const QString spec = qEnvironmentVariable("LATTIS_INSTALL"); !spec.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [spec] {
                    logHook("install", spec);
                    for (const QString &one : spec.split(u',', Qt::SkipEmptyParts)) {
                        const QStringList parts = one.split(u'/');
                        Installer::instance()->install(parts.first(), parts.value(1, QStringLiteral("appimage")));
                    }
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_STEAM_UNDO") == 1) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window] {
                    logHook("askSteamUndo");
                    QMetaObject::invokeMethod(window, "askSteamUndo");
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_UNINSTALL") == 1) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window] {
                    logHook("uninstallInEditor");
                    QMetaObject::invokeMethod(window, "uninstallInEditor");
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_LOOK_UP") == 1) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window] {
                    logHook("lookUpInEditor");
                    QMetaObject::invokeMethod(window, "lookUpInEditor");
                });
            }
            if (const QString bulk = qEnvironmentVariable("LATTIS_BULK_LOOKUP"); !bulk.isEmpty() && bulk != u'0') {
                const bool replace = qEnvironmentVariableIntValue("LATTIS_BULK_REPLACE") == 1;
                const QString size = qEnvironmentVariable("LATTIS_BULK_SIZE");
                QTimer::singleShot(actualDelay / 2, window, [window, bulk, replace, size] {
                    logHook("openBulkLookupForScreenshot");
                    QMetaObject::invokeMethod(window, "openBulkLookupForScreenshot", Q_ARG(QVariant, bulk),
                                              Q_ARG(QVariant, replace), Q_ARG(QVariant, size));
                });
            }
            if (qEnvironmentVariableIsSet("LATTIS_BULK_TAG_SEARCH")) {
                const QString search = qEnvironmentVariable("LATTIS_BULK_TAG_SEARCH");
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, search] {
                    logHook("bulkTagsForScreenshot", search);
                    QMetaObject::invokeMethod(window, "bulkTagsForScreenshot", Q_ARG(QVariant, search));
                });
            }
            if (const QString state = qEnvironmentVariable("LATTIS_BULK_FILTER"); !state.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, state] {
                    logHook("bulkFilterForScreenshot", state);
                    QMetaObject::invokeMethod(window, "bulkFilterForScreenshot", Q_ARG(QVariant, state));
                });
            }
            if (const QString name = qEnvironmentVariable("LATTIS_DETAILED_LOOKUP"); !name.isEmpty() && name != u'0') {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, name] {
                    logHook("detailedLookupInEditor", name);
                    QMetaObject::invokeMethod(window, "detailedLookupInEditor", Q_ARG(QVariant, name));
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_DETAILED_USE") == 1) {
                QTimer::singleShot(actualDelay * 3 / 4 + 700, window, [window] {
                    logHook("useDetailedLookupInEditor");
                    QMetaObject::invokeMethod(window, "useDetailedLookupInEditor");
                });
            }
            if (const QString kind = qEnvironmentVariable("LATTIS_ART_BROWSER"); !kind.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4 + 700, window, [window, kind] {
                    logHook("browseArtInEditor", kind);
                    QMetaObject::invokeMethod(window, "browseArtInEditor", Q_ARG(QVariant, kind));
                });
            }
            if (const QString mode = qEnvironmentVariable("LATTIS_LOGO_MENU"); !mode.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, mode] {
                    logHook("logoForScreenshot", mode);
                    QMetaObject::invokeMethod(window, "logoForScreenshot", Q_ARG(QVariant, mode));
                });
            }
            if (const QString id = qEnvironmentVariable("LATTIS_GAME_MENU"); !id.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, id] {
                    logHook("gameMenuForScreenshot", id);
                    QMetaObject::invokeMethod(window, "gameMenuForScreenshot", Q_ARG(QVariant, id));
                });
            }
            if (const QString spec = qEnvironmentVariable("LATTIS_GAME_PAGE_MENU"); !spec.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, spec] {
                    logHook("gamePageMenuForScreenshot", spec);
                    QMetaObject::invokeMethod(window, "gamePageMenuForScreenshot", Q_ARG(QVariant, spec));
                });
            }
            if (const QString ids = qEnvironmentVariable("LATTIS_SELECT_GAMES"); !ids.isEmpty()) {
                QTimer::singleShot(actualDelay / 2, window, [window, ids] {
                    logHook("selectGamesForScreenshot", ids);
                    QMetaObject::invokeMethod(window, "selectGamesForScreenshot", Q_ARG(QVariant, ids));
                });
            }
            if (const QString id = qEnvironmentVariable("LATTIS_PLAY_HISTORY"); !id.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, id] {
                    logHook("playHistoryForScreenshot", id);
                    QMetaObject::invokeMethod(window, "playHistoryForScreenshot", Q_ARG(QVariant, id));
                });
            }
            if (const QString month = qEnvironmentVariable("LATTIS_STATS"); !month.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, month] {
                    logHook("statsForScreenshot", month);
                    QMetaObject::invokeMethod(window, "statsForScreenshot", Q_ARG(QVariant, month));
                });
            }
            if (const QString mode = qEnvironmentVariable("LATTIS_APP_MENU"); !mode.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, mode] {
                    logHook("appMenuForScreenshot", mode);
                    QMetaObject::invokeMethod(window, "appMenuForScreenshot", Q_ARG(QVariant, mode));
                });
            }
            if (const QString id = qEnvironmentVariable("LATTIS_PLATFORM_MENU"); !id.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, id] {
                    logHook("platformMenuForScreenshot", id);
                    QMetaObject::invokeMethod(window, "platformMenuForScreenshot", Q_ARG(QVariant, id));
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_RAIL_MENU") == 1) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window] {
                    logHook("railMenuForScreenshot", {});
                    QMetaObject::invokeMethod(window, "railMenuForScreenshot");
                });
            }
            if (const QString action = qEnvironmentVariable("LATTIS_ACTION"); !action.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, action] {
                    logHook("run", action);
                    QMetaObject::invokeMethod(window, "run", Q_ARG(QVariant, action));
                });
            }
            if (const QString step = qEnvironmentVariable("LATTIS_SETUP_STEP"); !step.isEmpty()) {
                QTimer::singleShot(actualDelay / 2, window, [window, step] {
                    logHook("openSetupAt", step);
                    QMetaObject::invokeMethod(window, "openSetupAt", Q_ARG(QVariant, step));
                });
            }
            if (const QString folder = qEnvironmentVariable("LATTIS_FIND_ROM_FOLDERS"); !folder.isEmpty()) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window, folder] {
                    logHook("openFindRomFoldersForScreenshot", folder);
                    QMetaObject::invokeMethod(window, "openFindRomFoldersForScreenshot", Q_ARG(QVariant, folder));
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_TEST_METADATA_KEY") == 1) {
                QTimer::singleShot(actualDelay * 3 / 4, window, [window] {
                    logHook("testMetadataKeyForScreenshot");
                    QMetaObject::invokeMethod(window, "testMetadataKeyForScreenshot");
                });
            }
            if (qEnvironmentVariableIntValue("LATTIS_OPEN_FILTERS") == 1) {
                QTimer::singleShot(actualDelay / 2, window, [window] {
                    logHook("run", QStringLiteral("open_filters"));
                    QMetaObject::invokeMethod(window, "run", Q_ARG(QVariant, QStringLiteral("open_filters")));
                });
            }
            const QString openAt = qEnvironmentVariable("LATTIS_OPEN_SETTINGS");
            const bool openSettings = !openAt.isEmpty() && openAt != u'0';
            const QString settingsScreenshot = qEnvironmentVariable("LATTIS_SCREENSHOT_SETTINGS");

            if (const int speed = qEnvironmentVariableIntValue("LATTIS_SCROLL"); speed > 0) {
                QTimer::singleShot(actualDelay / 2, window, [window, speed] {
                    logHook("scrollList", QVariant(speed).toString());
                    QMetaObject::invokeMethod(window, "scrollList", Q_ARG(QVariant, speed));
                });
            }
            QTimer::singleShot(actualDelay, window, [window, screenshot, openSettings, openAt, settingsScreenshot, actualDelay] {
                window->grabWindow().save(screenshot);
                logMemory();
                if (const QString bulk = qEnvironmentVariable("LATTIS_SCREENSHOT_BULK"); !bulk.isEmpty()) {
                    for (QWindow *w : QGuiApplication::topLevelWindows()) {
                        if (auto *quick = qobject_cast<QQuickWindow *>(w); quick && w->objectName() == QLatin1String("bulkLookupWindow"))
                            quick->grabWindow().save(bulk);
                    }
                }
                if (!openSettings || settingsScreenshot.isEmpty()) {
                    QCoreApplication::quit();
                    return;
                }
                if (openAt == u'1') {
                    logHook("openSettings");
                    QMetaObject::invokeMethod(window, "openSettings");
                } else {
                    const QStringList parts = openAt.split(u'/');
                    logHook("openSettingsAt", openAt);
                    QMetaObject::invokeMethod(window, "openSettingsAt", Q_ARG(QVariant, parts.first()),
                                              Q_ARG(QVariant, parts.size() > 1 ? QVariant(parts.at(1)) : QVariant()));
                }
                if (const QString spec = qEnvironmentVariable("LATTIS_DRAG_PLATFORM"); !spec.isEmpty()) {
                    QTimer::singleShot(actualDelay / 2, window, [window, spec] {
                        logHook("dragPlatformInSettings", spec);
                        QMetaObject::invokeMethod(window, "dragPlatformInSettings", Q_ARG(QVariant, spec));
                    });
                }
                QTimer::singleShot(actualDelay, window, [settingsScreenshot] {
                    for (QWindow *w : QGuiApplication::allWindows()) {
                        if (w->objectName() == QStringLiteral("settingsWindow")) {
                            if (auto *quick = qobject_cast<QQuickWindow *>(w))
                                quick->grabWindow().save(settingsScreenshot);
                            break;
                        }
                    }
                    logMemory();
                    QCoreApplication::quit();
                });
            });
        }
    }

    return app.exec();
}
