#include "apppaths.h"
#include "oldname.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

class OldNameTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void movesTheFoldersAndRenamesKeyedFiles();
    void keepsWhatTheNewNameHasAlready();
    void runningOldCopyKeepsItsDataDir();

private:
    void write(const QString &path, const QByteArray &contents = "x");
    QString read(const QString &path);
    OldName::Folders folders() const;

    std::unique_ptr<QTemporaryDir> m_dir;
};

void OldNameTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
}

void OldNameTest::write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

QString OldNameTest::read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

OldName::Folders OldNameTest::folders() const
{
    const QString root = m_dir->path();
    return {root + "/share/game-launcher", root + "/share/lattis", root + "/state/game-launcher",
            root + "/state/lattis"};
}

void OldNameTest::movesTheFoldersAndRenamesKeyedFiles()
{
    const OldName::Folders f = folders();
    const QString oldKey = AppPaths::dataDirKey(f.dataFrom);
    const QString newKey = AppPaths::dataDirKey(f.dataTo);
    write(f.dataFrom + "/library/games/switch-mario.toml", "title = \"Mario\"");
    write(f.stateFrom + "/windows-" + oldKey + ".toml", "main");
    write(f.stateFrom + "/steam-sync-" + oldKey + ".toml", "sync");
    write(f.stateFrom + "/windows-0123456789ab.toml", "scratch");
    write(f.stateFrom + "/steam-backup/config/config.vdf");

    const QStringList log = OldName::move(f);

    QCOMPARE(log.size(), 4);
    QVERIFY(!QFileInfo::exists(f.dataFrom));
    QCOMPARE(read(f.dataTo + "/library/games/switch-mario.toml"), "title = \"Mario\"");
    QCOMPARE(read(f.stateTo + "/windows-" + newKey + ".toml"), "main");
    QCOMPARE(read(f.stateTo + "/steam-sync-" + newKey + ".toml"), "sync");
    QVERIFY(!QFileInfo::exists(f.stateTo + "/windows-" + oldKey + ".toml"));
    // Another data dir's files keep their key: that dir didn't move.
    QCOMPARE(read(f.stateTo + "/windows-0123456789ab.toml"), "scratch");
    QVERIFY(QFileInfo::exists(f.stateTo + "/steam-backup/config/config.vdf"));
}

void OldNameTest::keepsWhatTheNewNameHasAlready()
{
    const OldName::Folders f = folders();
    write(f.dataFrom + "/settings.toml", "old");
    write(f.dataTo + "/settings.toml", "new");
    write(f.stateFrom + "/windows-" + AppPaths::dataDirKey(f.dataFrom) + ".toml", "old");

    const QStringList log = OldName::move(f);

    QCOMPARE(read(f.dataTo + "/settings.toml"), "new");
    QCOMPARE(read(f.dataFrom + "/settings.toml"), "old");
    // The state dir moves, but its files keep the old key: the data dir they belong to stayed.
    QVERIFY(QFileInfo::exists(f.stateTo + "/windows-" + AppPaths::dataDirKey(f.dataFrom) + ".toml"));
    QCOMPARE(log.size(), 1);

    // Nothing left to move the second time.
    QVERIFY(OldName::move(f).isEmpty());
}

void OldNameTest::runningOldCopyKeepsItsDataDir()
{
    qputenv("XDG_DATA_HOME", (m_dir->path() + "/share").toUtf8());
    const QString runtime = m_dir->filePath("runtime");
    QDir().mkpath(runtime);
    QCoreApplication::setApplicationName("lattis");
    QCOMPARE(OldName::folders(runtime).dataTo, m_dir->path() + "/share/lattis");

    QLockFile old(QStringLiteral("%1/game-launcher-%2.lock")
                          .arg(runtime, AppPaths::dataDirKey(m_dir->path() + "/share/game-launcher")));
    QVERIFY(old.tryLock(0));
    const OldName::Folders f = OldName::folders(runtime);
    QVERIFY(f.dataFrom.isEmpty());
    QVERIFY(f.dataTo.isEmpty());
    QVERIFY(!f.stateTo.isEmpty());
}

QTEST_GUILESS_MAIN(OldNameTest)
#include "oldname_test.moc"
