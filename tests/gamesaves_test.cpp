#include "gamesaves.h"
#include "librarytestbase.h"
#include "pluralfallback.h"
#include "savesbackuplibrary.h"
#include "savesmanifest.h"

#include <QLocale>

using SavesBackup::Input;

namespace {
QDateTime at(int seconds)
{
    return QDateTime(QDate(2026, 11, 3), QTime(12, 0, 0)).addSecs(seconds);
}

QStringList pathsOf(const QVariantMap &state)
{
    QStringList out;
    for (const QVariant &path : state.value(QStringLiteral("paths")).toList())
        out.append(path.toString());
    return out;
}

QStringList archivesOf(const QVariantMap &state)
{
    QStringList out;
    for (const QVariant &backup : state.value(QStringLiteral("backups")).toList())
        out.append(backup.toMap().value(QStringLiteral("path")).toString());
    return out;
}
}

class GameSavesTest : public LibraryTestBase
{
    Q_OBJECT

    QString m_home;
    QString m_folder;
    QString m_manifest;
    QHash<QString, Input> m_inputs;
    QList<Input> m_played;
    QStringList m_trashed;
    bool m_trashWorks = true;
    QString m_chooseError;

    // Each test's own folders, inside the fixture's scratch dir.
    void start()
    {
        m_home = m_installs.home;
        m_folder = m_dir->filePath(QStringLiteral("backups"));
        m_manifest = m_dir->filePath(QStringLiteral("manifest"));
        m_inputs.clear();
        m_played.clear();
        m_trashed.clear();
        m_trashWorks = true;
        m_chooseError.clear();
        SavesIndex index;
        for (const QString &name : {QStringLiteral("Celeste"), QStringLiteral("Celeste Classic"), QStringLiteral("Hades")}) {
            SavesGame game;
            game.name = name;
            game.paths.append(SavesPath{QStringLiteral("<xdgData>/") + name + QStringLiteral("/Saves"), {}});
            index.games.append(game);
        }
        for (int i = 0; i < 40; ++i) {
            SavesGame game;
            game.name = QStringLiteral("Puzzle %1").arg(i);
            index.games.append(game);
        }
        QVERIFY(QDir().mkpath(m_manifest));
        QVERIFY(SavesIndexFile::save(m_manifest + QStringLiteral("/index.bin"), index));
    }

    SavesBackup::Trash trash()
    {
        return [this](const QString &path, QString *error) {
            if (!m_trashWorks) {
                *error = QStringLiteral("there's no trash here");
                return false;
            }
            m_trashed.append(path);
            return QFile::remove(path);
        };
    }

    Input game(const QString &id, const QString &title)
    {
        Input in;
        in.gameId = id;
        in.title = title;
        in.env.home = m_home;
        in.env.xdgData = m_home + QStringLiteral("/.local/share");
        in.env.xdgConfig = m_home + QStringLiteral("/.config");
        in.env.user = QStringLiteral("someone");
        SavesFinder::Query query;
        query.id = id;
        query.title = title;
        in.copies.append(query);
        in.manifestDir = m_manifest;
        in.options.folder = m_folder;
        in.appVersion = QStringLiteral("1.2.3");
        in.now = at(0);
        in.trash = trash();
        return in;
    }

    Input celeste() { return game(QStringLiteral("linux-celeste"), QStringLiteral("Celeste")); }

    Input azahar()
    {
        Input in = game(QStringLiteral("emulator-azahar"), QStringLiteral("Azahar"));
        in.copies.clear();
        in.emulatorId = QStringLiteral("azahar");
        in.emulatorPaths = {m_home + QStringLiteral("/emu/sdmc")};
        return in;
    }

    void writeSave(const QString &title, const QByteArray &content = "slot")
    {
        writeFile(m_home + QStringLiteral("/.local/share/") + title + QStringLiteral("/Saves/0.sav"), content);
    }

    // A backup of `in` made at `seconds` past noon.
    QString backUp(Input in, int seconds)
    {
        in.now = at(seconds);
        const SavesBackup::Result result = SavesBackup::run(in);
        if (result.status != SavesBackup::Status::Written)
            qWarning("backUp: no backup written: %s", qPrintable(result.error));
        return result.archive;
    }

    GameSaves::Setup setup()
    {
        GameSaves::Setup out;
        out.input = [this](const QString &id) -> std::optional<Input> {
            if (!m_inputs.contains(id))
                return std::nullopt;
            return m_inputs.value(id);
        };
        out.playedInputs = [this] { return m_played; };
        out.choose = [this](const QString &id, const QString &name) {
            if (!m_chooseError.isEmpty())
                return m_chooseError;
            if (!m_inputs.contains(id))
                return QStringLiteral("No game %1").arg(id);
            m_inputs[id].copies[0].savesGame = name;
            return QString();
        };
        out.trash = trash();
        return out;
    }

