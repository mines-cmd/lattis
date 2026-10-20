#include "backup.h"
#include "libraryfiles.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

#include <memory>

namespace {

const QString kCompatdata = QStringLiteral(STEAM_DATA_DIR "/steamapps/compatdata");
const quint32 kCaveStory = 2900000001u; // the fake Steam's prefix
const QDateTime kCreated(QDate(2026, 9, 28), QTime(21, 4), QTimeZone::fromSecondsAheadOfUtc(-3 * 3600));

void writeFile(const QString &path, qint64 size = 1)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(size, 'x'));
}

// Every entry of an archive, in order: folders end in '/', links are "name -> target".
QStringList entries(const QString &path)
{
    QStringList out;
    std::unique_ptr<struct archive, decltype(&archive_read_free)> a(archive_read_new(), archive_read_free);
    archive_read_support_filter_zstd(a.get());
    archive_read_support_format_tar(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(path).constData(), 64 * 1024) != ARCHIVE_OK)
        return {QStringLiteral("can't open")};
    archive_entry *entry = nullptr;
    while (archive_read_next_header(a.get(), &entry) == ARCHIVE_OK) {
        QString name = QString::fromUtf8(archive_entry_pathname(entry));
        if (archive_entry_filetype(entry) == AE_IFDIR && !name.endsWith(u'/'))
            name += u'/';
        if (archive_entry_filetype(entry) == AE_IFLNK)
            name += QStringLiteral(" -> ") + QString::fromUtf8(archive_entry_symlink(entry));
        out.append(name);
        archive_read_data_skip(a.get());
    }
    return out;
}

QStringList files(const QStringList &entries)
{
    QStringList out;
    for (const QString &e : entries) {
        if (!e.endsWith(u'/'))
            out.append(e);
    }
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

EmulatorInstall installed(const QString &program)
{
    EmulatorInstall install;
    install.kind = EmulatorInstall::Kind::Path;
    install.program = program;
    return install;
}

} // namespace

class BackupTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void library();
    void libraryWithoutCredentials();
    void libraryInsideDataDir();
    void saves();
    void savesWithoutKeysAndProton();
    void cancel();
    void readOnlyTarget();
    void lastBackups();
    void job();
    void jobRefusedWhileGameRuns();

private:
    QString path(const QString &relative) const { return m_dir->filePath(relative); }
    BackupPlan::Sources sources() const;

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_data;
    QString m_home;
};

void BackupTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_data = path(QStringLiteral("data"));
    m_home = path(QStringLiteral("home"));

    for (const QString &file : {QStringLiteral("library/library.toml"), QStringLiteral("library/platforms.toml"),
                                QStringLiteral("library/games/gba-emerald.toml"), QStringLiteral("settings.toml"),
                                QStringLiteral("credentials.toml"), QStringLiteral("art/gba-emerald/cover.png"),
                                QStringLiteral("themes/colors/reference.toml"), QStringLiteral("themes/styles/reference.toml"), QStringLiteral("roms/gba/Kept.gba")})
        writeFile(m_data + u'/' + file);

    // Ryubing: a save folder with a link in it, a second one missing, and keys; Cemu: a key
    // file; mGBA: a save next to its ROM.
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/0/data.bin"), 3000);
    QVERIFY(QFile::link(QStringLiteral("0/data.bin"), m_home + QStringLiteral("/.config/Ryujinx/bis/user/save/0001/latest")));
    writeFile(m_home + QStringLiteral("/.config/Ryujinx/system/prod.keys"), 7);
    writeFile(m_home + QStringLiteral("/.local/share/Cemu/keys.txt"), 5);
    writeFile(m_home + QStringLiteral("/Games/Emerald.gba"), 100);
    writeFile(m_home + QStringLiteral("/Games/Emerald.sav"), 2);
}

