#include "backupplan.h"
#include "libraryfiles.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using BackupPlan::EmulatorPart;

namespace {

const QString kCompatdata = QStringLiteral(STEAM_DATA_DIR "/steamapps/compatdata");
const quint32 kCaveStory = 2900000001u; // the fake Steam's prefix
const QDateTime kCreated(QDate(2026, 9, 28), QTime(21, 4), QTimeZone::fromSecondsAheadOfUtc(-3 * 3600));

void writeFile(const QString &path, qint64 size = 0)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(size, 'x'));
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Every file under `folder`, relative, sorted.
QStringList files(const QString &folder)
{
    QStringList out;
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
        out.append(QDir(folder).relativeFilePath(it.next()));
    out.sort();
    return out;
}

Game game(const QString &id, const QString &platform)
{
    Game g;
    g.id = id;
    g.title = id;
    g.platform = platform;
    return g;
}

EmulatorDef emulator(const QString &id, const QMap<QString, QStringList> &saves = {},
                     const QMap<QString, QStringList> &keys = {})
{
    EmulatorDef def;
    def.id = id;
    def.name = id.at(0).toUpper() + id.mid(1);
    def.saves = saves;
    def.keys = keys;
    return def;
}

Platform platform(const QString &id, const QStringList &emulators, const QStringList &romFolders = {})
{
    Platform p;
    p.id = id;
    p.emulators = emulators;
    p.romFolders = romFolders;
    return p;
}

} // namespace

class BackupPlanTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void settingsDefaults();
    void settingsParsed();
    void settingsWarnings();
    void saveSetting();

    void libraryExclusions();
    void libraryRomFolders();
    void libraryCredentials();
    void libraryArchive();
    void libraryManifest();

    void configuredEmulators();
    void emulatorPlaces();
    void emulatorSizes();
    void besideRomsOnce();
    void keysChoice();
    void protonGames();
    void protonChoice();
    void savesManifest();

private:
    QString path(const QString &relative) const { return m_dir->filePath(relative); }
    BackupPlan::Sources librarySources() const;
    BackupPlan::Sources savesSources() const;

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_data;
    QString m_home;
};

void BackupPlanTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_data = path(QStringLiteral("data"));
    m_home = path(QStringLiteral("home"));
    QVERIFY(QDir().mkpath(m_data));
    QVERIFY(QDir().mkpath(m_home));
}

// ---- [backup] ----

void BackupPlanTest::settingsDefaults()
{
    const Parsed<BackupPlan::Settings> parsed = BackupPlan::parseSettings("[list]\nsort = \"title\"\n", "settings.toml");
    QVERIFY(parsed.ok);
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value, BackupPlan::Settings());
    QVERIFY(parsed.value.credentials && parsed.value.keys && parsed.value.proton);
}

void BackupPlanTest::settingsParsed()
{
    const Parsed<BackupPlan::Settings> parsed =
        BackupPlan::parseSettings("[backup]\ncredentials = false\nkeys = false\nproton = false\n", "settings.toml");
    QVERIFY(parsed.warnings.isEmpty());
    QVERIFY(!parsed.value.credentials);
    QVERIFY(!parsed.value.keys);
    QVERIFY(!parsed.value.proton);
}

void BackupPlanTest::settingsWarnings()
{
    Parsed<BackupPlan::Settings> parsed =
        BackupPlan::parseSettings("[backup]\nkeys = \"no\"\nprotn = false\ncredentials = false\n", "settings.toml");
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.warnings, QStringList({QStringLiteral("settings.toml:2: keys must be true or false"),
                                           QStringLiteral("settings.toml:3: unknown key protn — did you mean \"proton\"?")}));
    QVERIFY(parsed.value.keys);
    QVERIFY(parsed.value.proton);
    QVERIFY(!parsed.value.credentials);

    parsed = BackupPlan::parseSettings("backup = 1\n", "settings.toml");
    QCOMPARE(parsed.warnings, QStringList(QStringLiteral("settings.toml:1: backup must be a [backup] section")));

    parsed = BackupPlan::parseSettings("[backup\n", "settings.toml");
    QVERIFY(!parsed.ok);
    QCOMPARE(parsed.warnings.size(), 1);
    QVERIFY(parsed.warnings.first().startsWith(QLatin1String("settings.toml:1: ")));
}