    // What the page shows for a game; waits for the read on the worker thread.
    static QVariantMap loaded(GameSaves &saves, const QString &id)
    {
        QSignalSpy changed(&saves, &GameSaves::stateChanged);
        saves.load(id);
        if (changed.isEmpty() && !changed.wait(5000))
            qWarning("loaded: the state never came");
        return saves.state();
    }

    static QString backedUp(GameSaves &saves, const QString &id)
    {
        QSignalSpy done(&saves, &GameSaves::backedUp);
        saves.backUpNow(id);
        if (done.isEmpty() && !done.wait(10000))
            return QStringLiteral("(no answer)");
        return done.first().at(1).toString();
    }

    static QString backedUpAll(GameSaves &saves)
    {
        QSignalSpy done(&saves, &GameSaves::backedUpAll);
        saves.backUpAll();
        if (done.isEmpty() && !done.wait(10000))
            return QStringLiteral("(no answer)");
        return saves.message();
    }

    // The library's own inputs, as the app wires them, with the manifest and the trash kept in the test.
    GameSaves::Setup librarySetup(Library &library)
    {
        GameSaves::Setup out;
        out.input = [this, &library](const QString &id) {
            std::optional<Input> in = SavesBackup::inputFor(library, id, m_data, QStringLiteral("1.2.3"), true);
            if (in) {
                in->manifestDir = m_manifest;
                in->trash = trash();
            }
            return in;
        };
        out.trash = trash();
        return out;
    }

    void addMelonDs(const QStringList &romFolders)
    {
        const QString app = m_installs.flatpakDirs.first() + QStringLiteral("/net.kuribo64.melonDS/current/active");
        writeFile(app + QStringLiteral("/metadata"), "[Context]\nfilesystems=host;\n");
        QByteArray platform = "\n[[platform]]\nid = \"nds\"\nname = \"Nintendo DS\"\nicon = \"nds\"\n"
                              "emulators = [\"melonds\"]\nrom_folders = [";
        for (const QString &folder : romFolders)
            platform += (folder == romFolders.first() ? "\"" : ", \"") + folder.toUtf8() + "\"";
        platform += "]\n";
        const QString platforms = m_data + QStringLiteral("/library/platforms.toml");
        writeFile(platforms, readFile(platforms) + platform);
    }

private slots:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        QCoreApplication::installTranslator(new PluralFallback(qApp));
    }

    void aGameThatCantBeBackedUpShowsNothing();
    void renameAndRemoveRefuseABackupThatIsntTheGames();
    void aTrashThatRefusesKeepsTheBackup();
    void removeTrashesTheBackupAndItsName();
    void renameNamesAndClears();
    void loadShowsTheSavesAndTheBackups();
    void loadForAnEmulatorShowsItsPlaces();
    void aProtonGameWithNothingFoundFallsBackToItsPrefix();
    void loadLastShowsTheNewestBackup();
    void findPathsGivesTheSavesAsABackupFindsThem();
    void backUpNowSaysWhatCameOfIt();
    void backUpNowShowsTheMessageOnlyOnItsGamesPage();
    void backUpAllCountsTheGames();
    void searchLooksInTheManifestOnceItIsRead();
    void chooseWritesTheChoiceAndReloads();
    void theNewestBackupStaysNewestWhenTheClockGoesBack();

    void anEmulatedGameGetsItsEmulatorsSavePlaces();
    void anEmulatedGameWithItsOwnSavePathsIsBackedUpAlone();
    void savesBesideTheRomComeWithTheirEmulator();
};

void GameSavesTest::aGameThatCantBeBackedUpShowsNothing()
{
    start();
    GameSaves saves(setup());

    const QVariantMap state = loaded(saves, QStringLiteral("steam-1"));
    QCOMPARE(state.value(QStringLiteral("eligible")).toBool(), false);
    QCOMPARE(state.value(QStringLiteral("requested")).toString(), QStringLiteral("steam-1"));
    QVERIFY(!state.contains(QStringLiteral("paths")));

    QCOMPARE(saves.lastFor(QStringLiteral("steam-1")).value(QStringLiteral("pending")).toBool(), true);
    saves.loadLast(QStringLiteral("steam-1"));
    QCOMPARE(saves.last(), (QVariantMap{{QStringLiteral("requested"), QStringLiteral("steam-1")}}));
    QVERIFY(!saves.lastFor(QStringLiteral("steam-1")).contains(QStringLiteral("pending")));

    bool called = false;
    saves.findPaths(QStringLiteral("steam-1"), [&](const QStringList &paths) {
        called = true;
        QVERIFY(paths.isEmpty());
    });
    QVERIFY(called);

    QVERIFY(saves.rename(QStringLiteral("steam-1"), m_folder + QStringLiteral("/x.tar.zst"), QStringLiteral("n")).isEmpty());
    QVERIFY(saves.remove(QStringLiteral("steam-1"), m_folder + QStringLiteral("/x.tar.zst")).isEmpty());
    QCOMPARE(saves.choose(QStringLiteral("steam-1"), QStringLiteral("Celeste")), QStringLiteral("No game steam-1"));

    QCOMPARE(backedUp(saves, QStringLiteral("steam-1")), QStringLiteral("This game's saves can't be backed up."));
    QVERIFY(!saves.working());
    QVERIFY(!QFileInfo::exists(m_folder));
    QVERIFY(m_trashed.isEmpty());

    // No list of played games at all (the setup gave none): nothing happens.
    GameSaves bare(GameSaves::Setup{});
    QSignalSpy all(&bare, &GameSaves::backedUpAll);
    bare.backUpAll();
    QVERIFY(all.isEmpty());
    QVERIFY(bare.choose(QStringLiteral("linux-celeste"), QStringLiteral("Celeste")).isEmpty());
}