BackupPlan::Sources BackupTest::sources() const
{
    BackupPlan::Sources s;
    s.dataDir = m_data;
    s.home = m_home;
    s.library = {1, QStringLiteral("ea9d5f1e-9ab9-42f0-b1b6-fba7c0552362")};
    s.compatdata = kCompatdata;

    const QString native = QStringLiteral("native");
    EmulatorDef ryubing;
    ryubing.id = QStringLiteral("ryubing");
    ryubing.name = QStringLiteral("Ryubing");
    ryubing.saves = {{native, {QStringLiteral("~/.config/Ryujinx/bis/user/save"),
                               QStringLiteral("~/.config/Ryujinx/bis/system/save")}}};
    ryubing.keys = {{native, {QStringLiteral("~/.config/Ryujinx/system")}}};
    EmulatorDef cemu;
    cemu.id = QStringLiteral("cemu");
    cemu.name = QStringLiteral("Cemu");
    cemu.saves = {{native, {QStringLiteral("~/.local/share/Cemu/mlc01/usr/save")}}};
    cemu.keys = {{native, {QStringLiteral("~/.local/share/Cemu/keys.txt")}}};
    EmulatorDef mgba;
    mgba.id = QStringLiteral("mgba");
    mgba.name = QStringLiteral("mGBA");
    mgba.savesBesideRom = {QStringLiteral("sav")};
    s.emulators = {cemu, mgba, ryubing};
    s.installs = {{QStringLiteral("ryubing"), installed(m_home + QStringLiteral("/bin/ryujinx"))},
                  {QStringLiteral("cemu"), installed(m_home + QStringLiteral("/bin/cemu"))},
                  {QStringLiteral("mgba"), installed(m_home + QStringLiteral("/bin/mgba"))}};

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
    emerald.rom = true;
    emerald.romPath = m_home + QStringLiteral("/Games/Emerald.gba");
    Game caveStory = game(QStringLiteral("proton-cave-story"), kProtonPlatform);
    caveStory.title = QStringLiteral("Cave Story");
    caveStory.shortcut = kCaveStory;
    Game neverLaunched = game(QStringLiteral("proton-new"), kProtonPlatform);
    neverLaunched.shortcut = 1234u;
    s.games = {caveStory, emerald, neverLaunched};
    return s;
}

void BackupTest::library()
{
    const BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(sources(), {});
    const QString archive = path(QStringLiteral("out/library.tar.zst"));
    QVERIFY(QDir().mkpath(path(QStringLiteral("out"))));
    QString part;
    qint64 bytes = 0;
    const QString error = Backup::write(plan, archive, kCreated, QStringLiteral("0.1"),
                                        {[&](qint64 b, const QString &p) { bytes = b, part = p; }, nullptr});
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!QFile::exists(archive + QStringLiteral(".partial")));
    QCOMPARE(part, QStringLiteral("The library"));
    QCOMPARE(bytes, 6); // the six 1-byte files

    const QStringList all = entries(archive);
    QCOMPARE(all.first(), QStringLiteral("manifest.toml"));
    QCOMPARE(files(all), QStringList({QStringLiteral("data/art/gba-emerald/cover.png"),
                                      QStringLiteral("data/credentials.toml"),
                                      QStringLiteral("data/library/games/gba-emerald.toml"),
                                      QStringLiteral("data/library/library.toml"),
                                      QStringLiteral("data/library/platforms.toml"),
                                      QStringLiteral("data/settings.toml"), QStringLiteral("manifest.toml")}));
    QVERIFY(all.contains(QStringLiteral("data/")));
    QVERIFY(!all.contains(QStringLiteral("data/roms/gba/")));

    QString readError;
    const std::optional<QByteArray> manifest = Archive::readFirst(archive, QStringLiteral("manifest.toml"), &readError);
    QVERIFY(manifest);
    QCOMPARE(*manifest, BackupPlan::manifest([&] {
        BackupPlan::LibraryPlan measured = plan;
        BackupPlan::measure(measured);
        return measured;
    }(), kCreated, QStringLiteral("0.1")));
}

void BackupTest::libraryWithoutCredentials()
{
    const BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(sources(), {false, true, true});
    const QString archive = path(QStringLiteral("library.tar.zst"));
    QCOMPARE(Backup::write(plan, archive, kCreated, QStringLiteral("0.1")), QString());
    QVERIFY(!entries(archive).contains(QStringLiteral("data/credentials.toml")));
}

void BackupTest::libraryInsideDataDir()
{
    // Saved into the data dir: the archive doesn't hold itself.
    const BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(sources(), {});
    const QString archive = m_data + QStringLiteral("/library.tar.zst");
    writeFile(archive, 10); // an earlier one, replaced
    QCOMPARE(Backup::write(plan, archive, kCreated, QStringLiteral("0.1")), QString());
    const QStringList all = entries(archive);
    QVERIFY(!all.contains(QStringLiteral("data/library.tar.zst")));
    QVERIFY(!all.contains(QStringLiteral("data/library.tar.zst.partial")));
    QVERIFY(all.contains(QStringLiteral("data/settings.toml")));
}

