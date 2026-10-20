#include "restore.h"
#include "backup.h"
#include "backups.h"
#include "libraryfiles.h"
#include "movedaside.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

#include <memory>

using Restore::Part;

namespace {

const QString kFakeCompatdata = QStringLiteral(STEAM_DATA_DIR "/steamapps/compatdata");
const quint32 kCaveStory = 2900000001u; // the fake Steam's prefix
const QDateTime kCreated(QDate(2026, 9, 28), QTime(21, 4), QTimeZone::fromSecondsAheadOfUtc(-3 * 3600));
const QDate kToday(2026, 9, 30);
const QString kAside = QStringLiteral(".before-import-2026-09-30");

void writeFile(const QString &path, const QByteArray &data)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("(missing)");
}

// Copies a folder, keeping links as links.
void copyTree(const QString &from, const QString &to)
{
    QVERIFY(QDir().mkpath(to));
    const QFileInfoList entries =
        QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) {
        const QString target = to + u'/' + entry.fileName();
        if (entry.isSymLink()) {
            QByteArray link(4096, Qt::Uninitialized);
            const ssize_t n = ::readlink(QFile::encodeName(entry.filePath()).constData(), link.data(), link.size());
            QVERIFY(n > 0);
            QCOMPARE(::symlink(link.left(n).constData(), QFile::encodeName(target).constData()), 0);
        } else if (entry.isDir()) {
            copyTree(entry.filePath(), target);
        } else {
            QVERIFY(QFile::copy(entry.filePath(), target));
        }
    }
}

// Every path under `folder` with its size or link target, for "nothing changed".
QStringList snapshot(const QString &folder)
{
    QStringList out;
    QDirIterator it(folder, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        const QString name = QDir(folder).relativeFilePath(info.filePath());
        if (info.isSymLink())
            out.append(name + QStringLiteral(" -> ") + info.symLinkTarget());
        else if (info.isDir())
            out.append(name + u'/');
        else
            out.append(name + u' ' + QString::fromLatin1(readFile(info.filePath()).toHex()));
    }
    out.sort();
    return out;
}

// A snapshot() entry's path.
QString entryPath(const QString &entry)
{
    if (entry.contains(QStringLiteral(" -> ")))
        return entry.section(QStringLiteral(" -> "), 0, 0);
    if (entry.endsWith(u'/'))
        return entry.chopped(1);
    return entry.left(entry.lastIndexOf(u' '));
}

Game game(const QString &id, const QString &platform)
{
    Game g;
    g.id = id;
    g.title = id;
    g.platform = platform;
    return g;
}

EmulatorInstall native(const QString &program)
{
    EmulatorInstall install;
    install.kind = EmulatorInstall::Kind::Path;
    install.program = program;
    return install;
}

EmulatorInstall flatpak(const QString &id)
{
    EmulatorInstall install;
    install.kind = EmulatorInstall::Kind::Flatpak;
    install.program = id;
    return install;
}

const Part *findPart(const Restore::Plan &plan, Part::Kind kind, const QString &id = {})
{
    for (const Part &part : plan.parts) {
        if (part.kind == kind && part.id == id)
            return &part;
    }
    return nullptr;
}

void checkOnly(Restore::Plan &plan, Part::Kind kind)
{
    for (Part &part : plan.parts)
        part.checked = part.kind == kind && part.restorable();
}

} // namespace

class RestoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void refusals();
    void manifest();
    void library();
    void libraryKeepsCredentials();
    void libraryOnNewMachine();
    void savesPartAlone_data();
    void savesPartAlone();
    void allParts();
    void movedAsideFoundAndDeleted();
    void flatpakToNative();
    void emulatorNotInstalledNow();
    void fewerFoldersNow();
    void besideMovedRom();
    void protonOverNewerPrefix();
    void protonChangedShortcut();
    void cantBeRestored();
    void importTwiceTheSameDay();
    void failureWhileStaging();
    void failureAfterStaging();
    void failureWhileMoving();
    void runningGame();
    void libraryImportRestarts();
    void savesImportDoesntRestart();
    void refusedImportGoesOn();
    void libraryLinksKept();
    void importDialogSaves();
    void importDialogLibrary();
    void importDialogRefuses();
    void importDialogRuns();
    void backupDialogs();

private:
    // Backups as main.cpp sets it up, noting the hold and restart calls.
    Backups::Setup backupsSetup(QStringList *calls, const QStringList &running = {});
    QString path(const QString &relative) const { return m_dir->filePath(relative); }
    BackupPlan::Sources sources() const;
    QString backUpLibrary(const BackupPlan::Settings &settings = {});
    QString backUpSaves();
    // What restoring the archive would do now.
    Restore::Plan planFor(const QString &archive, const BackupPlan::Sources &now);
    // The saves as they are after a play session: each changed.
    void playAgain();

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_data;
    QString m_home;
    QString m_compatdata;
    QString m_prefix;
    QString m_ryubingInstall = QStringLiteral("native");
};

void RestoreTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_data = path(QStringLiteral("data"));
    m_home = path(QStringLiteral("home"));
    m_compatdata = path(QStringLiteral("steam/steamapps/compatdata"));
    m_prefix = m_compatdata + QStringLiteral("/2900000001");
    m_ryubingInstall = QStringLiteral("native");

    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 1\n");
    writeFile(m_data + QStringLiteral("/library/games/gba-emerald.toml"), "title = \"Emerald\"\n");
    writeFile(m_data + QStringLiteral("/settings.toml"), "# backed up\n");
    writeFile(m_data + QStringLiteral("/credentials.toml"), "igdb = \"backed up\"\n");
    writeFile(m_data + QStringLiteral("/roms/gba/Kept.gba"), "rom");

    writeFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin"), "user save");
    QVERIFY(QFile::link(QStringLiteral("0/data.bin"), m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/latest")));
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/bis/system/save/8000/data.bin"), "system save");
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/system/prod.keys"), "keys");
    writeFile(m_home + QStringLiteral("/.local/share/Cemu/keys.txt"), "cemu keys");
    writeFile(m_home + QStringLiteral("/Games/Emerald.gba"), "rom");
    writeFile(m_home + QStringLiteral("/Games/Emerald.sav"), "emerald save");
    writeFile(m_home + QStringLiteral("/Games/Emerald.ss0"), "emerald state");

    copyTree(kFakeCompatdata + QStringLiteral("/2900000001"), m_prefix);
}

BackupPlan::Sources RestoreTest::sources() const
{
    BackupPlan::Sources s;
    s.dataDir = m_data;
    s.home = m_home;
    s.library = {1, QStringLiteral("ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362")};
    s.compatdata = m_compatdata;

    EmulatorDef ryubing;
    ryubing.id = QStringLiteral("ryubing");
    ryubing.name = QStringLiteral("Ryubing");
    ryubing.saves = {{QStringLiteral("native"), {QStringLiteral("~/.config/Ryujinx/bis/user/save"),
                                                  QStringLiteral("~/.config/Ryujinx/bis/system/save")}},
                     {QStringLiteral("flatpak"),
                      {QStringLiteral("~/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/bis/user/save"),
                       QStringLiteral("~/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/bis/system/save")}}};
    ryubing.keys = {{QStringLiteral("native"), {QStringLiteral("~/.config/Ryujinx/system")}},
                    {QStringLiteral("flatpak"), {QStringLiteral("~/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/system")}}};
    EmulatorDef cemu;
    cemu.id = QStringLiteral("cemu");
    cemu.name = QStringLiteral("Cemu");
    cemu.saves = {{QStringLiteral("native"), {QStringLiteral("~/.local/share/Cemu/mlc01/usr/save")}}};
    cemu.keys = {{QStringLiteral("native"), {QStringLiteral("~/.local/share/Cemu/keys.txt")}}};
    EmulatorDef mgba;
    mgba.id = QStringLiteral("mgba");
    mgba.name = QStringLiteral("mGBA");
    mgba.savesBesideRom = {QStringLiteral("sav"), QStringLiteral("ss0")};
    s.emulators = {cemu, mgba, ryubing};
    s.installs = {{QStringLiteral("ryubing"), m_ryubingInstall == QLatin1String("flatpak")
                                                  ? flatpak(QStringLiteral("io.github.ryubing.Ryujinx"))
                                                  : native(m_home + QStringLiteral("/bin/ryujinx"))},
                  {QStringLiteral("cemu"), native(m_home + QStringLiteral("/bin/cemu"))},
                  {QStringLiteral("mgba"), native(m_home + QStringLiteral("/bin/mgba"))}};

    Platform sw;
    sw.id = QStringLiteral("switch");
    sw.emulators = {QStringLiteral("ryubing")};
    Platform wiiu;
    wiiu.id = QStringLiteral("wiiu");
    wiiu.emulators = {QStringLiteral("cemu")};
    Platform gba;
    gba.id = QStringLiteral("gba");
    gba.emulators = {QStringLiteral("mgba")};
    gba.romFolders = {QStringLiteral("roms/gba")};
    s.platforms = {sw, wiiu, gba};

    Game emerald = game(QStringLiteral("gba-emerald"), QStringLiteral("gba"));
    emerald.title = QStringLiteral("Emerald");
    emerald.rom = true;
    emerald.romPath = m_home + QStringLiteral("/Games/Emerald.gba");
    Game caveStory = game(QStringLiteral("proton-cave-story"), kProtonPlatform);
    caveStory.title = QStringLiteral("Cave Story");
    caveStory.shortcut = kCaveStory;
    s.games = {caveStory, emerald};
    return s;
}

QString RestoreTest::backUpLibrary(const BackupPlan::Settings &settings)
{
    const QString archive = path(QStringLiteral("library.tar.zst"));
    const QString error =
        Backup::write(BackupPlan::planLibrary(sources(), settings), archive, kCreated, QStringLiteral("0.1"));
    if (!error.isEmpty())
        qWarning() << error;
    return archive;
}

QString RestoreTest::backUpSaves()
{
    const QString archive = path(QStringLiteral("saves.tar.zst"));
    const QString error = Backup::write(BackupPlan::planSaves(sources(), {}), archive, kCreated, QStringLiteral("0.1"));
    if (!error.isEmpty())
        qWarning() << error;
    return archive;
}

Restore::Plan RestoreTest::planFor(const QString &archive, const BackupPlan::Sources &now)
{
    const Restore::Opened opened = Restore::open(archive);
    if (!opened.manifest) {
        qWarning() << opened.error;
        return {};
    }
    return Restore::plan(archive, *opened.manifest, now);
}

void RestoreTest::playAgain()
{
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin"), "played");
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0002/new.bin"), "new game");
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/system/prod.keys"), "other keys");
    writeFile(m_home + QStringLiteral("/.local/share/Cemu/keys.txt"), "other cemu keys");
    writeFile(m_home + QStringLiteral("/Games/Emerald.sav"), "played");
    writeFile(m_prefix + QStringLiteral("/pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat"), "played");
}

void RestoreTest::refusals()
{
    // Not an archive at all.
    const QString text = path(QStringLiteral("notes.tar.zst"));
    writeFile(text, "just some text");
    Restore::Opened opened = Restore::open(text);
    QVERIFY(!opened.manifest);
    QVERIFY2(opened.error.startsWith(text + QStringLiteral(" isn't a backup")), qPrintable(opened.error));

    // An archive that doesn't start with a manifest.
    const QString other = path(QStringLiteral("other.tar.zst"));
    {
        Archive::Writer writer;
        QVERIFY(writer.open(other));
        QVERIFY(writer.addData(QStringLiteral("readme.txt"), "hello"));
        QVERIFY(writer.finish());
    }
    opened = Restore::open(other);
    QVERIFY(!opened.manifest);
    QVERIFY2(opened.error.contains(QStringLiteral("doesn't start with manifest.toml")), qPrintable(opened.error));

    const auto withManifest = [&](const QByteArray &manifest) {
        const QString file = path(QStringLiteral("made.tar.zst"));
        Archive::Writer writer;
        writer.open(file);
        writer.addData(QStringLiteral("manifest.toml"), manifest);
        writer.finish();
        return Restore::open(file);
    };
    opened = withManifest("kind = \"photos\"\nformat = 1\n");
    QCOMPARE(opened.error, QStringLiteral("made.tar.zst isn't a Lattis backup."));
    opened = withManifest("kind = \"saves\"\n");
    QCOMPARE(opened.error, QStringLiteral("made.tar.zst isn't a Lattis backup."));
    opened = withManifest("kind = \"saves\nformat = 1\n");
    QCOMPARE(opened.error, QStringLiteral("made.tar.zst isn't a Lattis backup."));
    opened = withManifest("kind = \"saves\"\nformat = 2\napp_version = \"0.3\"\n");
    QVERIFY(!opened.manifest);
    QCOMPARE(opened.error,
             QStringLiteral("made.tar.zst was made by a newer version of Lattis (0.3). Update the app to import it."));

    // A newer library imports, with a warning.
    opened = withManifest("kind = \"library\"\nformat = 1\napp_version = \"0.2\"\nlibrary_version = 2\n");
    QVERIFY(opened.manifest);
    QCOMPARE(opened.error, QString());
    QCOMPARE(opened.warning, QStringLiteral("Its library is from a newer version of Lattis (0.2): it opens "
                                            "read-only until you update the app."));
    opened = withManifest("kind = \"library\"\nformat = 1\nlibrary_version = 1\n");
    QVERIFY(opened.manifest);
    QCOMPARE(opened.warning, QString());
}

void RestoreTest::manifest()
{
    Restore::Opened opened = Restore::open(backUpLibrary());
    QVERIFY(opened.manifest);
    const Restore::Manifest library = *opened.manifest;
    QCOMPARE(library.kind, Backup::kLibrary);
    QCOMPARE(library.format, 1);
    QCOMPARE(library.appVersion, QStringLiteral("0.1"));
    QCOMPARE(library.libraryVersion, 1);
    QCOMPARE(library.libraryId, QStringLiteral("ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362"));
    QCOMPARE(library.created, kCreated);
    QCOMPARE(library.games, 2);
    QVERIFY(library.bytes > 0);
    QVERIFY(library.credentials);

    opened = Restore::open(backUpSaves());
    QVERIFY(opened.manifest);
    const Restore::Manifest saves = *opened.manifest;
    QCOMPARE(saves.kind, Backup::kSaves);
    QVERIFY(saves.keys);
    QVERIFY(saves.proton);
    QCOMPARE(saves.emulators.size(), 3);
    const Restore::Manifest::Emulator &cemu = saves.emulators.at(0);
    QCOMPARE(cemu.id, QStringLiteral("cemu"));
    QCOMPARE(cemu.keys, QStringList({QStringLiteral("~/.local/share/Cemu/keys.txt")}));
    const Restore::Manifest::Emulator &mgba = saves.emulators.at(1);
    QCOMPARE(mgba.besideRoms, QStringList({QStringLiteral("gba-emerald")}));
    const Restore::Manifest::Emulator &ryubing = saves.emulators.at(2);
    QCOMPARE(ryubing.name, QStringLiteral("Ryubing"));
    QCOMPARE(ryubing.install, QStringLiteral("native"));
    QCOMPARE(ryubing.saves, QStringList({QStringLiteral("~/.config/Ryujinx/bis/user/save"),
                                         QStringLiteral("~/.config/Ryujinx/bis/system/save")}));
    QVERIFY(ryubing.bytes > 0);
    QCOMPARE(saves.protonGames.size(), 1);
    QCOMPARE(saves.protonGames.first().id, QStringLiteral("proton-cave-story"));
    QCOMPARE(saves.protonGames.first().title, QStringLiteral("Cave Story"));
    QCOMPARE(saves.protonGames.first().shortcut, kCaveStory);
}

void RestoreTest::library()
{
    const QString archive = backUpLibrary();
    // Edited since: a setting changed, a game added.
    writeFile(m_data + QStringLiteral("/settings.toml"), "# changed\n");
    writeFile(m_data + QStringLiteral("/library/games/gba-ruby.toml"), "title = \"Ruby\"\n");

    Restore::Plan plan = planFor(archive, sources());
    QCOMPARE(plan.parts.size(), 1);
    const Part &part = plan.parts.first();
    QCOMPARE(part.kind, Part::Kind::Library);
    QVERIFY(part.checked);
    QCOMPARE(part.targets.first().path, m_data);
    QVERIFY(part.targets.first().exists);
    QVERIFY(plan.sameLibrary);
    QCOMPARE(plan.games, 2);
    QCOMPARE(plan.carriedOver, QStringList({QStringLiteral("roms/gba")}));

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.restored, 1);
    QCOMPARE(result.movedAside, QStringList({m_data + kAside}));

    QCOMPARE(readFile(m_data + QStringLiteral("/settings.toml")), "# backed up\n");
    QVERIFY(!QFile::exists(m_data + QStringLiteral("/library/games/gba-ruby.toml")));
    QCOMPARE(readFile(m_data + QStringLiteral("/credentials.toml")), "igdb = \"backed up\"\n");
    // The ROMs kept in the data dir, which no backup holds, went along.
    QCOMPARE(readFile(m_data + QStringLiteral("/roms/gba/Kept.gba")), "rom");
    // What was there is moved aside, whole but for them.
    QCOMPARE(readFile(m_data + kAside + QStringLiteral("/settings.toml")), "# changed\n");
    QVERIFY(QFile::exists(m_data + kAside + QStringLiteral("/library/games/gba-ruby.toml")));
    QVERIFY(!QFile::exists(m_data + kAside + QStringLiteral("/roms/gba")));
    QCOMPARE(QDir(path(QString())).entryList(QDir::Dirs | QDir::NoDotAndDotDot).filter(QStringLiteral("import-")),
             QStringList({QStringLiteral("data") + kAside}));
}

