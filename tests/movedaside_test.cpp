#include "movedaside.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

class MovedAsideTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void recordedAndRead();
    void unreadableFileIsNone();
    void foundWhileThere();
    void measured();
    void deletedToTheTrash();
    void trashFailureKeepsIt();

private:
    void makeFile(const QString &path, qsizetype bytes = 4);
    // Moves to <scratch>/trash/<n>, as QFile::moveToTrash would to the real trash.
    MovedAside::Trash fakeTrash();

    std::unique_ptr<QTemporaryDir> m_dir;
    QStringList m_trashed;
};

void MovedAsideTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_trashed.clear();
}

void MovedAsideTest::makeFile(const QString &path, qsizetype bytes)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(bytes, 'x'));
}

MovedAside::Trash MovedAsideTest::fakeTrash()
{
    return [this](const QString &path, QString *error) {
        const QString trash = m_dir->filePath(QStringLiteral("trash"));
        QDir().mkpath(trash);
        const QString to = trash + u'/' + QString::number(m_trashed.size());
        if (!QDir().rename(path, to)) {
            *error = QStringLiteral("rename failed");
            return false;
        }
        m_trashed.append(path);
        return true;
    };
}

void MovedAsideTest::recordedAndRead()
{
    const QString file = MovedAside::file(m_dir->filePath(QStringLiteral("state")), QStringLiteral("abc123"));
    QVERIFY(file.endsWith(QStringLiteral("/state/imports-abc123.toml")));
    QVERIFY(MovedAside::read(file).isEmpty());

    MovedAside::Import library;
    library.kind = QStringLiteral("library");
    library.archive = QStringLiteral("/b/lattis-library-2026-09-25.tar.zst");
    library.time = QDateTime(QDate(2026, 9, 30), QTime(14, 5, 7), QTimeZone::fromSecondsAheadOfUtc(7200));
    library.paths = {QStringLiteral("/d/lattis.before-import-2026-09-30")};
    MovedAside::Import saves;
    saves.kind = QStringLiteral("saves");
    saves.archive = QStringLiteral("/b/\"quoted\" saves.tar.zst");
    saves.time = QDateTime(QDate(2026, 9, 30), QTime(15, 0), QTimeZone::UTC);
    saves.paths = {QStringLiteral("/h/.config/Ryujinx/bis/user/save.before-import-2026-09-30"),
                   QStringLiteral("/s/steamapps/compatdata.before-import-2026-09-30/3000000001")};
    saves.notice = false;
    QVERIFY(MovedAside::record(file, library));
    QVERIFY(MovedAside::record(file, saves));

    const QList<MovedAside::Import> read = MovedAside::read(file);
    QCOMPARE(read.size(), 2);
    QCOMPARE(read[0].kind, library.kind);
    QCOMPARE(read[0].archive, library.archive);
    QCOMPARE(read[0].time, library.time);
    QCOMPARE(read[0].paths, library.paths);
    QVERIFY(read[0].notice);
    QCOMPARE(read[1].archive, saves.archive);
    QCOMPARE(read[1].time, saves.time);
    QCOMPARE(read[1].paths, saves.paths);
    QVERIFY(!read[1].notice);

    // Written again without the first.
    QVERIFY(MovedAside::write(file, {read[1]}));
    QCOMPARE(MovedAside::read(file).size(), 1);
    QVERIFY(MovedAside::write(file, {}));
    QVERIFY(MovedAside::read(file).isEmpty());
}