void BackupTest::saves()
{
    BackupPlan::SavesPlan plan = BackupPlan::planSaves(sources(), {});
    const QString archive = path(QStringLiteral("saves.tar.zst"));
    QStringList parts;
    const QString error = Backup::write(plan, archive, kCreated, QStringLiteral("0.1"),
                                        {[&](qint64, const QString &p) {
                                             if (parts.isEmpty() || parts.last() != p)
                                                 parts.append(p);
                                         },
                                         nullptr});
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(parts, QStringList({QStringLiteral("Cemu's keys and firmware"), QStringLiteral("mGBA's saves next to ROMs"),
                                 QStringLiteral("Ryubing's saves"), QStringLiteral("Ryubing's keys and firmware"),
                                 QStringLiteral("Cave Story")}));

    // Entry by entry against the plan.
    BackupPlan::measure(plan);
    QStringList expected = {
        QStringLiteral("manifest.toml"),
        QStringLiteral("emulators/cemu/keys/1/keys.txt"),
        QStringLiteral("emulators/mgba/beside-roms/gba-emerald/Emerald.sav"),
        QStringLiteral("emulators/ryubing/saves/1/0001/0/data.bin"),
        QStringLiteral("emulators/ryubing/saves/1/0001/latest -> 0/data.bin"),
        QStringLiteral("emulators/ryubing/keys/1/prod.keys"),
    };
    const ProtonSaves::GameSaves &caveStory = plan.protonGames.first().saves;
    QVERIFY(!caveStory.files.isEmpty());
    for (const ProtonSaves::SaveFile &file : caveStory.files) {
        const QFileInfo info(caveStory.prefix + u'/' + file.path);
        QString entry = QStringLiteral("proton/proton-cave-story/") + file.path;
        if (info.isSymLink())
            entry += QStringLiteral(" -> ") + info.readSymLink();
        expected.append(entry);
    }
    expected.sort();
    QStringList all = entries(archive);
    QCOMPARE(all.first(), QStringLiteral("manifest.toml"));
    // readSymLink() resolves relative targets; compare the links by name only.
    const auto names = [](QStringList list) {
        for (QString &e : list)
            e = e.section(QStringLiteral(" -> "), 0, 0);
        return list;
    };
    QCOMPARE(names(files(all)), names(expected));
    QVERIFY(all.contains(QStringLiteral("emulators/ryubing/saves/1/0001/latest -> 0/data.bin")));
    // Missing places (Ryubing's second save folder, Cemu's saves) aren't there.
    QVERIFY(!all.join(u'\n').contains(QStringLiteral("emulators/ryubing/saves/2")));
    QVERIFY(!all.join(u'\n').contains(QStringLiteral("emulators/cemu/saves")));
    QVERIFY(!all.join(u'\n').contains(QStringLiteral("proton/proton-new")));

    // Extracted, each part as it was.
    const QString out = path(QStringLiteral("extracted"));
    QString extractError;
    QVERIFY2(Archive::extract(archive, {{QStringLiteral("emulators"), out}},
                              {QStringLiteral("manifest.toml"), QStringLiteral("proton")}, &extractError),
             qPrintable(extractError));
    QCOMPARE(QFileInfo(out + QStringLiteral("/ryubing/saves/1/0001/0/data.bin")).size(), 3000);
    QCOMPARE(QFileInfo(out + QStringLiteral("/ryubing/saves/1/0001/latest")).symLinkTarget(),
             out + QStringLiteral("/ryubing/saves/1/0001/0/data.bin"));

    const QByteArray manifest = *Archive::readFirst(archive, QStringLiteral("manifest.toml"), &extractError);
    QCOMPARE(manifest, BackupPlan::manifest(plan, kCreated, QStringLiteral("0.1")));
}

void BackupTest::savesWithoutKeysAndProton()
{
    const BackupPlan::SavesPlan plan = BackupPlan::planSaves(sources(), {true, false, false});
    const QString archive = path(QStringLiteral("saves.tar.zst"));
    QCOMPARE(Backup::write(plan, archive, kCreated, QStringLiteral("0.1")), QString());
    QCOMPARE(files(entries(archive)),
             QStringList({QStringLiteral("emulators/mgba/beside-roms/gba-emerald/Emerald.sav"),
                          QStringLiteral("emulators/ryubing/saves/1/0001/0/data.bin"),
                          QStringLiteral("emulators/ryubing/saves/1/0001/latest -> 0/data.bin"),
                          QStringLiteral("manifest.toml")}));
}