void RestoreTest::libraryKeepsCredentials()
{
    // Backed up without the API keys; the keys set since stay.
    const QString archive = backUpLibrary({false, true, true});
    writeFile(m_data + QStringLiteral("/credentials.toml"), "igdb = \"new\"\n");
    QVERIFY(QFile::setPermissions(m_data + QStringLiteral("/credentials.toml"), QFile::ReadOwner | QFile::WriteOwner));
    Restore::Plan plan = planFor(archive, sources());
    QCOMPARE(plan.carriedOver, QStringList({QStringLiteral("roms/gba"), QStringLiteral("credentials.toml")}));

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(readFile(m_data + QStringLiteral("/credentials.toml")), "igdb = \"new\"\n");
    QCOMPARE(QFileInfo(m_data + QStringLiteral("/credentials.toml")).permissions() & 0x0077, QFile::Permissions());
    // Copied: what's moved aside is whole.
    QCOMPARE(readFile(m_data + kAside + QStringLiteral("/credentials.toml")), "igdb = \"new\"\n");
}

void RestoreTest::libraryOnNewMachine()
{
    // A new machine: no data dir at all.
    const QString archive = backUpLibrary();
    Archive::removeTree(m_data);
    BackupPlan::Sources now = sources();
    now.games.clear();
    now.platforms.clear();
    Restore::Plan plan = planFor(archive, now);
    QVERIFY(!plan.parts.first().targets.first().exists);
    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(result.movedAside.isEmpty());
    QCOMPARE(readFile(m_data + QStringLiteral("/settings.toml")), "# backed up\n");
    QVERIFY(!QFile::exists(m_data + QStringLiteral("/roms/gba")));
}

void RestoreTest::savesPartAlone_data()
{
    QTest::addColumn<Part::Kind>("kind");
    QTest::newRow("saves") << Part::Kind::Saves;
    QTest::newRow("keys") << Part::Kind::Keys;
    QTest::newRow("beside ROMs") << Part::Kind::BesideRoms;
    QTest::newRow("Proton") << Part::Kind::Proton;
}

void RestoreTest::savesPartAlone()
{
    QFETCH(Part::Kind, kind);
    const QString archive = backUpSaves();
    playAgain();
    const QStringList before = snapshot(path(QString()));

    Restore::Plan plan = planFor(archive, sources());
    checkOnly(plan, kind);
    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));

    const QString user = m_home + QStringLiteral("/.config/Ryujinx/bis/user/save");
    const QString keys = m_home + QStringLiteral("/.config/Ryujinx/system");
    const QString cemuKeys = m_home + QStringLiteral("/.local/share/Cemu/keys.txt");
    const QString sav = m_home + QStringLiteral("/Games/Emerald.sav");
    const QString profile = QStringLiteral("/pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat");
    const QByteArray backedUpProfile = readFile(kFakeCompatdata + QStringLiteral("/2900000001") + profile);

    QCOMPARE(readFile(user + QStringLiteral("/0001/0/data.bin")), kind == Part::Kind::Saves ? "user save" : "played");
    QCOMPARE(readFile(keys + QStringLiteral("/prod.keys")), kind == Part::Kind::Keys ? "keys" : "other keys");
    QCOMPARE(readFile(cemuKeys), kind == Part::Kind::Keys ? "cemu keys" : "other cemu keys");
    QCOMPARE(readFile(sav), kind == Part::Kind::BesideRoms ? "emerald save" : "played");
    QCOMPARE(readFile(m_prefix + profile), kind == Part::Kind::Proton ? backedUpProfile : QByteArray("played"));

    QStringList aside;
    switch (kind) {
    case Part::Kind::Saves:
        // The system saves, unchanged, are replaced too: the part is restored whole.
        aside = {user + kAside, m_home + QStringLiteral("/.config/Ryujinx/bis/system/save") + kAside};
        QCOMPARE(readFile(user + kAside + QStringLiteral("/0001/0/data.bin")), "played");
        QVERIFY(!QFile::exists(user + QStringLiteral("/0002")));
        QCOMPARE(QFileInfo(user + QStringLiteral("/0001/latest")).symLinkTarget(), user + QStringLiteral("/0001/0/data.bin"));
        break;
    case Part::Kind::Keys:
        aside = {cemuKeys + kAside, keys + kAside};
        QCOMPARE(readFile(cemuKeys + kAside), "other cemu keys");
        QVERIFY(QFileInfo(cemuKeys).isFile());
        break;
    case Part::Kind::BesideRoms:
        aside = {sav + kAside, m_home + QStringLiteral("/Games/Emerald.ss0") + kAside};
        QCOMPARE(readFile(sav + kAside), "played");
        QCOMPARE(QDir(m_home + QStringLiteral("/Games")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), 5);
        break;
    case Part::Kind::Proton:
        aside = {m_compatdata + kAside + QStringLiteral("/2900000001")};
        QCOMPARE(readFile(aside.first() + profile), "played");
        break;
    default:
        break;
    }
    QStringList moved = result.movedAside;
    moved.sort();
    aside.sort();
    QCOMPARE(moved, aside);

    // Nothing else changed, and no staging folder is left.
    QStringList roots = aside;
    for (const QString &a : std::as_const(aside))
        roots.append(a.chopped(kAside.size()));
    if (kind == Part::Kind::Proton)
        roots = {m_prefix, m_compatdata + kAside};
    const auto untouched = [&](const QStringList &list) {
        QStringList out;
        for (const QString &entry : list) {
            const QString full = path(entryPath(entry));
            if (std::none_of(roots.cbegin(), roots.cend(),
                             [&](const QString &r) { return full == r || full.startsWith(r + u'/'); }))
                out.append(entry);
        }
        return out;
    };
    const QStringList after = snapshot(path(QString()));
    QCOMPARE(untouched(after), untouched(before));
    QVERIFY(!after.join(u'\n').contains(QStringLiteral(".import-")));
}

