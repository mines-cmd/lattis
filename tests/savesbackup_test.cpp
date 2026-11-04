#include "savesbackup.h"
#include "backuparchive.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <toml++/toml.hpp>

using namespace SavesBackup;

namespace {
void write(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QDateTime at(int seconds)
{
    return QDateTime(QDate(2026, 11, 3), QTime(12, 0, 0)).addSecs(seconds);
}
}

class SavesBackupTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_home;
    QString m_manifestDir;
    QString m_folder;
    QStringList m_trashed;

    Trash fakeTrash()
    {
        return [this](const QString &path, QString *) {
            m_trashed.append(path);
            return QFile::remove(path);
        };
    }

    Input input(const QString &title = QStringLiteral("Celeste"))
    {
        Input in;
        in.gameId = QStringLiteral("linux-celeste");
        in.title = title;
        in.env.home = m_home;
        in.env.xdgData = m_home + "/.local/share";
        in.env.xdgConfig = m_home + "/.config";
        in.env.user = "someone";
        SavesFinder::Query q;
        q.id = in.gameId;
        q.title = title;
        in.copies.append(q);
        in.manifestDir = m_manifestDir;
        in.options.folder = m_folder;
        in.appVersion = "1.2.3";
        in.now = at(0);
        in.trash = fakeTrash();
        return in;
    }

    void saveSlot(const QByteArray &content = "slot")
    {
        write(m_home + "/.local/share/Celeste/Saves/0.celeste", content);
    }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QDir(m_dir.path()).removeRecursively();
        m_home = m_dir.filePath("home");
        m_manifestDir = m_dir.filePath("manifest");
        m_folder = m_dir.filePath("backups");
        m_trashed.clear();
        QDir().mkpath(m_home);
        QDir().mkpath(m_manifestDir);
        SavesIndex index;
        SavesGame celeste;
        celeste.name = "Celeste";
        celeste.paths.append(SavesPath{"<xdgData>/Celeste/Saves", {}});
        index.games.append(celeste);
        QVERIFY(SavesIndexFile::save(m_manifestDir + "/index.bin", index));
    }

    void aGameOwnChoiceWinsOverItsPlatform()
    {
        QVERIFY(SavesBackup::autoBackupResolved(-1, true));
        QVERIFY(!SavesBackup::autoBackupResolved(-1, false));
        QVERIFY(SavesBackup::autoBackupResolved(1, false));
        QVERIFY(!SavesBackup::autoBackupResolved(0, true));
    }
    void writesTheSavesAndAManifest();
    void unchangedSavesMakeNoNewBackup();
    void keepsTheLastFewAndTrashesTheRest();
    void aGameOverTheSizeCapIsSkipped();
    void noMatchAndNoOwnPathsWritesNothing();
    void ownPathsBackUpWithoutAMatch();
    void aProtonGameWithNoMatchFallsBackToItsPrefix();
    void aFailureLeavesNoPartialFile();
    void anEmulatorBacksUpItsWholeSaveFolders();
    void twoGamesOfOneEmulatorShareOneBackup();

    void aSessionEndMakesTheBackup();
    void aSessionShorterThanTheMinimumMakesNone();
    void waitsForTheGameAndItsManagersToClose();
    void aGameWithBackupOffMakesNone();
    void failuresAndTheCapShowAMessage();
};