void BackupTest::cancel()
{
    const std::atomic_bool cancelled = true;
    const QString archive = path(QStringLiteral("saves.tar.zst"));
    const QString error = Backup::write(BackupPlan::planSaves(sources(), {}), archive, kCreated,
                                        QStringLiteral("0.1"), {{}, &cancelled});
    QCOMPARE(error, QStringLiteral("Cancelled"));
    QVERIFY(!QFile::exists(archive));
    QVERIFY(!QFile::exists(archive + QStringLiteral(".partial")));
}

void BackupTest::readOnlyTarget()
{
    const QString folder = path(QStringLiteral("read-only"));
    QVERIFY(QDir().mkpath(folder));
    QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::ExeOwner));
    const QString archive = folder + QStringLiteral("/library.tar.zst");
    const QString error =
        Backup::write(BackupPlan::planLibrary(sources(), {}), archive, kCreated, QStringLiteral("0.1"));
    QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QVERIFY(error.startsWith(QStringLiteral("Can't write ") + archive + QStringLiteral(".partial")));
    QVERIFY(QDir(folder).isEmpty());
}

void BackupTest::lastBackups()
{
    const QString file = Backup::lastFile(path(QStringLiteral("state")), QStringLiteral("0123456789ab"));
    QCOMPARE(file, path(QStringLiteral("state/backups-0123456789ab.toml")));
    QVERIFY(Backup::readLast(file).isEmpty());

    const Backup::Last library{QStringLiteral("/backups/lattis-library-2026-09-28.tar.zst"), kCreated};
    const Backup::Last saves{QStringLiteral("/backups/\"odd\" name.tar.zst"), kCreated.addDays(1)};
    QVERIFY(Backup::writeLast(file, Backup::kLibrary, library));
    QVERIFY(Backup::writeLast(file, Backup::kSaves, saves));
    const QHash<QString, Backup::Last> last = Backup::readLast(file);
    QCOMPARE(last.value(Backup::kLibrary), library);
    QCOMPARE(last.value(Backup::kSaves), saves);
    QCOMPARE(last.value(Backup::kSaves).time.offsetFromUtc(), -3 * 3600);
}

void BackupTest::job()
{
    const QString lastFile = path(QStringLiteral("state/backups.toml"));
    BackupJob job(lastFile, [] { return QStringList(); });
    QSignalSpy finished(&job, &BackupJob::finished);
    const QString archive = path(QStringLiteral("library.tar.zst"));
    BackupPlan::LibraryPlan plan = BackupPlan::planLibrary(sources(), {});
    BackupPlan::measure(plan);
    QCOMPARE(job.start(plan, archive), QString());
    QVERIFY(job.running());
    QCOMPARE(job.kind(), Backup::kLibrary);
    QCOMPARE(job.total(), 6);
    QCOMPARE(job.start(plan, archive), QStringLiteral("A backup is being made already."));
    QVERIFY(finished.wait());
    QCOMPARE(finished.first(), QVariantList({Backup::kLibrary, archive, QString()}));
    QVERIFY(!job.running());
    QCOMPARE(job.bytes(), 6);
    QVERIFY(QFile::exists(archive));
    QCOMPARE(job.last(Backup::kLibrary).path, archive);
    QVERIFY(qAbs(job.last(Backup::kLibrary).time.secsTo(QDateTime::currentDateTime())) < 5);
    QVERIFY(job.last(Backup::kSaves).path.isEmpty());
}

void BackupTest::jobRefusedWhileGameRuns()
{
    QStringList running = {QStringLiteral("Hades")};
    BackupJob job(path(QStringLiteral("backups.toml")), [&] { return running; });
    const QString archive = path(QStringLiteral("saves.tar.zst"));
    const BackupPlan::SavesPlan plan = BackupPlan::planSaves(sources(), {});
    QCOMPARE(job.start(plan, archive), QStringLiteral("Close Hades first: it may be writing its saves."));
    QVERIFY(!job.running());
    running.append(QStringLiteral("Celeste"));
    QCOMPARE(job.gamesRunning(), QStringLiteral("Close Hades and Celeste first: it may be writing its saves."));

    // The library backup doesn't touch saves.
    QSignalSpy finished(&job, &BackupJob::finished);
    QCOMPARE(job.start(BackupPlan::planLibrary(sources(), {}), path(QStringLiteral("library.tar.zst"))), QString());
    QVERIFY(finished.wait());

    running.clear();
    QCOMPARE(job.start(plan, archive), QString());
    QVERIFY(finished.wait());
    QCOMPARE(finished.last().at(2).toString(), QString());
    QVERIFY(QFile::exists(archive));
}

QTEST_GUILESS_MAIN(BackupTest)
#include "backup_test.moc"