// The page passes the path it listed; anything else (another game's backup, a file that isn't a backup, one
// that's gone) is refused and left alone.
void GameSavesTest::renameAndRemoveRefuseABackupThatIsntTheGames()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    m_inputs.insert(QStringLiteral("linux-hades"), game(QStringLiteral("linux-hades"), QStringLiteral("Hades")));
    writeSave(QStringLiteral("Celeste"));
    writeSave(QStringLiteral("Hades"));
    const QString own = backUp(celeste(), 0);
    const QString other = backUp(m_inputs.value(QStringLiteral("linux-hades")), 0);
    const QString notes = m_folder + QStringLiteral("/linux-celeste/notes.txt");
    writeFile(notes, "mine");
    const QString outside = m_dir->filePath(QStringLiteral("elsewhere/2026-11-03-120000.tar.zst"));
    writeFile(outside, "not a backup of this game");
    GameSaves saves(setup());

    for (const QString &path : {other, notes, outside, own + QStringLiteral(".gone")}) {
        QCOMPARE(saves.remove(QStringLiteral("linux-celeste"), path), QStringLiteral("That backup is gone."));
        QCOMPARE(saves.rename(QStringLiteral("linux-celeste"), path, QStringLiteral("Before the boss")),
                 QStringLiteral("That backup is gone."));
        QVERIFY(!QFileInfo::exists(path + QStringLiteral(".name")));
    }
    QVERIFY(m_trashed.isEmpty());
    QVERIFY(QFileInfo::exists(other));
    QCOMPARE(readFile(notes), QByteArray("mine"));
    QVERIFY(QFileInfo::exists(outside));
    QVERIFY(QFileInfo::exists(own));
}

void GameSavesTest::aTrashThatRefusesKeepsTheBackup()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    writeSave(QStringLiteral("Celeste"));
    const QString archive = backUp(celeste(), 0);
    QVERIFY(SavesBackup::setBackupName(archive, QStringLiteral("Chapter 7")).isEmpty());
    m_trashWorks = false;
    GameSaves saves(setup());
    QSignalSpy changed(&saves, &GameSaves::stateChanged);

    QCOMPARE(saves.remove(QStringLiteral("linux-celeste"), archive),
             QStringLiteral("Couldn't delete the backup: there's no trash here"));
    QVERIFY(QFileInfo::exists(archive));
    QCOMPARE(SavesBackup::backupName(archive), QStringLiteral("Chapter 7"));
    QVERIFY(changed.isEmpty());
}

void GameSavesTest::removeTrashesTheBackupAndItsName()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    writeSave(QStringLiteral("Celeste"));
    const QString older = backUp(celeste(), 0);
    writeSave(QStringLiteral("Celeste"), "changed");
    const QString newer = backUp(celeste(), 60);
    QVERIFY(SavesBackup::setBackupName(newer, QStringLiteral("Chapter 7")).isEmpty());
    GameSaves saves(setup());
    saves.loadLast(QStringLiteral("linux-celeste"));
    QTRY_COMPARE(saves.last().value(QStringLiteral("name")).toString(), QStringLiteral("Chapter 7"));

    QSignalSpy changed(&saves, &GameSaves::stateChanged);
    QVERIFY(saves.remove(QStringLiteral("linux-celeste"), newer).isEmpty());
    QCOMPARE(m_trashed, QStringList{newer});
    QVERIFY(!QFileInfo::exists(newer));
    QVERIFY(!QFileInfo::exists(newer + QStringLiteral(".name")));
    QVERIFY(QFileInfo::exists(older));

    // The page and its chip follow.
    QVERIFY(changed.wait(5000));
    QCOMPARE(archivesOf(saves.state()), QStringList{older});
    QTRY_VERIFY(saves.last().value(QStringLiteral("name")).toString().isEmpty());
    QVERIFY(saves.last().contains(QStringLiteral("when")));
}