void SavesBackupTest::writesTheSavesAndAManifest()
{
    saveSlot();
    const Result result = run(input());
    QCOMPARE(result.status, Status::Written);
    QCOMPARE(result.files, 1);
    QCOMPARE(result.archive, m_folder + "/linux-celeste/2026-11-03-120000.tar.zst");
    QVERIFY(QFileInfo::exists(result.archive));
    QVERIFY(!QFileInfo::exists(result.archive + ".partial"));

    QString error;
    const std::optional<QByteArray> bytes = Archive::readFirst(result.archive, "manifest.toml", &error);
    QVERIFY2(bytes, qPrintable(error));
    const toml::table manifest = toml::parse(std::string_view(bytes->constData(), size_t(bytes->size())));
    QCOMPARE(QString::fromStdString(manifest["kind"].value_or(std::string())), QStringLiteral("game-saves"));
    QCOMPARE(manifest["format"].value_or(0), 1);
    QCOMPARE(QString::fromStdString(manifest["game"].value_or(std::string())), QStringLiteral("linux-celeste"));
    QCOMPARE(QString::fromStdString(manifest["matched"].value_or(std::string())), QStringLiteral("Celeste"));
    QCOMPARE(QString::fromStdString(manifest["app_version"].value_or(std::string())), QStringLiteral("1.2.3"));
    QCOMPARE(manifest["files"].value_or(0), 1);
    const toml::table *path = manifest["path"][0].as_table();
    QVERIFY(path);
    QCOMPARE(QString::fromStdString((*path)["from"].value_or(std::string())),
             QStringLiteral("~/.local/share/Celeste/Saves"));
    QCOMPARE(QString::fromStdString((*path)["type"].value_or(std::string())), QStringLiteral("folder"));

    QProcess tar;
    tar.start("tar", {"--zstd", "-tf", result.archive});
    if (!tar.waitForFinished() || tar.exitCode() != 0)
        QSKIP("tar can't list a zstd archive here");
    const QString listing = QString::fromUtf8(tar.readAllStandardOutput());
    QVERIFY2(listing.contains("files/0/0.celeste"), qPrintable(listing));
    QVERIFY(listing.contains("manifest.toml"));
}