void BackupPlanTest::saveSetting()
{
    const QString file = path(QStringLiteral("settings.toml"));
    // The default leaves a missing file missing.
    QVERIFY(BackupPlan::saveSetting(file, QStringLiteral("keys"), true));
    QVERIFY(!QFile::exists(file));

    QVERIFY(BackupPlan::saveSetting(file, QStringLiteral("keys"), false));
    QVERIFY(BackupPlan::saveSetting(file, QStringLiteral("proton"), false));
    QCOMPARE(BackupPlan::parseSettings(readFile(file), "settings.toml").value,
             (BackupPlan::Settings{true, false, false}));
    QVERIFY(readFile(file).contains("[backup]\nkeys = false\nproton = false\n"));

    // Back to the default: the key goes.
    QVERIFY(BackupPlan::saveSetting(file, QStringLiteral("keys"), true));
    QVERIFY(!readFile(file).contains("keys"));
    QCOMPARE(BackupPlan::parseSettings(readFile(file), "settings.toml").value,
             (BackupPlan::Settings{true, true, false}));
}

// ---- The library backup ----

BackupPlan::Sources BackupPlanTest::librarySources() const
{
    // A data dir with each kind of file, 1 byte each unless said.
    const QStringList kept = {
        QStringLiteral("library/library.toml"), QStringLiteral("library/platforms.toml"),
        QStringLiteral("library/emulators.toml"), QStringLiteral("library/games/gba-a.toml"),
        QStringLiteral("library/games/gba-b.toml"), QStringLiteral("library/roms/gba.toml"),
        QStringLiteral("library/backup-v0/library.toml"), QStringLiteral("themes/mine.toml"),
        QStringLiteral("settings.toml"), QStringLiteral("art/gba-a/cover.png"),
    };
    for (const QString &file : kept)
        writeFile(m_data + u'/' + file, 1);
    writeFile(m_data + QStringLiteral("/credentials.toml"), 10);
    writeFile(m_data + QStringLiteral("/library/emulators.reference.toml"), 100);
    writeFile(m_data + QStringLiteral("/themes/colors/reference.toml"), 100);
    writeFile(m_data + QStringLiteral("/themes/styles/reference.toml"), 100);
    writeFile(m_data + QStringLiteral("/roms/gba/A.gba"), 1000);
    writeFile(m_data + QStringLiteral("/roms/gba/sub/B.gba"), 1000);
    writeFile(m_data + QStringLiteral("/loose/C.gba"), 500); // a game file's `rom`
    writeFile(m_home + QStringLiteral("/Games/D.gba"), 1000); // outside the data dir

    BackupPlan::Sources s;
    s.dataDir = m_data;
    s.home = m_home;
    s.library = {1, QStringLiteral("ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362")};
    s.platforms = {platform(QStringLiteral("gba"), {QStringLiteral("mgba")},
                            {QStringLiteral("roms/gba"), QStringLiteral("roms/gba/sub"), QStringLiteral("~/Games"),
                             QStringLiteral("roms/missing")})};
    Game a = game(QStringLiteral("gba-a"), QStringLiteral("gba"));
    a.rom = true;
    a.romPath = m_data + QStringLiteral("/roms/gba/A.gba");
    Game c = game(QStringLiteral("gba-c"), QStringLiteral("gba"));
    c.rom = true;
    c.romKey = true;
    c.romPath = m_data + QStringLiteral("/loose/C.gba");
    Game d = game(QStringLiteral("gba-d"), QStringLiteral("gba"));
    d.rom = true;
    d.romPath = m_home + QStringLiteral("/Games/D.gba");
    s.games = {a, c, d};
    return s;
}