void GameSavesTest::renameNamesAndClears()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    writeSave(QStringLiteral("Celeste"));
    const QString archive = backUp(celeste(), 0);
    GameSaves saves(setup());

    QSignalSpy changed(&saves, &GameSaves::stateChanged);
    QVERIFY(saves.rename(QStringLiteral("linux-celeste"), archive, QStringLiteral("  Before the boss ")).isEmpty());
    QCOMPARE(SavesBackup::backupName(archive), QStringLiteral("Before the boss"));
    QVERIFY(changed.wait(5000));
    const QVariantList backups = saves.state().value(QStringLiteral("backups")).toList();
    QCOMPARE(backups.size(), 1);
    QCOMPARE(backups.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Before the boss"));

    QVERIFY(saves.rename(QStringLiteral("linux-celeste"), archive, QString()).isEmpty());
    QVERIFY(!QFileInfo::exists(archive + QStringLiteral(".name")));
    QVERIFY(SavesBackup::backupName(archive).isEmpty());
}

void GameSavesTest::loadShowsTheSavesAndTheBackups()
{
    start();
    Input in = celeste();
    in.options.keep = 3;
    m_inputs.insert(QStringLiteral("linux-celeste"), in);
    GameSaves saves(setup());

    // Nothing saved yet: matched, but no paths and no backups.
    QVariantMap state = loaded(saves, QStringLiteral("linux-celeste"));
    QCOMPARE(state.value(QStringLiteral("eligible")).toBool(), true);
    QCOMPARE(state.value(QStringLiteral("requested")).toString(), QStringLiteral("linux-celeste"));
    QCOMPARE(state.value(QStringLiteral("matched")).toString(), QStringLiteral("Celeste"));
    QCOMPARE(state.value(QStringLiteral("match")).toString(), QStringLiteral("title"));
    QVERIFY(pathsOf(state).isEmpty());
    QVERIFY(archivesOf(state).isEmpty());
    QCOMPARE(state.value(QStringLiteral("fallback")).toBool(), false);

    writeSave(QStringLiteral("Celeste"));
    const QString older = backUp(in, 0);
    writeSave(QStringLiteral("Celeste"), "changed");
    const QString newer = backUp(in, 60);
    state = loaded(saves, QStringLiteral("linux-celeste"));
    QCOMPARE(state.value(QStringLiteral("id")).toString(), QStringLiteral("linux-celeste"));
    QCOMPARE(state.value(QStringLiteral("title")).toString(), QStringLiteral("Celeste"));
    QVERIFY(state.value(QStringLiteral("emulator")).toString().isEmpty());
    QCOMPARE(state.value(QStringLiteral("hasManifest")).toBool(), true);
    QCOMPARE(pathsOf(state), QStringList{QStringLiteral("~/.local/share/Celeste/Saves")});
    QCOMPARE(archivesOf(state), (QStringList{newer, older}));
    const QVariantMap first = state.value(QStringLiteral("backups")).toList().first().toMap();
    QCOMPARE(first.value(QStringLiteral("size")).toString(),
             QLocale().formattedDataSize(QFileInfo(newer).size(), 1, QLocale::DataSizeTraditionalFormat));
    QVERIFY(first.value(QStringLiteral("when")).toString().contains(u','));
    QCOMPARE(state.value(QStringLiteral("folder")).toString(), m_folder + QStringLiteral("/linux-celeste"));
    QCOMPARE(state.value(QStringLiteral("keep")).toInt(), 3);
}

void GameSavesTest::loadForAnEmulatorShowsItsPlaces()
{
    start();
    m_inputs.insert(QStringLiteral("3ds-game"), azahar());
    writeFile(m_home + QStringLiteral("/emu/sdmc/save.bin"), "3ds");
    GameSaves saves(setup());

    const QVariantMap state = loaded(saves, QStringLiteral("3ds-game"));
    QCOMPARE(state.value(QStringLiteral("requested")).toString(), QStringLiteral("3ds-game"));
    QCOMPARE(state.value(QStringLiteral("id")).toString(), QStringLiteral("emulator-azahar"));
    QCOMPARE(state.value(QStringLiteral("emulator")).toString(), QStringLiteral("Azahar"));
    QCOMPARE(pathsOf(state), QStringList{QStringLiteral("~/emu/sdmc")});
    QVERIFY(state.value(QStringLiteral("match")).toString().isEmpty());
    // No manifest is read for an emulator, so search has nothing to look in.
    QCOMPARE(state.value(QStringLiteral("hasManifest")).toBool(), false);
    QVERIFY(saves.search(QStringLiteral("celeste")).isEmpty());
    QCOMPARE(state.value(QStringLiteral("folder")).toString(), m_folder + QStringLiteral("/emulator-azahar"));
}

void GameSavesTest::aProtonGameWithNothingFoundFallsBackToItsPrefix()
{
    start();
    Input in = game(QStringLiteral("proton-unknown"), QStringLiteral("Nobody Listed"));
    in.protonPrefix = m_home + QStringLiteral("/compatdata/9");
    m_inputs.insert(in.gameId, in);
    GameSaves saves(setup());

    const QVariantMap state = loaded(saves, in.gameId);
    QCOMPARE(state.value(QStringLiteral("fallback")).toBool(), true);
    QCOMPARE(pathsOf(state), QStringList{QStringLiteral("~/compatdata/9")});
    QVERIFY(state.value(QStringLiteral("matched")).toString().isEmpty());
}