void RestoreTest::allParts()
{
    const QString archive = backUpSaves();
    playAgain();
    Restore::Plan plan = planFor(archive, sources());
    // Cemu's saves (not found, so not in the backup) and keys, mGBA's saves next to ROMs,
    // Ryubing's saves and keys, Cave Story.
    QCOMPARE(plan.parts.size(), 6);
    for (const Part &part : std::as_const(plan.parts))
        QVERIFY2(part.checked, qPrintable(part.name));
    QVERIFY(plan.bytes() > 0);

    qint64 progress = 0;
    const Restore::Result result = Restore::apply(plan, kToday, {[&](qint64 bytes) { progress = bytes; }, nullptr});
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(progress > 0);
    // Cemu's keys file, both Ryubing save folders and its keys, Emerald's save and state, the prefix.
    QCOMPARE(result.movedAside.size(), 7);
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin")), "user save");
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/system/save/8000/data.bin")), "system save");
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/system/prod.keys")), "keys");
    QCOMPARE(readFile(m_home + QStringLiteral("/.local/share/Cemu/keys.txt")), "cemu keys");
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.sav")), "emerald save");
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.ss0")), "emerald state");
    QVERIFY(!QFile::exists(m_compatdata + QStringLiteral(".import-2026-09-30")));
}

void RestoreTest::movedAsideFoundAndDeleted()
{
    const QString archive = backUpSaves();
    playAgain();
    Restore::Plan plan = planFor(archive, sources());
    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));

    // Remembered as the app does, and found again (after a restart, say).
    const QString file = MovedAside::file(path(QStringLiteral("state")), QStringLiteral("key"));
    QVERIFY(MovedAside::record(file, {Backup::kSaves, archive, QDateTime::currentDateTime(), result.movedAside, true}));
    QList<MovedAside::Import> found = MovedAside::present(MovedAside::read(file));
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().paths, result.movedAside);
    QVERIFY(MovedAside::bytes(found.first().paths) > 0);

    // Deleted: into a scratch trash, never the real one.
    const QString trash = path(QStringLiteral("trash"));
    QVERIFY(QDir().mkpath(trash));
    int n = 0;
    const QString error = MovedAside::remove(found.first().paths, [&](const QString &from, QString *why) {
        if (QDir().rename(from, trash + u'/' + QString::number(n++)))
            return true;
        *why = QStringLiteral("rename failed");
        return false;
    });
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(n, 7);
    for (const QString &aside : std::as_const(result.movedAside))
        QVERIFY2(!QFileInfo::exists(aside), qPrintable(aside));
    QVERIFY(!QFileInfo::exists(m_compatdata + kAside));
    QVERIFY(MovedAside::present(MovedAside::read(file)).isEmpty());
    // What was restored stays.
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/system/prod.keys")), "keys");
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.sav")), "emerald save");
    QVERIFY(QFileInfo(m_prefix).isDir());
}

void RestoreTest::flatpakToNative()
{
    // Backed up from the Flatpak, restored into the native install's folders.
    m_ryubingInstall = QStringLiteral("flatpak");
    const QString flatpakSaves = m_home + QStringLiteral("/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/bis/user/save");
    writeFile(flatpakSaves + QStringLiteral("/0001/0/data.bin"), "flatpak save");
    const QString archive = backUpSaves();
    m_ryubingInstall = QStringLiteral("native");

    Restore::Plan plan = planFor(archive, sources());
    const Part *saves = findPart(plan, Part::Kind::Saves, QStringLiteral("ryubing"));
    QVERIFY(saves);
    QCOMPARE(saves->backedUp, QStringLiteral("flatpak"));
    QCOMPARE(saves->install, QStringLiteral("native"));
    QCOMPARE(saves->targets.first().label, QStringLiteral("~/.config/Ryujinx/bis/user/save"));
    QVERIFY(saves->targets.first().problem.isEmpty());
    checkOnly(plan, Part::Kind::Saves);

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin")), "flatpak save");
    // The Flatpak had no system saves: that folder is left as it is.
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/system/save/8000/data.bin")), "system save");
    QCOMPARE(result.movedAside,
             QStringList({m_home + QStringLiteral("/.config/Ryujinx/bis/user/save") + kAside}));
}

void RestoreTest::emulatorNotInstalledNow()
{
    const QString archive = backUpSaves();
    playAgain();
    BackupPlan::Sources now = sources();
    now.installs.remove(QStringLiteral("ryubing"));
    now.installs.remove(QStringLiteral("mgba")); // not needed for its saves next to ROMs
    Restore::Plan plan = planFor(archive, now);

    const Part *saves = findPart(plan, Part::Kind::Saves, QStringLiteral("ryubing"));
    QVERIFY(saves);
    QCOMPARE(saves->problem, QStringLiteral("Install Ryubing first; its saves stay in the backup"));
    QVERIFY(!saves->checked);
    QCOMPARE(saves->targets.first().label, QStringLiteral("~/.config/Ryujinx/bis/user/save"));
    QCOMPARE(findPart(plan, Part::Kind::Keys, QStringLiteral("ryubing"))->problem,
             QStringLiteral("Install Ryubing first; its keys stay in the backup"));
    QVERIFY(findPart(plan, Part::Kind::BesideRoms, QStringLiteral("mgba"))->checked);

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin")), "played");
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.sav")), "emerald save");

    // Checked anyway, it's left out.
    for (Part &part : plan.parts)
        part.checked = true;
    QVERIFY2(Restore::apply(plan, kToday).error.isEmpty(), "");
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin")), "played");
}

void RestoreTest::fewerFoldersNow()
{
    const QString archive = backUpSaves();
    playAgain();
    BackupPlan::Sources now = sources();
    for (EmulatorDef &def : now.emulators) {
        if (def.id == QLatin1String("ryubing"))
            def.saves[QStringLiteral("native")].removeLast();
    }
    Restore::Plan plan = planFor(archive, now);
    const Part *saves = findPart(plan, Part::Kind::Saves, QStringLiteral("ryubing"));
    QVERIFY(saves);
    QVERIFY(saves->checked);
    QCOMPARE(saves->targets.size(), 2);
    QVERIFY(saves->targets.at(0).problem.isEmpty());
    QCOMPARE(saves->targets.at(1).problem, QStringLiteral("Ryubing lists fewer save folders now"));
    QCOMPARE(saves->targets.at(1).label, QStringLiteral("~/.config/Ryujinx/bis/system/save"));
    checkOnly(plan, Part::Kind::Saves);

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.restored, 1);
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin")), "user save");
    QVERIFY(!QFile::exists(m_home + QStringLiteral("/.config/Ryujinx/bis/system/save") + kAside));
}