void SavesBackupTest::unchangedSavesMakeNoNewBackup()
{
    saveSlot();
    QCOMPARE(run(input()).status, Status::Written);
    Input later = input();
    later.now = at(60);
    QCOMPARE(run(later).status, Status::Unchanged);
    QCOMPARE(backups(m_folder, "linux-celeste").size(), 1);

    saveSlot("a longer slot");
    later.now = at(120);
    QCOMPARE(run(later).status, Status::Written);
    QCOMPARE(backups(m_folder, "linux-celeste").size(), 2);

    write(m_home + "/.local/share/Celeste/Saves/1.celeste", "new");
    later.now = at(180);
    QCOMPARE(run(later).status, Status::Written);

    QFile touched(m_home + "/.local/share/Celeste/Saves/1.celeste");
    QVERIFY(touched.open(QIODevice::ReadWrite));
    QVERIFY(touched.setFileTime(QDateTime(QDate(2020, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime));
    touched.close();
    later.now = at(240);
    QCOMPARE(run(later).status, Status::Written); // same names and sizes, another modification time
}

void SavesBackupTest::keepsTheLastFewAndTrashesTheRest()
{
    for (int i = 0; i < 4; ++i) {
        saveSlot(QByteArray(10 + i, 'x'));
        Input in = input();
        in.now = at(i * 60);
        in.options.keep = 2;
        QCOMPARE(run(in).status, Status::Written);
    }
    const QStringList left = backups(m_folder, "linux-celeste");
    QCOMPARE(left.size(), 2);
    QVERIFY(left.first().endsWith("2026-11-03-120300.tar.zst"));
    QVERIFY(left.last().endsWith("2026-11-03-120200.tar.zst"));
    QCOMPARE(m_trashed.size(), 2); // the oldest, after the third backup and again after the fourth
}

void SavesBackupTest::aGameOverTheSizeCapIsSkipped()
{
    saveSlot(QByteArray(2048, 'x'));
    Input in = input();
    in.options.maxBytes = 1024;
    const Result result = run(in);
    QCOMPARE(result.status, Status::TooBig);
    QCOMPARE(result.bytes, 2048);
    QVERIFY(!QFileInfo::exists(m_folder));
}

void SavesBackupTest::noMatchAndNoOwnPathsWritesNothing()
{
    saveSlot();
    QCOMPARE(run(input("A game the manifest lacks")).status, Status::Nothing);
    QVERIFY(!QFileInfo::exists(m_folder));

    QCOMPARE(run(input()).status, Status::Written);
    QDir(m_home).removeRecursively();
    QDir().mkpath(m_home);
    Input in = input();
    in.now = at(60);
    QCOMPARE(run(in).status, Status::Nothing); // the matched path is gone
    QCOMPARE(backups(m_folder, "linux-celeste").size(), 1);
}

void SavesBackupTest::ownPathsBackUpWithoutAMatch()
{
    write(m_home + "/games/mine/save.dat", "data");
    Input in = input("Not in the manifest");
    in.copies[0].savePaths = {"~/games/mine"};
    const Result result = run(in);
    QCOMPARE(result.status, Status::Written);
    QCOMPARE(result.files, 1);
}

void SavesBackupTest::aProtonGameWithNoMatchFallsBackToItsPrefix()
{
    const QString prefix = m_dir.filePath("compatdata/77");
    write(prefix + "/tracked_files", "drive_c/windows/system32/kernel32.dll\n");
    write(prefix + "/pfx/drive_c/users/steamuser/Documents/Game/save.dat", "progress");
    write(prefix + "/pfx/drive_c/windows/system32/kernel32.dll", "proton's");
    write(prefix + "/pfx/user.reg", "reg");

    Input in = input("Not in the manifest");
    in.protonPrefix = prefix;
    const Result result = run(in);
    QCOMPARE(result.status, Status::Written);
    QVERIFY(result.files >= 2);

    QString error;
    const std::optional<QByteArray> bytes = Archive::readFirst(result.archive, "manifest.toml", &error);
    QVERIFY(bytes);
    QVERIFY(bytes->contains("prefix = true"));
    QVERIFY(bytes->contains("compatdata/77"));

    QProcess tar;
    tar.start("tar", {"--zstd", "-tf", result.archive});
    if (tar.waitForFinished() && tar.exitCode() == 0) {
        const QString listing = QString::fromUtf8(tar.readAllStandardOutput());
        QVERIFY(listing.contains("Game/save.dat"));
        QVERIFY(!listing.contains("kernel32.dll"));
    }

    // A prefix Proton didn't list files for isn't backed up whole.
    in.protonPrefix = m_dir.filePath("compatdata/78");
    QCOMPARE(run(in).status, Status::Nothing);
}

void SavesBackupTest::aFailureLeavesNoPartialFile()
{
    saveSlot();
    write(m_folder + "/linux-celeste", "a file where the folder should go");
    const Result result = run(input());
    QCOMPARE(result.status, Status::Failed);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(backups(m_folder, "linux-celeste").isEmpty());
}

void SavesBackupTest::anEmulatorBacksUpItsWholeSaveFolders()
{
    const QString saves = m_home + "/.local/share/ryujinx/saves";
    write(saves + "/0001/data.sav", "one");
    write(saves + "/0002/data.sav", "two");
    write(m_home + "/Roms/Pikmin 4.sav", "beside");
    const auto emulator = [&] {
        Input in = input(QStringLiteral("Ryujinx"));
        in.copies.clear();
        in.emulatorId = QStringLiteral("ryujinx");
        in.gameId = backupKey(in);
        in.emulatorPaths = {saves, m_home + "/Roms/Pikmin 4.sav"};
        return in;
    };
    QCOMPARE(backupKey(emulator()), QStringLiteral("emulator-ryujinx"));

    const Result result = run(emulator());
    QCOMPARE(result.status, Status::Written);
    QCOMPARE(result.gameId, QStringLiteral("emulator-ryujinx"));
    QCOMPARE(result.files, 3);
    QCOMPARE(backups(m_folder, "emulator-ryujinx").size(), 1);

    QString error;
    const std::optional<QByteArray> bytes =
        Archive::readFirst(result.archive, QStringLiteral("manifest.toml"), &error);
    QVERIFY2(bytes, qPrintable(error));
    const toml::table manifest = toml::parse(std::string_view(bytes->constData(), size_t(bytes->size())));
    QCOMPARE(QString::fromStdString(manifest["kind"].value_or(std::string())), kKind);
    QCOMPARE(QString::fromStdString(manifest["game"].value_or(std::string())), QStringLiteral("emulator-ryujinx"));
    QCOMPARE(QString::fromStdString(manifest["emulator"].value_or(std::string())), QStringLiteral("ryujinx"));
    QCOMPARE(manifest["path"].as_array()->size(), size_t(2));

    Input later = emulator();
    later.now = at(60);
    QCOMPARE(run(later).status, Status::Unchanged);
    write(saves + "/0003/data.sav", "three");
    later.now = at(120);
    QCOMPARE(run(later).status, Status::Written);

    Input none = emulator();
    none.emulatorPaths = {m_home + "/gone"};
    QCOMPARE(run(none).status, Status::Nothing);
}

void SavesBackupTest::twoGamesOfOneEmulatorShareOneBackup()
{
    const QString saves = m_home + "/.local/share/ryujinx/saves";
    write(saves + "/0001/data.sav", "one");
    int gathered = 0;
    Service service(
        [&](const QString &) -> std::optional<Input> {
            ++gathered;
            Input in = input(QStringLiteral("Ryujinx"));
            in.copies.clear();
            in.emulatorId = QStringLiteral("ryujinx");
            in.gameId = backupKey(in);
            in.emulatorPaths = {saves};
            return in;
        },
        {}, [] { return 0; });
    QSignalSpy finished(&service, &Service::finished);

    service.started("switch-pikmin-4");
    service.started("switch-zelda");
    service.ended("switch-pikmin-4");
    service.ended("switch-zelda");
    QVERIFY(finished.wait(10000));
    QTest::qWait(100);
    QCOMPARE(gathered, 2);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(backups(m_folder, "emulator-ryujinx").size(), 1);
}

void SavesBackupTest::aSessionEndMakesTheBackup()
{
    saveSlot();
    QStringList asked;
    Service service(
        [&](const QString &id) -> std::optional<Input> {
            asked.append(id);
            return input();
        },
        {}, [] { return 0; });
    QSignalSpy finished(&service, &Service::finished);
    QSignalSpy message(&service, &Service::message);

    service.started("linux-celeste");
    QVERIFY(asked.isEmpty()); // nothing is made before or while the game runs
    service.ended("linux-celeste");
    QVERIFY(finished.wait(10000));
    QCOMPARE(asked, QStringList{"linux-celeste"});
    const Result result = finished.first().first().value<Result>();
    QCOMPARE(result.status, Status::Written);
    QCOMPARE(backups(m_folder, "linux-celeste").size(), 1);
    QVERIFY(message.isEmpty());

    service.ended("linux-celeste"); // a second end without a start makes nothing
    QTest::qWait(50);
    QCOMPARE(finished.size(), 1);
}

void SavesBackupTest::aSessionShorterThanTheMinimumMakesNone()
{
    saveSlot();
    int asked = 0;
    Service service(
        [&](const QString &) -> std::optional<Input> {
            ++asked;
            return input();
        },
        {}, [] { return 3600; });
    QSignalSpy finished(&service, &Service::finished);
    service.started("linux-celeste");
    service.ended("linux-celeste");
    QTest::qWait(100);
    QCOMPARE(asked, 0);
    QVERIFY(finished.isEmpty());
    QVERIFY(!QFileInfo::exists(m_folder));
}

void SavesBackupTest::waitsForTheGameAndItsManagersToClose()
{
    saveSlot();
    bool managerOpen = true;
    Service service(
        [&](const QString &id) -> std::optional<Input> {
            return id == "linux-celeste" ? std::optional<Input>(input()) : std::nullopt;
        },
        [&](const QString &) { return managerOpen; }, [] { return 0; });
    QSignalSpy finished(&service, &Service::finished);

    service.started("linux-celeste");
    service.started("manager:vortex");
    service.ended("linux-celeste");
    QTest::qWait(100);
    QVERIFY(finished.isEmpty());

    managerOpen = false;
    service.ended("manager:vortex"); // the manager closing is what lets the game's backup go
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().value<Result>().gameId, QStringLiteral("linux-celeste"));
}

void SavesBackupTest::aGameWithBackupOffMakesNone()
{
    saveSlot();
    Service service([](const QString &) -> std::optional<Input> { return std::nullopt; }, {}, [] { return 0; });
    QSignalSpy finished(&service, &Service::finished);
    service.started("linux-celeste");
    service.ended("linux-celeste");
    QTest::qWait(100);
    QVERIFY(finished.isEmpty());
    QVERIFY(!service.running());
}

void SavesBackupTest::failuresAndTheCapShowAMessage()
{
    saveSlot(QByteArray(2048, 'x'));
    Input in = input();
    Service service([&](const QString &) -> std::optional<Input> { return in; }, {}, [] { return 0; });
    QSignalSpy finished(&service, &Service::finished);
    QSignalSpy message(&service, &Service::message);

    in.options.maxBytes = 1024;
    service.started("linux-celeste");
    service.ended("linux-celeste");
    QVERIFY(finished.wait(10000));
    QCOMPARE(message.size(), 1);
    QVERIFY(message.first().first().toString().contains("Celeste"));

    in.options.maxBytes = qint64(1) << 30;
    write(m_folder + "/linux-celeste", "in the way");
    service.started("linux-celeste");
    service.ended("linux-celeste");
    QVERIFY(finished.wait(10000));
    QCOMPARE(message.size(), 2);
    QVERIFY(message.last().first().toString().contains("Celeste"));
}

QTEST_MAIN(SavesBackupTest)
#include "savesbackup_test.moc"