void GameSavesTest::loadLastShowsTheNewestBackup()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    m_inputs.insert(QStringLiteral("linux-hades"), game(QStringLiteral("linux-hades"), QStringLiteral("Hades")));
    GameSaves saves(setup());

    // No backup yet: the chip has nothing to show, and it's no longer pending.
    saves.loadLast(QStringLiteral("linux-celeste"));
    QTRY_VERIFY(!saves.lastFor(QStringLiteral("linux-celeste")).contains(QStringLiteral("pending")));
    QCOMPARE(saves.last(), (QVariantMap{{QStringLiteral("requested"), QStringLiteral("linux-celeste")}}));

    writeSave(QStringLiteral("Celeste"));
    backUp(celeste(), 0);
    writeSave(QStringLiteral("Celeste"), "a longer slot");
    const QString newer = backUp(celeste(), 60);
    QVERIFY(SavesBackup::setBackupName(newer, QStringLiteral("Summit")).isEmpty());
    saves.loadLast(QStringLiteral("linux-celeste"));
    QTRY_COMPARE(saves.last().value(QStringLiteral("name")).toString(), QStringLiteral("Summit"));
    QCOMPARE(saves.last().value(QStringLiteral("size")).toString(),
             QLocale().formattedDataSize(QFileInfo(newer).size(), 1, QLocale::DataSizeTraditionalFormat));
    QCOMPARE(saves.lastFor(QStringLiteral("linux-celeste")), saves.last());

    // Another game's chip replaces it; the first one is remembered.
    saves.loadLast(QStringLiteral("linux-hades"));
    QTRY_COMPARE(saves.last().value(QStringLiteral("requested")).toString(), QStringLiteral("linux-hades"));
    QVERIFY(!saves.last().contains(QStringLiteral("when")));
    QCOMPARE(saves.lastFor(QStringLiteral("linux-celeste")).value(QStringLiteral("name")).toString(), QStringLiteral("Summit"));
}