void RestoreTest::besideMovedRom()
{
    const QString archive = backUpSaves();
    // The ROM moved and was renamed since; its old saves are left where they are.
    writeFile(m_home + QStringLiteral("/Roms/GBA/Pokemon Emerald.gba"), "rom");
    BackupPlan::Sources now = sources();
    for (Game &g : now.games) {
        if (g.id == QLatin1String("gba-emerald"))
            g.romPath = m_home + QStringLiteral("/Roms/GBA/Pokemon Emerald.gba");
    }
    Restore::Plan plan = planFor(archive, now);
    checkOnly(plan, Part::Kind::BesideRoms);
    const Part *part = findPart(plan, Part::Kind::BesideRoms, QStringLiteral("mgba"));
    QCOMPARE(part->targets.first().label, QStringLiteral("Emerald"));

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(result.movedAside.isEmpty());
    QCOMPARE(readFile(m_home + QStringLiteral("/Roms/GBA/Pokemon Emerald.sav")), "emerald save");
    QCOMPARE(readFile(m_home + QStringLiteral("/Roms/GBA/Pokemon Emerald.ss0")), "emerald state");
    QCOMPARE(QDir(m_home + QStringLiteral("/Roms/GBA")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).size(), 3);
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.sav")), "emerald save");
}

void RestoreTest::protonOverNewerPrefix()
{
    const QString archive = backUpSaves();
    playAgain();
    // Since the backup, a newer Proton upgraded the prefix: it tracks a new file.
    writeFile(m_prefix + QStringLiteral("/pfx/drive_c/windows/system32/newer.dll"), "newer proton");
    {
        QFile tracked(m_prefix + QStringLiteral("/tracked_files"));
        QVERIFY(tracked.open(QIODevice::Append));
        tracked.write("drive_c/windows/system32/newer.dll\n");
    }
    writeFile(m_prefix + QStringLiteral("/version"), "GE-Proton12-1\n");
    const QByteArray tracked = readFile(m_prefix + QStringLiteral("/tracked_files"));

    Restore::Plan plan = planFor(archive, sources());
    checkOnly(plan, Part::Kind::Proton);
    const Part *part = findPart(plan, Part::Kind::Proton, QStringLiteral("proton-cave-story"));
    QVERIFY(part);
    QCOMPARE(part->name, QStringLiteral("Cave Story"));
    QCOMPARE(part->targets.first().path, m_prefix);

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    // Proton's files are the current prefix's, the saves and version the backup's.
    QCOMPARE(readFile(m_prefix + QStringLiteral("/pfx/drive_c/windows/system32/newer.dll")), "newer proton");
    QCOMPARE(readFile(m_prefix + QStringLiteral("/tracked_files")), tracked);
    QCOMPARE(readFile(m_prefix + QStringLiteral("/version")), readFile(kFakeCompatdata + QStringLiteral("/2900000001/version")));
    QCOMPARE(readFile(m_prefix + QStringLiteral("/pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat")),
             readFile(kFakeCompatdata + QStringLiteral("/2900000001/pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat")));

    // Everything the fake prefix had is back, but what the backup drops (the lock, old registry).
    const auto withoutDropped = [](QStringList list) {
        list.removeIf([](const QString &e) {
            return e.startsWith(QStringLiteral("pfx.lock")) || e.startsWith(QStringLiteral("pfx/user.reg.old"))
                   || e.startsWith(QStringLiteral("pfx/drive_c/users/steamuser/AppData/Local/Temp"));
        });
        return list;
    };
    QStringList restored = snapshot(m_prefix);
    restored.removeIf([](const QString &e) {
        return e.startsWith(QStringLiteral("pfx/drive_c/windows/system32/newer.dll")) || e.startsWith(QStringLiteral("tracked_files"));
    });
    QStringList original = withoutDropped(snapshot(kFakeCompatdata + QStringLiteral("/2900000001")));
    original.removeIf([](const QString &e) { return e.startsWith(QStringLiteral("tracked_files")); });
    // Links by name: relative targets resolve differently in the two places.
    const auto names = [](QStringList list) {
        for (QString &e : list)
            e = e.section(QStringLiteral(" -> "), 0, 0);
        return list;
    };
    QCOMPARE(names(restored), names(original));

    // The one before is outside compatdata/, whole.
    const QString aside = m_compatdata + kAside + QStringLiteral("/2900000001");
    QCOMPARE(result.movedAside, QStringList({aside}));
    QCOMPARE(readFile(aside + QStringLiteral("/version")), "GE-Proton12-1\n");
    QVERIFY(QFile::exists(aside + QStringLiteral("/pfx.lock")));
    QCOMPARE(QDir(m_compatdata).entryList(QDir::AllEntries | QDir::NoDotAndDotDot), QStringList({QStringLiteral("2900000001")}));
    QVERIFY(!QFile::exists(m_compatdata + QStringLiteral(".import-2026-09-30")));
}

void RestoreTest::protonChangedShortcut()
{
    const QString archive = backUpSaves();
    playAgain();
    // Its shortcut was made again: a new app id, and a prefix Proton made for it.
    const QString newPrefix = m_compatdata + QStringLiteral("/2900000002");
    QVERIFY(QDir().rename(m_prefix, newPrefix));
    BackupPlan::Sources now = sources();
    for (Game &g : now.games) {
        if (g.id == QLatin1String("proton-cave-story"))
            g.shortcut = 2900000002u;
    }
    Restore::Plan plan = planFor(archive, now);
    checkOnly(plan, Part::Kind::Proton);
    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.movedAside, QStringList({m_compatdata + kAside + QStringLiteral("/2900000002")}));
    QCOMPARE(readFile(newPrefix + QStringLiteral("/pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat")),
             readFile(kFakeCompatdata + QStringLiteral("/2900000001/pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat")));
    QVERIFY(!QFile::exists(m_prefix));
}