void BackupPlanTest::libraryExclusions()
{
    BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(librarySources(), {});
    QCOMPARE(plan.games, 3);
    QCOMPARE(plan.excluded(), QStringList({QStringLiteral("library/emulators.reference.toml"),
                                           QStringLiteral("loose/C.gba"), QStringLiteral("roms/gba"),
                                           QStringLiteral("roms/missing"), QStringLiteral("themes/colors/reference.toml"),
                                           QStringLiteral("themes/styles/reference.toml")}));
    const Archive::Skip skip = plan.skip();
    QVERIFY(skip(QStringLiteral("roms/gba")));
    QVERIFY(skip(QStringLiteral("themes/colors/reference.toml")));
    QVERIFY(skip(QStringLiteral("themes/styles/reference.toml")));
    QVERIFY(!skip(QStringLiteral("themes/mine.toml")));
    QVERIFY(!skip(QStringLiteral("credentials.toml")));
    QVERIFY(!skip(QStringLiteral("library/backup-v0")));

    QCOMPARE(plan.bytes(), -1);
    BackupPlan::measure(plan);
    QCOMPARE(plan.dataBytes, 10); // the ten 1-byte files
    QCOMPARE(plan.credentialsFile.bytes, 10);
    QCOMPARE(plan.bytes(), 20);
}

void BackupPlanTest::libraryRomFolders()
{
    BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(librarySources(), {});
    BackupPlan::measure(plan);
    // roms/gba/sub is inside roms/gba; ~/Games is outside the data dir, untouched.
    QCOMPARE(plan.roms.size(), 3);
    QCOMPARE(plan.roms.at(0), (BackupPlan::Place{m_data + QStringLiteral("/roms/gba"), true, 2000}));
    QCOMPARE(plan.roms.at(1), (BackupPlan::Place{m_data + QStringLiteral("/roms/missing"), false, 0}));
    QCOMPARE(plan.roms.at(2), (BackupPlan::Place{m_data + QStringLiteral("/loose/C.gba"), true, 500}));

    // A ROM folder that is the data dir: its ROMs are left out one by one.
    BackupPlan::Sources s = librarySources();
    s.platforms = {platform(QStringLiteral("gba"), {}, {QStringLiteral(".")})};
    plan = BackupPlan::planLibrary(s, {});
    QCOMPARE(plan.excluded(), QStringList({QStringLiteral("library/emulators.reference.toml"),
                                           QStringLiteral("loose/C.gba"), QStringLiteral("roms/gba/A.gba"),
                                           QStringLiteral("themes/colors/reference.toml"),
                                           QStringLiteral("themes/styles/reference.toml")}));
}