void GameSavesTest::findPathsGivesTheSavesAsABackupFindsThem()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    m_inputs.insert(QStringLiteral("3ds-game"), azahar());
    writeSave(QStringLiteral("Celeste"));
    GameSaves saves(setup());

    QStringList found;
    bool done = false;
    saves.findPaths(QStringLiteral("linux-celeste"), [&](const QStringList &paths) {
        found = paths;
        done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(found, QStringList{m_home + QStringLiteral("/.local/share/Celeste/Saves")});
    // The manifest it read is kept for search.
    QCOMPARE(saves.search(QStringLiteral("hades")), QVariantList{QStringLiteral("Hades")});

    done = false;
    saves.findPaths(QStringLiteral("3ds-game"), [&](const QStringList &paths) {
        found = paths;
        done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(found, QStringList{m_home + QStringLiteral("/emu/sdmc")});
}

void GameSavesTest::backUpNowSaysWhatCameOfIt()
{
    start();
    GameSaves saves(setup());
    saves.open(QStringLiteral("linux-celeste"));
    saves.loadLast(QStringLiteral("linux-celeste"));

    // The backup folder can't be made: a file is in its way.
    Input blocked = celeste();
    blocked.options.folder = m_dir->filePath(QStringLiteral("a-file"));
    writeFile(blocked.options.folder, "in the way");
    m_inputs.insert(QStringLiteral("linux-celeste"), blocked);
    writeSave(QStringLiteral("Celeste"));
    QCOMPARE(backedUp(saves, QStringLiteral("linux-celeste")),
             QStringLiteral("Couldn't back up: Can't create %1/linux-celeste").arg(blocked.options.folder));
    QCOMPARE(saves.message(), QStringLiteral("Couldn't back up: Can't create %1/linux-celeste").arg(blocked.options.folder));
    QVERIFY(!saves.working());
    QCOMPARE(readFile(blocked.options.folder), QByteArray("in the way"));

    Input capped = celeste();
    capped.options.maxBytes = 2;
    m_inputs.insert(QStringLiteral("linux-celeste"), capped);
    QCOMPARE(backedUp(saves, QStringLiteral("linux-celeste")),
             QStringLiteral("The saves come to 4 bytes, over the limit in Settings → Library."));
    QVERIFY(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")).isEmpty());

    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    QCOMPARE(backedUp(saves, QStringLiteral("linux-celeste")), QStringLiteral("Backed up 1 file, 4 bytes."));
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")).size(), 1);
    // The page reloads with the new backup, and the chip with it.
    QTRY_COMPARE(archivesOf(saves.state()).size(), 1);
    QTRY_VERIFY(saves.last().contains(QStringLiteral("when")));

    QCOMPARE(backedUp(saves, QStringLiteral("linux-celeste")),
             QStringLiteral("The saves haven't changed since the last backup."));
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")).size(), 1);

    QVERIFY(QDir(m_home + QStringLiteral("/.local/share/Celeste")).removeRecursively());
    QCOMPARE(backedUp(saves, QStringLiteral("linux-celeste")), QStringLiteral("No saves found to back up."));
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")).size(), 1);
}

void GameSavesTest::backUpNowShowsTheMessageOnlyOnItsGamesPage()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    m_inputs.insert(QStringLiteral("linux-hades"), game(QStringLiteral("linux-hades"), QStringLiteral("Hades")));
    writeSave(QStringLiteral("Hades"));
    GameSaves saves(setup());
    saves.open(QStringLiteral("linux-celeste"));

    QSignalSpy working(&saves, &GameSaves::workingChanged);
    QSignalSpy done(&saves, &GameSaves::backedUp);
    saves.backUpNow(QStringLiteral("linux-hades"));
    QVERIFY(saves.working());
    // One at a time: a second press while it runs does nothing.
    saves.backUpNow(QStringLiteral("linux-hades"));
    saves.backUpAll();
    QVERIFY(done.wait(10000));
    QCOMPARE(done.size(), 1);
    QCOMPARE(done.first().at(0).toString(), QStringLiteral("linux-hades"));
    QCOMPARE(done.first().at(1).toString(), QStringLiteral("Backed up 1 file, 4 bytes."));
    QVERIFY(saves.message().isEmpty());
    QVERIFY(!saves.working());
    QCOMPARE(working.size(), 2);
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-hades")).size(), 1);
}

void GameSavesTest::backUpAllCountsTheGames()
{
    start();
    GameSaves saves(setup());

    QCOMPARE(backedUpAll(saves), QStringLiteral("No played games with saves to back up."));

    // One of two fails: that is what the user hears, though the other was written.
    Input blocked = game(QStringLiteral("linux-hades"), QStringLiteral("Hades"));
    blocked.options.folder = m_dir->filePath(QStringLiteral("a-file"));
    writeFile(blocked.options.folder, "in the way");
    writeSave(QStringLiteral("Celeste"));
    writeSave(QStringLiteral("Hades"));
    m_played = {celeste(), blocked};
    QCOMPARE(backedUpAll(saves), QStringLiteral("1 game couldn't be backed up."));
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")).size(), 1);
    QVERIFY(!saves.working());

    Input capped = game(QStringLiteral("linux-hades"), QStringLiteral("Hades"));
    capped.options.maxBytes = 2;
    m_played = {capped};
    QCOMPARE(backedUpAll(saves), QStringLiteral("1 game couldn't be backed up."));

    Input hades = game(QStringLiteral("linux-hades"), QStringLiteral("Hades"));
    hades.now = at(60);
    Input celesteLater = celeste();
    celesteLater.now = at(60);
    writeSave(QStringLiteral("Celeste"), "changed");
    m_played = {celesteLater, hades};
    QCOMPARE(backedUpAll(saves), QStringLiteral("Backed up the saves of 2 games."));
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")).size(), 2);
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-hades")).size(), 1);

    QCOMPARE(backedUpAll(saves), QStringLiteral("The saves haven't changed since the last backup."));

    m_played = {game(QStringLiteral("linux-nobody"), QStringLiteral("Nobody Listed"))};
    QCOMPARE(backedUpAll(saves), QStringLiteral("No saves found to back up."));
}

void GameSavesTest::searchLooksInTheManifestOnceItIsRead()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    GameSaves saves(setup());

    QVERIFY(saves.search(QStringLiteral("celeste")).isEmpty()); // nothing read yet
    loaded(saves, QStringLiteral("linux-celeste"));
    QCOMPARE(saves.search(QStringLiteral("celeste")),
             (QVariantList{QStringLiteral("Celeste"), QStringLiteral("Celeste Classic")}));
    QCOMPARE(saves.search(QStringLiteral("  CLASSIC   céleste ")), QVariantList{QStringLiteral("Celeste Classic")});
    QVERIFY(saves.search(QStringLiteral("   ")).isEmpty());
    QVERIFY(saves.search(QStringLiteral("nothing like it")).isEmpty());
    QCOMPARE(saves.search(QStringLiteral("puzzle")).size(), 30);
}

void GameSavesTest::chooseWritesTheChoiceAndReloads()
{
    start();
    m_inputs.insert(QStringLiteral("linux-celeste"), celeste());
    GameSaves saves(setup());
    loaded(saves, QStringLiteral("linux-celeste"));

    m_chooseError = QStringLiteral("Couldn't save the game file");
    QSignalSpy changed(&saves, &GameSaves::stateChanged);
    QCOMPARE(saves.choose(QStringLiteral("linux-celeste"), QStringLiteral("Celeste Classic")), m_chooseError);
    QVERIFY(m_inputs.value(QStringLiteral("linux-celeste")).copies.first().savesGame.isEmpty());
    QVERIFY(!changed.wait(200));
    QCOMPARE(saves.state().value(QStringLiteral("match")).toString(), QStringLiteral("title"));

    m_chooseError.clear();
    QVERIFY(saves.choose(QStringLiteral("linux-celeste"), QStringLiteral("Celeste Classic")).isEmpty());
    QVERIFY(changed.wait(5000));
    QCOMPARE(saves.state().value(QStringLiteral("match")).toString(), QStringLiteral("chosen"));
    QCOMPARE(saves.state().value(QStringLiteral("matched")).toString(), QStringLiteral("Celeste Classic"));
}

// Archive names are the local time: when the clocks go back, a backup made in the repeated hour sorts before one
// made in the first pass of it.
void GameSavesTest::theNewestBackupStaysNewestWhenTheClockGoesBack()
{
    start();
    const QTimeZone berlin(QByteArrayLiteral("Europe/Berlin"));
    if (!berlin.isValid())
        QSKIP("no time zone data for Europe/Berlin");
    // 02:45 summer time, then 25 minutes later, after the clocks went back at 03:00: 02:10 winter time.
    const QDateTime before =
        QDateTime(QDate(2026, 10, 25), QTime(0, 45), QTimeZone::UTC).toTimeZone(berlin);
    const QDateTime after = before.addSecs(25 * 60);
    QCOMPARE(before.toString(QStringLiteral("HH:mm")), QStringLiteral("02:45"));
    QCOMPARE(after.toString(QStringLiteral("HH:mm")), QStringLiteral("02:10"));

    Input in = celeste();
    in.options.keep = 1;
    in.now = before;
    writeSave(QStringLiteral("Celeste"), "first");
    const SavesBackup::Result first = SavesBackup::run(in);
    QCOMPARE(first.status, SavesBackup::Status::Written);
    in.now = after;
    writeSave(QStringLiteral("Celeste"), "second, made later");
    const SavesBackup::Result second = SavesBackup::run(in);
    QCOMPARE(second.status, SavesBackup::Status::Written);

    QEXPECT_FAIL("", "S3.5: the backup made after the clocks went back sorts as the older one", Continue);
    QCOMPARE(SavesBackup::backups(m_folder, QStringLiteral("linux-celeste")), QStringList{second.archive});
    QEXPECT_FAIL("", "S3.5: keep = 1 trashes the new backup and keeps the stale one", Continue);
    QCOMPARE(m_trashed, QStringList{first.archive});
}

// The emulated-game branch of SavesBackup::inputFor, through the library as the app wires it.
void GameSavesTest::anEmulatedGameGetsItsEmulatorsSavePlaces()
{
    start();
    auto library = openLibrary();
    const QString id = QStringLiteral("3ds-fire-emblem-awakening");
    QVERIFY(library->game(id));

    // Azahar has none of its save places yet: nothing to back up.
    QVERIFY(!SavesBackup::inputFor(*library, id, m_data, QStringLiteral("1.2.3"), true));

    const QString sdmc = m_home + QStringLiteral("/.var/app/org.azahar_emu.Azahar/data/azahar-emu/sdmc");
    writeFile(sdmc + QStringLiteral("/save.bin"), "3ds");
    // Automatic backup is off for it, so a session's end makes none; the page's Back up now ignores that.
    QVERIFY(!SavesBackup::inputFor(*library, id, m_data, QStringLiteral("1.2.3")));
    const std::optional<Input> in = SavesBackup::inputFor(*library, id, m_data, QStringLiteral("1.2.3"), true);
    QVERIFY(in);
    QCOMPARE(in->emulatorId, QStringLiteral("azahar"));
    QVERIFY(!in->ownSaves);
    QCOMPARE(in->gameId, QStringLiteral("emulator-azahar"));
    QCOMPARE(SavesBackup::backupKey(*in), QStringLiteral("emulator-azahar"));
    QCOMPARE(in->title, QStringLiteral("Azahar"));
    QCOMPARE(in->emulatorPaths, QStringList{sdmc}); // the states folder isn't there
    QVERIFY(in->copies.isEmpty());
    QCOMPARE(in->options.folder, m_home + QStringLiteral("/.local/share/lattis-saves"));

    GameSaves saves(librarySetup(*library));
    const QVariantMap state = loaded(saves, id);
    QCOMPARE(state.value(QStringLiteral("emulator")).toString(), QStringLiteral("Azahar"));
    QCOMPARE(pathsOf(state), QStringList{QStringLiteral("~/.var/app/org.azahar_emu.Azahar/data/azahar-emu/sdmc")});
    QCOMPARE(state.value(QStringLiteral("folder")).toString(), QStringLiteral("~/.local/share/lattis-saves/emulator-azahar"));
}

void GameSavesTest::anEmulatedGameWithItsOwnSavePathsIsBackedUpAlone()
{
    start();
    auto library = openLibrary();
    const QString id = QStringLiteral("3ds-fire-emblem-awakening");
    const QString sdmc = m_home + QStringLiteral("/.var/app/org.azahar_emu.Azahar/data/azahar-emu/sdmc");
    writeFile(sdmc + QStringLiteral("/save.bin"), "3ds");
    writeFile(m_dir->filePath(QStringLiteral("outside/save.bin")), "not in the home");

    // Paths a restore wouldn't take back (outside the home) don't count: the emulator's places stand.
    QVERIFY(library->saveGame(id, {{QStringLiteral("save_paths"),
                                    QStringList{QStringLiteral("~/../outside"), m_dir->filePath(QStringLiteral("outside"))}}})
                .isEmpty());
    std::optional<Input> in = SavesBackup::inputFor(*library, id, m_data, QStringLiteral("1.2.3"), true);
    QVERIFY(in);
    QVERIFY(!in->ownSaves);
    QCOMPARE(in->emulatorPaths, QStringList{sdmc});

    writeFile(m_home + QStringLiteral("/fe-saves/chapter.sav"), "awakening");
    QVERIFY(library->saveGame(id, {{QStringLiteral("save_paths"),
                                    QStringList{QStringLiteral("~/fe-saves"), QStringLiteral("~/missing"),
                                                QStringLiteral("~/../outside")}}})
                .isEmpty());
    in = SavesBackup::inputFor(*library, id, m_data, QStringLiteral("1.2.3"), true);
    QVERIFY(in);
    QVERIFY(in->ownSaves);
    QCOMPARE(in->gameId, id);
    QCOMPARE(SavesBackup::backupKey(*in), id);
    QCOMPARE(in->title, QStringLiteral("Fire Emblem Awakening"));
    QCOMPARE(in->emulatorPaths, QStringList{m_home + QStringLiteral("/fe-saves")});

    // Back up now writes the game's own backup, with its own files and not the emulator's.
    GameSaves saves(librarySetup(*library));
    saves.open(id);
    QCOMPARE(backedUp(saves, id), QStringLiteral("Backed up 1 file, 9 bytes."));
    const QString folder = m_home + QStringLiteral("/.local/share/lattis-saves");
    QCOMPARE(SavesBackup::backups(folder, id).size(), 1);
    QVERIFY(SavesBackup::backups(folder, QStringLiteral("emulator-azahar")).isEmpty());
    QTRY_COMPARE(pathsOf(saves.state()), QStringList{QStringLiteral("~/fe-saves")});
}

void GameSavesTest::savesBesideTheRomComeWithTheirEmulator()
{
    start();
    const QString roms = m_home + QStringLiteral("/roms/nds");
    const QString away = m_dir->filePath(QStringLiteral("roms-elsewhere"));
    writeFile(roms + QStringLiteral("/Puzzle Quest.nds"), "rom");
    writeFile(roms + QStringLiteral("/Puzzle Quest.sav"), "save");
    writeFile(roms + QStringLiteral("/Puzzle Quest.ml1"), "state");
    writeFile(roms + QStringLiteral("/Puzzle Quest.txt"), "notes");
    writeFile(away + QStringLiteral("/Far Away.nds"), "rom");
    writeFile(away + QStringLiteral("/Far Away.sav"), "outside the home");
    addMelonDs({roms, away});
    auto library = openLibrary();

    QString id;
    for (const Game &game : library->allGames()) {
        if (game.platform == QLatin1String("nds") && game.title.contains(QLatin1String("Puzzle")))
            id = game.id;
    }
    QVERIFY2(!id.isEmpty(), "the DS ROM wasn't scanned");
    QCOMPARE(library->installs().value(QStringLiteral("melonds")).kind, EmulatorInstall::Kind::Flatpak);

    const std::optional<Input> in = SavesBackup::inputFor(*library, id, m_data, QStringLiteral("1.2.3"), true);
    QVERIFY(in);
    QCOMPARE(in->emulatorId, QStringLiteral("melonds"));
    QVERIFY(!in->ownSaves);
    QCOMPARE(in->gameId, QStringLiteral("emulator-melonds"));
    QStringList paths = in->emulatorPaths;
    paths.sort();
    QCOMPARE(paths, (QStringList{roms + QStringLiteral("/Puzzle Quest.ml1"), roms + QStringLiteral("/Puzzle Quest.sav")}));

    GameSaves saves(librarySetup(*library));
    saves.open(id);
    const QVariantMap state = loaded(saves, id);
    QCOMPARE(state.value(QStringLiteral("emulator")).toString(), QStringLiteral("melonDS"));
    QVERIFY(pathsOf(state).contains(QStringLiteral("~/roms/nds/Puzzle Quest.sav")));
    QCOMPARE(backedUp(saves, id), QStringLiteral("Backed up 2 files, 9 bytes."));
    QCOMPARE(SavesBackup::backups(m_home + QStringLiteral("/.local/share/lattis-saves"), QStringLiteral("emulator-melonds")).size(), 1);
    QCOMPARE(readFile(away + QStringLiteral("/Far Away.sav")), QByteArray("outside the home"));
}

QTEST_MAIN(GameSavesTest)
#include "gamesaves_test.moc"