void RestoreTest::cantBeRestored()
{
    const QString archive = backUpSaves();

    // A new machine where Cave Story wasn't launched yet, and Emerald isn't in the library.
    Archive::removeTree(m_prefix);
    BackupPlan::Sources now = sources();
    now.games.removeIf([](const Game &g) { return g.id == QLatin1String("gba-emerald"); });
    Restore::Plan plan = planFor(archive, now);
    const Part *proton = findPart(plan, Part::Kind::Proton, QStringLiteral("proton-cave-story"));
    QCOMPARE(proton->problem, QStringLiteral("Launch Cave Story once first; its saves stay in the backup"));
    QVERIFY(!proton->checked);
    const Part *beside = findPart(plan, Part::Kind::BesideRoms, QStringLiteral("mgba"));
    QCOMPARE(beside->targets.first().problem, QStringLiteral("Not in your library; restore the library first"));
    QCOMPARE(beside->targets.first().label, QStringLiteral("gba-emerald"));
    QVERIFY(!beside->checked);

    // Not in the library at all, or its ROM gone.
    now.games.clear();
    plan = planFor(archive, now);
    QCOMPARE(findPart(plan, Part::Kind::Proton, QStringLiteral("proton-cave-story"))->problem,
             QStringLiteral("Not in your library; restore the library first"));
    now = sources();
    QVERIFY(QFile::remove(m_home + QStringLiteral("/Games/Emerald.gba")));
    plan = planFor(archive, now);
    QCOMPARE(findPart(plan, Part::Kind::BesideRoms, QStringLiteral("mgba"))->targets.first().problem,
             QStringLiteral("Its ROM isn't found"));

    // A Proton that doesn't list its files.
    copyTree(kFakeCompatdata + QStringLiteral("/2900000001"), m_prefix);
    QVERIFY(QFile::remove(m_prefix + QStringLiteral("/tracked_files")));
    plan = planFor(archive, now);
    QCOMPARE(findPart(plan, Part::Kind::Proton, QStringLiteral("proton-cave-story"))->problem,
             QStringLiteral("Its Proton doesn't list its own files"));

    // An emulator no longer defined.
    now.emulators.removeIf([](const EmulatorDef &d) { return d.id == QLatin1String("cemu"); });
    plan = planFor(archive, now);
    QCOMPARE(findPart(plan, Part::Kind::Keys, QStringLiteral("cemu"))->problem,
             QStringLiteral("Cemu isn't set up in the launcher now"));
}

void RestoreTest::importTwiceTheSameDay()
{
    const QString archive = backUpSaves();
    Restore::Plan plan = planFor(archive, sources());
    checkOnly(plan, Part::Kind::Keys);
    QCOMPARE(Restore::apply(plan, kToday).error, QString());
    const Restore::Result again = Restore::apply(planFor(archive, sources()), kToday);
    QVERIFY2(again.error.isEmpty(), qPrintable(again.error));
    QVERIFY(again.movedAside.contains(m_home + QStringLiteral("/.local/share/Cemu/keys.txt.before-import-2026-09-30-2")));
    QVERIFY(QFile::exists(m_home + QStringLiteral("/.local/share/Cemu/keys.txt") + kAside));
}

void RestoreTest::failureWhileStaging()
{
    // Ryubing's keys can't be staged: nothing changes, the parts staged before it included.
    const QString archive = backUpSaves();
    playAgain();
    const QString config = m_home + QStringLiteral("/.config/Ryujinx");
    const QStringList before = snapshot(path(QString()));
    QVERIFY(QFile::setPermissions(config, QFile::ReadOwner | QFile::ExeOwner));
    const Restore::Result result = Restore::apply(planFor(archive, sources()), kToday);
    QFile::setPermissions(config, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.movedAside.isEmpty());
    QCOMPARE(snapshot(path(QString())), before);
}

void RestoreTest::failureAfterStaging()
{
    // Everything is staged, then the prefix's Proton files can't be read.
    const QString archive = backUpSaves();
    playAgain();
    Restore::Plan plan = planFor(archive, sources());
    QVERIFY(QFile::remove(m_prefix + QStringLiteral("/tracked_files")));
    const QStringList before = snapshot(path(QString()));
    const Restore::Result result = Restore::apply(plan, kToday);
    QCOMPARE(result.error, m_prefix + QStringLiteral(" doesn't list Proton's files now"));
    QCOMPARE(snapshot(path(QString())), before);
}

void RestoreTest::failureWhileMoving()
{
    // A ROM folder in the data dir can't be moved into the restored one: what was moved
    // already goes back.
    const QString archive = backUpLibrary();
    writeFile(m_data + QStringLiteral("/zroms/snes/Kept.sfc"), "rom");
    BackupPlan::Sources now = sources();
    Platform snes;
    snes.id = QStringLiteral("snes");
    snes.romFolders = {QStringLiteral("zroms/snes")};
    now.platforms.append(snes);
    writeFile(m_data + QStringLiteral("/settings.toml"), "# changed\n");
    Restore::Plan plan = planFor(archive, now);
    QCOMPARE(plan.carriedOver, QStringList({QStringLiteral("roms/gba"), QStringLiteral("zroms/snes")}));

    const QStringList before = snapshot(path(QString()));
    QVERIFY(QFile::setPermissions(m_data + QStringLiteral("/zroms"), QFile::ReadOwner | QFile::ExeOwner));
    const Restore::Result result = Restore::apply(plan, kToday);
    QFile::setPermissions(m_data + QStringLiteral("/zroms"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QVERIFY(result.error.startsWith(QStringLiteral("Can't move ") + m_data + QStringLiteral("/zroms/snes")));
    QCOMPARE(snapshot(path(QString())), before);
}

Backups::Setup RestoreTest::backupsSetup(QStringList *calls, const QStringList &running)
{
    return {m_data,
            path(QStringLiteral("state")),
            QStringLiteral("key"),
            [this] { return sources(); },
            [running] { return running; },
            [calls](bool on) { calls->append(on ? QStringLiteral("hold") : QStringLiteral("go on")); },
            [calls] { calls->append(QStringLiteral("restart")); }};
}

void RestoreTest::libraryImportRestarts()
{
    const QString archive = backUpLibrary();
    writeFile(m_data + QStringLiteral("/settings.toml"), "# changed\n");
    QStringList calls;
    {
        Backups backups(backupsSetup(&calls));
        QSignalSpy finished(&backups, &Backups::importFinished);
        QCOMPARE(backups.startImport(planFor(archive, sources())), QString());
        QCOMPARE(calls, QStringList{QStringLiteral("hold")});
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), Backup::kLibrary);
        QCOMPARE(finished.first().at(1).toString(), QString());
        QCOMPARE(calls, (QStringList{QStringLiteral("hold"), QStringLiteral("restart")}));
    }

    // The restarted copy finds the notice and what was moved aside. Backups names that folder
    // with today's date, not kAside's.
    Backups restarted(backupsSetup(&calls));
    const QVariantMap notice = restarted.notice();
    const QString aside = QStringLiteral(".before-import-") + QDate::currentDate().toString(Qt::ISODate);
    QVERIFY2(notice.value(QStringLiteral("text")).toString().startsWith(
                 QStringLiteral("Restored the library from %1. What you had is in data%2")
                     .arg(QFileInfo(archive).fileName(), aside)),
             qPrintable(notice.value(QStringLiteral("text")).toString()));
    QCOMPARE(notice.value(QStringLiteral("folder")).toUrl(), QUrl::fromLocalFile(m_data + aside));
    QCOMPARE(restarted.movedAside().size(), 1);
    QCOMPARE(readFile(m_data + aside + QStringLiteral("/settings.toml")), "# changed\n");

    // Hidden: still listed until deleted.
    restarted.dismissNotice();
    QVERIFY(restarted.notice().isEmpty());
    QCOMPARE(restarted.movedAside().size(), 1);
    QVERIFY(Backups(backupsSetup(&calls)).notice().isEmpty());
}

void RestoreTest::savesImportDoesntRestart()
{
    const QString archive = backUpSaves();
    playAgain();
    QStringList calls;
    Backups backups(backupsSetup(&calls));
    QSignalSpy finished(&backups, &Backups::importFinished);
    QSignalSpy changed(&backups, &Backups::importsChanged);
    QCOMPARE(backups.startImport(planFor(archive, sources())), QString());
    QVERIFY(finished.wait());
    QCOMPARE(finished.first().at(1).toString(), QString());
    QVERIFY(calls.isEmpty());
    QVERIFY(changed.count() > 0);
    const QString text = backups.notice().value(QStringLiteral("text")).toString();
    QVERIFY2(text.contains(QStringLiteral("What you had is moved aside: 4 folders and 3 files")), qPrintable(text));
    const QVariantList aside = backups.movedAside();
    QCOMPARE(aside.size(), 1);
    QCOMPARE(aside.first().toMap().value(QStringLiteral("more")).toInt(), 4);
}