void BackupPlanTest::libraryCredentials()
{
    BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(librarySources(), {false, true, true});
    QVERIFY(!plan.credentials);
    QVERIFY(plan.excluded().contains(QStringLiteral("credentials.toml")));
    BackupPlan::measure(plan);
    QCOMPARE(plan.bytes(), 10);
    // Chosen after measuring.
    plan.credentials = true;
    QCOMPARE(plan.bytes(), 20);
    QVERIFY(!plan.excluded().contains(QStringLiteral("credentials.toml")));

    // No credentials.toml: nothing to take.
    const BackupPlan::Sources sources = librarySources();
    QVERIFY(QFile::remove(m_data + QStringLiteral("/credentials.toml")));
    plan = BackupPlan::planLibrary(sources, {});
    QVERIFY(!plan.credentialsFile.exists);
    QVERIFY(BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1")).contains("credentials = false\n"));
}

void BackupPlanTest::libraryArchive()
{
    for (const bool credentials : {true, false}) {
        const BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(librarySources(), {credentials, true, true});
        const QString archive = path(QStringLiteral("library-%1.tar.zst").arg(credentials));
        Archive::Writer writer;
        QVERIFY(writer.open(archive));
        QVERIFY(writer.addData(QStringLiteral("manifest.toml"), BackupPlan::manifest(plan, kCreated, "0.1")));
        QVERIFY(writer.addTree(plan.dataDir, QStringLiteral("data"), plan.skip()));
        QVERIFY(writer.finish());

        const QString out = path(QStringLiteral("out-%1").arg(credentials));
        QString error;
        QVERIFY2(Archive::extract(archive, {{QStringLiteral("data"), out}}, {QStringLiteral("manifest.toml")}, &error),
                 qPrintable(error));
        QStringList expected = {
            QStringLiteral("art/gba-a/cover.png"), QStringLiteral("library/backup-v0/library.toml"),
            QStringLiteral("library/emulators.toml"), QStringLiteral("library/games/gba-a.toml"),
            QStringLiteral("library/games/gba-b.toml"), QStringLiteral("library/library.toml"),
            QStringLiteral("library/platforms.toml"), QStringLiteral("library/roms/gba.toml"),
            QStringLiteral("settings.toml"), QStringLiteral("themes/mine.toml"),
        };
        if (credentials)
            expected.append(QStringLiteral("credentials.toml"));
        expected.sort();
        QCOMPARE(files(out), expected);
    }
}

void BackupPlanTest::libraryManifest()
{
    BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(librarySources(), {});
    BackupPlan::measure(plan);
    const QByteArray expected =
        "# A Lattis library backup: games, art, themes and settings. Restore it with\n"
        "# Import in the app, or by hand: tar --zstd -xf <this file>, then copy data/ to\n"
        "# ~/.local/share/lattis/.\n"
        "kind = \"library\"\n"
        "format = 1\n"
        "app_version = \"0.1\"\n"
        "library_version = 1\n"
        "library_id = \"ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362\"\n"
        "created = 2026-09-28T21:04:00-03:00\n"
        "games = 3\n"
        "bytes = 20\n"
        "credentials = true\n";
    QCOMPARE(BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1")), expected);
    QCOMPARE(BackupPlan::fileName(QStringLiteral("library"), kCreated.date()),
             QStringLiteral("lattis-library-2026-09-28.tar.zst"));
}

// ---- The saves backup ----

BackupPlan::Sources BackupPlanTest::savesSources() const
{
    BackupPlan::Sources s;
    s.dataDir = m_data;
    s.home = m_home;
    s.library = {1, QStringLiteral("ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362")};
    s.compatdata = kCompatdata;

    const QString native = QStringLiteral("native");
    const QString flatpak = QStringLiteral("flatpak");
    EmulatorDef ryubing = emulator(
        QStringLiteral("ryubing"),
        {{native, {QStringLiteral("~/.config/Ryujinx/bis/user/save"), QStringLiteral("~/.config/Ryujinx/bis/system/save")}},
         {flatpak, {QStringLiteral("~/.var/app/ryujinx/save")}}},
        {{native, {QStringLiteral("~/.config/Ryujinx/system")}}});
    EmulatorDef dolphin = emulator(QStringLiteral("dolphin"), {{native, {QStringLiteral("~/.local/share/dolphin-emu/GC")}}});
    EmulatorDef mgba = emulator(QStringLiteral("mgba"));
    mgba.savesBesideRom = {QStringLiteral("sav")};
    EmulatorDef bare = emulator(QStringLiteral("bare"), {}, {{native, {QStringLiteral("~/.bare/bios")}}});
    EmulatorDef pinned = emulator(QStringLiteral("pinned"), {{native, {QStringLiteral("~/.pinned/saves.bin")}}});
    EmulatorDef unused = emulator(QStringLiteral("unused"), {{native, {QStringLiteral("~/.unused")}}});
    EmulatorDef otherGba = emulator(QStringLiteral("vba"));
    otherGba.savesBesideRom = {QStringLiteral("sav")};
    s.emulators = {bare, dolphin, mgba, pinned, ryubing, unused, otherGba};

    const auto installed = [&](const QString &program) {
        EmulatorInstall install;
        install.kind = EmulatorInstall::Kind::Path;
        install.program = m_home + program;
        return install;
    };
    EmulatorInstall mgbaInstall;
    mgbaInstall.kind = EmulatorInstall::Kind::Flatpak;
    mgbaInstall.program = QStringLiteral("io.mgba.mGBA");
    s.installs = {{QStringLiteral("ryubing"), installed(QStringLiteral("/bin/ryujinx"))},
                  {QStringLiteral("mgba"), mgbaInstall},
                  {QStringLiteral("bare"), installed(QStringLiteral("/bin/bare"))},
                  {QStringLiteral("pinned"), installed(QStringLiteral("/bin/pinned"))},
                  {QStringLiteral("unused"), installed(QStringLiteral("/bin/unused"))},
                  {QStringLiteral("vba"), installed(QStringLiteral("/bin/vba"))}};

    s.platforms = {platform(QStringLiteral("switch"), {QStringLiteral("ryubing")}),
                   platform(QStringLiteral("gamecube"), {QStringLiteral("dolphin")}),
                   platform(QStringLiteral("gba"), {QStringLiteral("mgba"), QStringLiteral("vba")}),
                   platform(QStringLiteral("ps2"), {QStringLiteral("bare")})};

    // Saves: Ryubing's user saves (3 bytes) and its keys (7), the pinned emulator's file (5),
    // an unused emulator's (100), a ROM's save next to it (2).
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/data"), 3);
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/system/prod.keys"), 7);
    writeFile(m_home + QStringLiteral("/.pinned/saves.bin"), 5);
    writeFile(m_home + QStringLiteral("/.bare/bios"));
    writeFile(m_home + QStringLiteral("/.unused/save"), 100);
    writeFile(m_home + QStringLiteral("/Games/Emerald.gba"), 1000);
    writeFile(m_home + QStringLiteral("/Games/Emerald.sav"), 2);

    Game emerald = game(QStringLiteral("gba-emerald"), QStringLiteral("gba"));
    emerald.rom = true;
    emerald.romPath = m_home + QStringLiteral("/Games/Emerald.gba");
    Game hades = game(QStringLiteral("switch-hades"), QStringLiteral("switch"));
    hades.emulator = QStringLiteral("pinned");
    Game caveStory = game(QStringLiteral("proton-cave-story"), kProtonPlatform);
    caveStory.title = QStringLiteral("Cave Story");
    caveStory.shortcut = kCaveStory;
    Game neverLaunched = game(QStringLiteral("proton-new"), kProtonPlatform);
    neverLaunched.shortcut = 1234u;
    Game uninstalled = game(QStringLiteral("proton-gone"), kProtonPlatform);
    uninstalled.shortcut = 1235u;
    uninstalled.installed = false;
    uninstalled.uninstalled = true;
    s.games = {caveStory, emerald, uninstalled, hades, neverLaunched};
    return s;
}

void BackupPlanTest::configuredEmulators()
{
    const BackupPlan::SavesPlan plan = BackupPlan::planSaves(savesSources(), {});
    QStringList ids;
    for (const EmulatorPart &part : plan.emulators)
        ids.append(part.id);
    // In definition order; "unused" is installed but nothing lists or pins it.
    QCOMPARE(ids, QStringList({QStringLiteral("bare"), QStringLiteral("dolphin"), QStringLiteral("mgba"),
                               QStringLiteral("pinned"), QStringLiteral("ryubing"), QStringLiteral("vba")}));

    const auto part = [&](const QString &id) {
        return *std::find_if(plan.emulators.cbegin(), plan.emulators.cend(),
                             [&](const EmulatorPart &p) { return p.id == id; });
    };
    QCOMPARE(part(QStringLiteral("bare")).skipped, EmulatorPart::Skipped::NoSaves);
    QCOMPARE(part(QStringLiteral("dolphin")).skipped, EmulatorPart::Skipped::NotInstalled);
    QCOMPARE(part(QStringLiteral("dolphin")).describe, QStringLiteral("Not found"));
    QVERIFY(part(QStringLiteral("dolphin")).saves.isEmpty());
    QCOMPARE(part(QStringLiteral("mgba")).skipped, EmulatorPart::Skipped::No); // saves next to ROMs
    QCOMPARE(part(QStringLiteral("mgba")).install, QStringLiteral("flatpak"));
    QCOMPARE(part(QStringLiteral("pinned")).skipped, EmulatorPart::Skipped::No);
    QCOMPARE(part(QStringLiteral("ryubing")).skipped, EmulatorPart::Skipped::No);
    QCOMPARE(part(QStringLiteral("ryubing")).install, QStringLiteral("native"));
    QCOMPARE(part(QStringLiteral("ryubing")).name, QStringLiteral("Ryubing"));
}

void BackupPlanTest::emulatorPlaces()
{
    BackupPlan::Sources s = savesSources();
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(s, {});
    const EmulatorPart ryubing = plan.emulators.at(4);
    QCOMPARE(ryubing.saves,
             QList<BackupPlan::Place>({{m_home + QStringLiteral("/.config/Ryujinx/bis/user/save"), true, -1},
                                       {m_home + QStringLiteral("/.config/Ryujinx/bis/system/save"), false, -1}}));
    QCOMPARE(ryubing.keys,
             QList<BackupPlan::Place>({{m_home + QStringLiteral("/.config/Ryujinx/system"), true, -1}}));
    QCOMPARE(plan.emulators.at(0).keys.size(), 1); // no saves, but its keys are still taken

    // Installed another way: that install type's folders.
    s.installs[QStringLiteral("ryubing")].kind = EmulatorInstall::Kind::Flatpak;
    plan = BackupPlan::planSaves(s, {});
    QCOMPARE(plan.emulators.at(4).install, QStringLiteral("flatpak"));
    QCOMPARE(plan.emulators.at(4).saves,
             QList<BackupPlan::Place>({{m_home + QStringLiteral("/.var/app/ryujinx/save"), false, -1}}));
    QVERIFY(plan.emulators.at(4).keys.isEmpty());
    QVERIFY(!plan.emulators.at(4).takesAnything(true));
}

void BackupPlanTest::emulatorSizes()
{
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(savesSources(), {});
    QCOMPARE(plan.bytes(), -1);
    QCOMPARE(plan.emulators.at(4).bytes(true), -1);
    BackupPlan::measure(plan);
    const EmulatorPart &ryubing = plan.emulators.at(4);
    QCOMPARE(ryubing.saves.at(0).bytes, 3);
    QCOMPARE(ryubing.saves.at(1).bytes, 0);
    QCOMPARE(ryubing.bytes(true), 10);
    QCOMPARE(ryubing.bytes(false), 3);
    QCOMPARE(plan.emulators.at(3).bytes(true), 5); // pinned: a file
    QCOMPARE(plan.emulators.at(2).bytes(true), 2); // mGBA: the save next to the ROM
    QCOMPARE(plan.emulators.at(1).bytes(true), 0); // not installed
    QVERIFY(!plan.emulators.at(1).takesAnything(true));
}

void BackupPlanTest::besideRomsOnce()
{
    BackupPlan::Sources s = savesSources();
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(s, {});
    BackupPlan::measure(plan);
    // mGBA and VBA both run gba games; the first takes Emerald's save.
    QCOMPARE(plan.emulators.at(2).besideRoms.size(), 1);
    QCOMPARE(plan.emulators.at(2).besideRoms.first().gameId, QStringLiteral("gba-emerald"));
    QVERIFY(plan.emulators.at(5).besideRoms.isEmpty());

    // Not installed: saves next to ROMs are still there, so still taken.
    s.installs.remove(QStringLiteral("mgba"));
    plan = BackupPlan::planSaves(s, {});
    BackupPlan::measure(plan);
    QCOMPARE(plan.emulators.at(2).skipped, EmulatorPart::Skipped::NotInstalled);
    QCOMPARE(plan.emulators.at(2).besideRoms.size(), 1);
    QVERIFY(plan.emulators.at(2).takesAnything(true));
}

void BackupPlanTest::keysChoice()
{
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(savesSources(), {true, false, false});
    BackupPlan::measure(plan);
    QVERIFY(!plan.keys);
    QCOMPARE(plan.bytes(), 3 + 5 + 2);
    QByteArray text = BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1"));
    QVERIFY(text.contains("keys = false\n"));
    QVERIFY(!text.contains("Ryujinx/system\"]"));
    QVERIFY(!text.contains("id = \"bare\"")); // only keys to take

    plan.keys = true;
    QCOMPARE(plan.bytes(), 3 + 7 + 5 + 2);
    text = BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1"));
    QVERIFY(text.contains("keys = [\"~/.config/Ryujinx/system\"]\n"));
}

void BackupPlanTest::protonGames()
{
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(savesSources(), {});
    QCOMPARE(plan.protonGames.size(), 3);
    const BackupPlan::ProtonPart &caveStory = plan.protonGames.at(0);
    QCOMPARE(caveStory.gameId, QStringLiteral("proton-cave-story"));
    QCOMPARE(caveStory.saves.skipped, ProtonSaves::Skipped::No);
    QCOMPARE(caveStory.saves.prefix, kCompatdata + QStringLiteral("/2900000001"));
    QCOMPARE(caveStory.proton, QStringLiteral("GE-Proton10-4"));
    QVERIFY(caveStory.saves.files.isEmpty()); // until measured
    QCOMPARE(plan.protonGames.at(1).saves.skipped, ProtonSaves::Skipped::Uninstalled);
    QCOMPARE(plan.protonGames.at(2).saves.skipped, ProtonSaves::Skipped::NeverLaunched);

    BackupPlan::measure(plan);
    QVERIFY(!plan.protonGames.at(0).saves.files.isEmpty());
    QCOMPARE(plan.protonGames.at(0).saves.bytes,
             ProtonSaves::find({plan.games.first()}, kCompatdata).first().bytes);
}

void BackupPlanTest::protonChoice()
{
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(savesSources(), {true, true, false});
    BackupPlan::measure(plan);
    QCOMPARE(plan.bytes(), 3 + 7 + 5 + 2);
    const QByteArray text = BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1"));
    QVERIFY(text.contains("proton = false\n"));
    QVERIFY(!text.contains("[[game]]"));

    plan.proton = true;
    QCOMPARE(plan.bytes(), 3 + 7 + 5 + 2 + plan.protonGames.first().saves.bytes);
}

void BackupPlanTest::savesManifest()
{
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(savesSources(), {});
    BackupPlan::measure(plan);
    const QByteArray expected =
        "# A Lattis saves backup. Restore it with Import in the app, or by hand:\n"
        "# tar --zstd -xf <this file>, then copy each emulators/<id>/saves/<n>/ to the n-th\n"
        "# folder listed below, and each proton/<game>/ over compatdata/<shortcut>/ in Steam.\n"
        "kind = \"saves\"\n"
        "format = 1\n"
        "app_version = \"0.1\"\n"
        "library_id = \"ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362\"   # the library it was made with\n"
        "created = 2026-09-28T21:04:00-03:00\n"
        "keys = true\n"
        "proton = true\n"
        "\n"
        "[[emulator]]\n"
        "id = \"bare\"\n"
        "name = \"Bare\"\n"
        "install = \"native\"\n"
        "keys = [\"~/.bare/bios\"]\n"
        "bytes = 0\n"
        "\n"
        "[[emulator]]\n"
        "id = \"mgba\"\n"
        "name = \"Mgba\"\n"
        "install = \"flatpak\"\n"
        "beside_roms = [\"gba-emerald\"]   # games whose ROMs had saves next to them\n"
        "bytes = 2\n"
        "\n"
        "[[emulator]]\n"
        "id = \"pinned\"\n"
        "name = \"Pinned\"\n"
        "install = \"native\"\n"
        "saves = [\"~/.pinned/saves.bin\"]   # where they came from\n"
        "bytes = 5\n"
        "\n"
        "[[emulator]]\n"
        "id = \"ryubing\"\n"
        "name = \"Ryubing\"\n"
        "install = \"native\"\n"
        "saves = [\"~/.config/Ryujinx/bis/user/save\", \"~/.config/Ryujinx/bis/system/save\"]   # where they came from\n"
        "keys = [\"~/.config/Ryujinx/system\"]\n"
        "bytes = 10\n"
        "\n"
        "[[game]]\n"
        "id = \"proton-cave-story\"\n"
        "title = \"Cave Story\"\n"
        "shortcut = 2900000001   # its prefix's folder when it was backed up\n"
        "proton = \"GE-Proton10-4\"   # the prefix's version file\n"
        "bytes = "
        + QByteArray::number(plan.protonGames.first().saves.bytes) + "\n";
    QCOMPARE(BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1")), expected);
}

QTEST_GUILESS_MAIN(BackupPlanTest)
#include "backupplan_test.moc"