void MovedAsideTest::unreadableFileIsNone()
{
    const QString file = m_dir->filePath(QStringLiteral("imports-x.toml"));
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[[import]\nkind = ");
    f.close();
    QVERIFY(MovedAside::read(file).isEmpty());
}

void MovedAsideTest::foundWhileThere()
{
    const QString there = m_dir->filePath(QStringLiteral("save.before-import-2026-09-30"));
    const QString file = m_dir->filePath(QStringLiteral("rom.sav.before-import-2026-09-30"));
    const QString gone = m_dir->filePath(QStringLiteral("keys.before-import-2026-09-30"));
    QVERIFY(QDir().mkpath(there + QStringLiteral("/0000")));
    makeFile(file);
    // A dangling link is something there to delete, too.
    const QString link = m_dir->filePath(QStringLiteral("link.before-import-2026-09-30"));
    QVERIFY(QFile::link(m_dir->filePath(QStringLiteral("nowhere")), link));

    MovedAside::Import some;
    some.paths = {there, gone, file, link};
    MovedAside::Import allGone;
    allGone.paths = {gone};
    MovedAside::Import nothingMoved; // nothing was there: only the notice
    MovedAside::Import nothingMovedSeen;
    nothingMovedSeen.notice = false;

    const QList<MovedAside::Import> found = MovedAside::present({some, allGone, nothingMoved, nothingMovedSeen});
    QCOMPARE(found.size(), 2);
    QCOMPARE(found[0].paths, (QStringList{there, file, link}));
    QVERIFY(found[1].paths.isEmpty());
    QVERIFY(found[1].notice);
}

void MovedAsideTest::measured()
{
    const QString folder = m_dir->filePath(QStringLiteral("save.before-import-2026-09-30"));
    makeFile(folder + QStringLiteral("/a"), 100);
    makeFile(folder + QStringLiteral("/sub/.hidden"), 20);
    QVERIFY(QFile::link(folder + QStringLiteral("/a"), folder + QStringLiteral("/link")));
    const QString file = m_dir->filePath(QStringLiteral("rom.sav.before-import-2026-09-30"));
    makeFile(file, 3);
    QCOMPARE(MovedAside::bytes({folder, file, m_dir->filePath(QStringLiteral("gone"))}), 123);
}

void MovedAsideTest::deletedToTheTrash()
{
    const QString save = m_dir->filePath(QStringLiteral("save.before-import-2026-09-30"));
    makeFile(save + QStringLiteral("/0000/file"));
    const QString romSave = m_dir->filePath(QStringLiteral("roms/Game.sav.before-import-2026-09-30"));
    makeFile(romSave);
    // Two Proton prefixes moved aside into one folder next to compatdata/.
    const QString aside = m_dir->filePath(QStringLiteral("steamapps/compatdata.before-import-2026-09-30"));
    makeFile(aside + QStringLiteral("/3000000001/pfx/user.reg"));
    makeFile(aside + QStringLiteral("/3000000002/pfx/user.reg"));

    // One prefix: its folder stays for the other.
    QCOMPARE(MovedAside::remove({aside + QStringLiteral("/3000000001")}, fakeTrash()), QString());
    QVERIFY(QFileInfo::exists(aside));
    // The rest: the emptied folder goes, the ROM folder stays.
    const QString gone = m_dir->filePath(QStringLiteral("gone.before-import-2026-09-30"));
    QCOMPARE(MovedAside::remove({save, romSave, aside + QStringLiteral("/3000000002"), gone}, fakeTrash()), QString());
    QCOMPARE(m_trashed, (QStringList{aside + QStringLiteral("/3000000001"), save, romSave,
                                     aside + QStringLiteral("/3000000002")}));
    QVERIFY(!QFileInfo::exists(save));
    QVERIFY(!QFileInfo::exists(romSave));
    QVERIFY(!QFileInfo::exists(aside));
    QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("roms"))));
    QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("steamapps"))));
}

void MovedAsideTest::trashFailureKeepsIt()
{
    const QString save = m_dir->filePath(QStringLiteral("save.before-import-2026-09-30"));
    const QString keys = m_dir->filePath(QStringLiteral("keys.before-import-2026-09-30"));
    makeFile(save + QStringLiteral("/file"));
    makeFile(keys + QStringLiteral("/prod.keys"));
    const MovedAside::Trash refuse = [](const QString &path, QString *error) {
        if (path.contains(QStringLiteral("keys"))) {
            *error = QStringLiteral("no trash");
            return false;
        }
        return QDir(path).removeRecursively();
    };
    const QString error = MovedAside::remove({save, keys}, refuse);
    QVERIFY(error.contains(keys));
    QVERIFY(error.contains(QStringLiteral("no trash")));
    QVERIFY(!QFileInfo::exists(save));
    QVERIFY(QFileInfo::exists(keys + QStringLiteral("/prod.keys")));
    QCOMPARE(MovedAside::present({MovedAside::Import{{}, {}, {}, {save, keys}, false}}).value(0).paths, QStringList{keys});
}

QTEST_GUILESS_MAIN(MovedAsideTest)
#include "movedaside_test.moc"