void RestoreTest::refusedImportGoesOn()
{
    const QString archive = backUpLibrary();
    QStringList calls;
    Backups backups(backupsSetup(&calls, {QStringLiteral("Hades")}));
    QVERIFY(!backups.startImport(planFor(archive, sources())).isEmpty());
    QCOMPARE(calls, (QStringList{QStringLiteral("hold"), QStringLiteral("go on")}));
    QVERIFY(backups.notice().isEmpty());
}

void RestoreTest::runningGame()
{
    const QString archive = backUpSaves();
    playAgain();
    QStringList running = {QStringLiteral("Hades")};
    RestoreJob job([&] { return running; });
    const Restore::Plan plan = planFor(archive, sources());
    QCOMPARE(job.start(plan), QStringLiteral("Close Hades first: it may be writing its saves."));
    QVERIFY(!job.running());
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.sav")), "played");

    running.clear();
    QSignalSpy finished(&job, &RestoreJob::finished);
    QCOMPARE(job.start(plan), QString());
    QVERIFY(job.running());
    QCOMPARE(job.total(), plan.bytes());
    QCOMPARE(job.start(plan), QStringLiteral("An import is running already."));
    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toString(), QString());
    QCOMPARE(job.result().movedAside.size(), 7);
    QCOMPARE(readFile(m_home + QStringLiteral("/Games/Emerald.sav")), "emerald save");
}

namespace {

// The importPlan part of `kind` for `id`.
QVariantMap importPart(const QVariantMap &plan, const QString &kind, const QString &id)
{
    for (const QVariant &v : plan.value(QStringLiteral("parts")).toList()) {
        const QVariantMap part = v.toMap();
        if (part.value(QStringLiteral("kind")) == kind && part.value(QStringLiteral("id")) == id)
            return part;
    }
    return {};
}

QVariantMap firstTarget(const QVariantMap &part)
{
    return part.value(QStringLiteral("targets")).toList().value(0).toMap();
}

} // namespace

void RestoreTest::libraryLinksKept()
{
    // The user's links out of the data dir: art on another drive, a folder linked elsewhere.
    const QString elsewhere = path(QStringLiteral("elsewhere"));
    QVERIFY(QDir().mkpath(elsewhere + QStringLiteral("/snes")));
    QVERIFY(QFile::link(elsewhere, m_data + QStringLiteral("/art")));
    QVERIFY(QFile::link(elsewhere, m_data + QStringLiteral("/zroms")));
    const QString archive = backUpLibrary();

    // Since: zroms became a real folder with ROMs in it, which Import carries over.
    QVERIFY(QFile::remove(m_data + QStringLiteral("/zroms")));
    writeFile(m_data + QStringLiteral("/zroms/snes/Kept.sfc"), "rom");
    BackupPlan::Sources now = sources();
    Platform snes;
    snes.id = QStringLiteral("snes");
    snes.romFolders = {QStringLiteral("zroms/snes")};
    now.platforms.append(snes);
    const Restore::Plan plan = planFor(archive, now);
    QVERIFY(plan.carriedOver.contains(QStringLiteral("zroms/snes")));

    const Restore::Result result = Restore::apply(plan, kToday);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(QFileInfo(m_data + QStringLiteral("/art")).symLinkTarget(), elsewhere);
    QCOMPARE(QFileInfo(m_data + QStringLiteral("/zroms")).symLinkTarget(), elsewhere);
    // Not moved through the restored link: they stay in the moved-aside library.
    QVERIFY(!QFileInfo::exists(elsewhere + QStringLiteral("/snes/Kept.sfc")));
    QCOMPARE(readFile(m_data + kAside + QStringLiteral("/zroms/snes/Kept.sfc")), "rom");
    QCOMPARE(readFile(m_data + QStringLiteral("/roms/gba/Kept.gba")), "rom");
}

void RestoreTest::importDialogSaves()
{
    const QString archive = backUpSaves();
    QStringList calls;
    Backups backups(backupsSetup(&calls));
    QSignalSpy changed(&backups, &Backups::importPlanChanged);
    QCOMPARE(backups.openImport(QUrl::fromLocalFile(archive)), QString());
    QVariantMap plan = backups.importPlan();
    QCOMPARE(plan.value(QStringLiteral("kind")).toString(), Backup::kSaves);
    QCOMPARE(plan.value(QStringLiteral("file")).toString(), QStringLiteral("saves.tar.zst"));
    QVERIFY(plan.value(QStringLiteral("sameLibrary")).toBool());
    QVERIFY(plan.value(QStringLiteral("made")).toString().endsWith(QStringLiteral(", by Lattis 0.1")));

    QVariantMap saves = importPart(plan, QStringLiteral("saves"), QStringLiteral("ryubing"));
    QVERIFY(saves.value(QStringLiteral("checked")).toBool());
    QCOMPARE(saves.value(QStringLiteral("installText")).toString(), QString());
    QCOMPARE(firstTarget(saves).value(QStringLiteral("label")).toString(), QStringLiteral("~/.config/Ryujinx/bis/user/save"));
    QCOMPARE(firstTarget(importPart(plan, QStringLiteral("besideRoms"), QStringLiteral("mgba"))).value(QStringLiteral("label")).toString(),
             QStringLiteral("Emerald"));
    QCOMPARE(importPart(plan, QStringLiteral("proton"), QStringLiteral("proton-cave-story")).value(QStringLiteral("name")).toString(),
             QStringLiteral("Cave Story"));

    // What's there now is measured in the background (a link counts as nothing).
    QTRY_COMPARE(firstTarget(importPart(backups.importPlan(), QStringLiteral("saves"), QStringLiteral("ryubing")))
                     .value(QStringLiteral("bytes")).toLongLong(),
                 qint64(9));

    // Unchecking every part leaves nothing to import.
    for (const QVariant &v : backups.importPlan().value(QStringLiteral("parts")).toList())
        backups.checkImportPart(v.toMap().value(QStringLiteral("index")).toInt(), false);
    plan = backups.importPlan();
    QVERIFY(!plan.value(QStringLiteral("anyChecked")).toBool());
    QCOMPARE(plan.value(QStringLiteral("bytes")).toLongLong(), qint64(0));
    QCOMPARE(backups.startImport(), QStringLiteral("Check what to restore."));

    // Installed another way now: its folders for that install.
    m_ryubingInstall = QStringLiteral("flatpak");
    QCOMPARE(backups.openImport(QUrl::fromLocalFile(archive)), QString());
    saves = importPart(backups.importPlan(), QStringLiteral("saves"), QStringLiteral("ryubing"));
    QCOMPARE(saves.value(QStringLiteral("installText")).toString(),
             QStringLiteral("Backed up from the native install, restored into the Flatpak's folders."));
    QCOMPARE(firstTarget(saves).value(QStringLiteral("label")).toString(),
             QStringLiteral("~/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/bis/user/save"));
    QVERIFY(!firstTarget(saves).value(QStringLiteral("exists")).toBool());
}

void RestoreTest::importDialogLibrary()
{
    const QString archive = backUpLibrary();
    QStringList calls;
    Backups backups(backupsSetup(&calls));
    QCOMPARE(backups.openImport(QUrl::fromLocalFile(archive)), QString());
    const QVariantMap plan = backups.importPlan();
    QCOMPARE(plan.value(QStringLiteral("kind")).toString(), Backup::kLibrary);
    QCOMPARE(plan.value(QStringLiteral("gamesNow")).toInt(), 2);
    QCOMPARE(plan.value(QStringLiteral("carriedOver")).toStringList(), QStringList{QStringLiteral("roms/gba")});
    // The name apply() gives the data dir, the day's second Import taking -2.
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    QCOMPARE(plan.value(QStringLiteral("aside")).toString(), QStringLiteral("data.before-import-") + today);
    QVERIFY(QDir().mkdir(m_data + QStringLiteral(".before-import-") + today));
    QCOMPARE(backups.openImport(QUrl::fromLocalFile(archive)), QString());
    QCOMPARE(backups.importPlan().value(QStringLiteral("aside")).toString(), QStringLiteral("data.before-import-%1-2").arg(today));
    QVERIFY(calls.isEmpty());
}

void RestoreTest::importDialogRefuses()
{
    const QString notABackup = path(QStringLiteral("notes.tar.zst"));
    writeFile(notABackup, "just text");
    QStringList calls;
    Backups backups(backupsSetup(&calls));
    QCOMPARE(backups.openImport(QUrl::fromLocalFile(backUpSaves())), QString());
    QVERIFY(backups.openImport(QUrl::fromLocalFile(notABackup)).startsWith(notABackup + QStringLiteral(" isn't a backup")));
    QVERIFY(backups.importPlan().isEmpty());
    QCOMPARE(backups.startImport(), QStringLiteral("Pick a backup first."));
}

void RestoreTest::importDialogRuns()
{
    const QString archive = backUpSaves();
    playAgain();
    QStringList calls;
    Backups backups(backupsSetup(&calls));
    QSignalSpy finished(&backups, &Backups::importFinished);
    QCOMPARE(backups.openImport(QUrl::fromLocalFile(archive)), QString());
    QCOMPARE(backups.startImport(), QString());
    QVERIFY(backups.importing());
    QVERIFY(finished.wait());
    QCOMPARE(finished.first().at(1).toString(), QString());
    QVERIFY(!backups.importing());
    QVERIFY(backups.importPlan().isEmpty());
    QCOMPARE(readFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin")), "user save");
}

void RestoreTest::backupDialogs()
{
    QStringList calls;
    Backups backups(backupsSetup(&calls));
    QSignalSpy library(&backups, &Backups::libraryPlanChanged);
    QSignalSpy saves(&backups, &Backups::savesPlanChanged);
    const auto emulator = [&](const QString &id) {
        for (const QVariant &e : backups.savesPlan().value("emulators").toList()) {
            if (e.toMap().value("id") == id)
                return e.toMap();
        }
        return QVariantMap();
    };

    // Shown at once, then again with the sizes once they're added up.
    backups.planLibrary();
    backups.planSaves();
    QCOMPARE(library.size(), 1);
    QVERIFY(!backups.libraryPlan().value("measured").toBool());
    QCOMPARE(backups.libraryPlan().value("games").toInt(), 2);
    QCOMPARE(emulator("ryubing").value("savesBytes").toLongLong(), -1);
    QTRY_COMPARE(library.size(), 2);
    QTRY_COMPARE(saves.size(), 2);

    const QVariantMap plan = backups.libraryPlan();
    QVERIFY(plan.value("measured").toBool());
    QCOMPARE(plan.value("credentials").toMap(), (QVariantMap{{"exists", true}, {"bytes", 19}}));
    QCOMPARE(plan.value("roms").toList(), (QVariantList{QVariantMap{{"path", "roms/gba"}, {"bytes", 3}}}));

    // Paths from the fake home, with ~; each emulator's saves, keys and saves beside ROMs.
    QVERIFY(backups.savesPlan().value("measured").toBool());
    const QVariantMap ryubing = emulator("ryubing");
    QCOMPARE(ryubing.value("saves").toStringList(),
             (QStringList{"~/.config/Ryujinx/bis/user/save", "~/.config/Ryujinx/bis/system/save"}));
    QCOMPARE(ryubing.value("savesBytes").toLongLong(), 20);
    QCOMPARE(ryubing.value("keys").toStringList(), QStringList{"~/.config/Ryujinx/system"});
    QCOMPARE(ryubing.value("keysBytes").toLongLong(), 4);
    QCOMPARE(ryubing.value("describe").toString(), QStringLiteral("~/bin/ryujinx"));
    const QVariantMap mgba = emulator("mgba");
    QVERIFY(mgba.value("besideRomSaves").toBool());
    QCOMPARE(mgba.value("besideRoms").toInt(), 1);
    QCOMPARE(mgba.value("besideBytes").toLongLong(), 25);
    QCOMPARE(emulator("cemu").value("saves").toStringList(), QStringList());
    QCOMPARE(emulator("cemu").value("keysBytes").toLongLong(), 9);
    const QVariantList proton = backups.savesPlan().value("proton").toList();
    QCOMPARE(proton.size(), 1);
    QCOMPARE(proton.first().toMap().value("title").toString(), QStringLiteral("Cave Story"));
    QCOMPARE(proton.first().toMap().value("skipped").toString(), QString());

    // The choices are [backup] in settings.toml.
    QCOMPARE(backups.choices(), (QVariantMap{{"credentials", true}, {"keys", true}, {"proton", true}}));
    QSignalSpy choices(&backups, &Backups::choicesChanged);
    QCOMPARE(backups.setChoice("keys", false), QString());
    QCOMPARE(choices.size(), 1);
    QCOMPARE(backups.choices().value("keys").toBool(), false);
    QVERIFY(readFile(m_data + QStringLiteral("/settings.toml")).contains("keys = false"));

    // A backup: where it went is remembered for the next one.
    QVERIFY(backups.last().isEmpty());
    QCOMPARE(backups.defaultFile("library").adjusted(QUrl::RemoveFilename), QUrl::fromLocalFile(QDir::homePath() + u'/'));
    QCOMPARE(backups.start("library", QUrl()), QStringLiteral("Pick a file on this computer."));
    QCOMPARE(backups.start("everything", QUrl::fromLocalFile(path("out/all.tar.zst"))),
             QStringLiteral("Unknown backup: everything"));
    QSignalSpy finished(&backups, &Backups::finished);
    QSignalSpy last(&backups, &Backups::lastChanged);
    const QString file = path(QStringLiteral("out/library.tar.zst"));
    QDir().mkpath(path(QStringLiteral("out")));
    QCOMPARE(backups.start("library", QUrl::fromLocalFile(file)), QString());
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.first(), (QVariantList{"library", file, QString()}));
    QCOMPARE(last.size(), 1);
    QVERIFY(QFile::exists(file));
    const QVariantMap lastLibrary = backups.last().value("library").toMap();
    QCOMPARE(lastLibrary.value("text").toString(), QStringLiteral("Last backed up today to ") + file);
    QCOMPARE(lastLibrary.value("folder").toUrl(), QUrl::fromLocalFile(path(QStringLiteral("out"))));
    QCOMPARE(backups.defaultFile("library").adjusted(QUrl::RemoveFilename),
             QUrl::fromLocalFile(path(QStringLiteral("out/"))));
    QVERIFY(!backups.last().contains("saves"));
}

QTEST_GUILESS_MAIN(RestoreTest)
#include "restore_test.moc"
