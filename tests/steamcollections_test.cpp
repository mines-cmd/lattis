#include "steamcollections.h"

#include <QFile>
#include <QTest>

class SteamCollectionsTest : public QObject
{
    Q_OBJECT

private slots:
    void readsTheFixture();
    void aFileThatIsNotAList();
    void emptyFile();
    void path();
};

namespace {
QByteArray fixture()
{
    QFile file(QStringLiteral(STEAM_COLLECTIONS_DATA_DIR "/cloud-storage-namespace-1.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

const SteamCollection *find(const SteamCollections &all, const QString &steamId)
{
    for (const SteamCollection &c : all.collections) {
        if (c.steamId == steamId)
            return &c;
    }
    return nullptr;
}
}

void SteamCollectionsTest::readsTheFixture()
{
    const QByteArray bytes = fixture();
    QVERIFY(!bytes.isEmpty());
    const SteamCollections all = parseSteamCollections(bytes);
    QVERIFY(all.ok);

    // Hand-picked, a tag's, and the empty one, in the file's order; a name is trimmed, a repeated app counted once.
    QStringList ids;
    for (const SteamCollection &c : all.collections)
        ids.append(c.steamId);
    QCOMPARE(ids, (QStringList{"uc-AbCdEf1", "uc-GhIjKl2", "uc-Empty03", "from-tag-Roguelike", "uc-Mixed001"}));
    QCOMPARE(find(all, "uc-AbCdEf1")->name, QStringLiteral("Weekend Games"));
    QCOMPARE(find(all, "uc-AbCdEf1")->appIds, (QList<qint64>{1145360, 367520, 999001}));
    QCOMPARE(find(all, "uc-GhIjKl2")->name, QStringLiteral("Couch Co-op"));
    QCOMPARE(find(all, "uc-GhIjKl2")->appIds, (QList<qint64>{413150, 1091500}));
    QVERIFY(find(all, "uc-Empty03")->appIds.isEmpty());
    QCOMPARE(find(all, "from-tag-Roguelike")->name, QStringLiteral("Roguelike"));

    // Dynamic ones are counted, whatever they list; favorite, hidden and deleted ones are left out quietly.
    QCOMPARE(all.dynamic, 2);
    QVERIFY(!find(all, "favorite"));
    QVERIFY(!find(all, "hidden"));
    QVERIFY(!find(all, "uc-Deleted6"));
    QVERIFY(!find(all, "uc-Dyn0004"));

    // A list with entries that aren't app ids keeps the ones that are.
    QCOMPARE(find(all, "uc-Mixed001")->appIds, (QList<qint64>{1245620, 1462040}));

    // Each unreadable one is reported by its id, and none stops the others.
    const QString problems = all.problems.join('\n');
    QVERIFY(problems.contains("uc-Broken07"));
    QVERIFY(problems.contains("uc-NoValue8"));
    QVERIFY(problems.contains("uc-NoName09"));
    QVERIFY(problems.contains("uc-BadList0"));
    QVERIFY(problems.contains("uc-Mixed001"));
    QVERIFY(problems.contains("uc-NotObj02"));
    QCOMPARE(all.problems.size(), 6);
    QVERIFY(!find(all, "uc-Broken07"));
    QVERIFY(!find(all, "uc-NoValue8"));
    QVERIFY(!find(all, "uc-NoName09"));
    QVERIFY(!find(all, "uc-BadList0"));
    QVERIFY(!find(all, "uc-NotObj02"));
}

void SteamCollectionsTest::aFileThatIsNotAList()
{
    for (const QByteArray &bytes : {QByteArray("{\"a\":1}"), QByteArray("[1,"), QByteArray("garbage")}) {
        const SteamCollections all = parseSteamCollections(bytes);
        QVERIFY(!all.ok);
        QVERIFY(all.collections.isEmpty());
        QCOMPARE(all.problems.size(), 1);
    }
}

void SteamCollectionsTest::emptyFile()
{
    QVERIFY(!parseSteamCollections({}).ok);
    const SteamCollections none = parseSteamCollections("[]");
    QVERIFY(none.ok);
    QVERIFY(none.collections.isEmpty());
    QVERIFY(none.problems.isEmpty());
}

void SteamCollectionsTest::path()
{
    QCOMPARE(steamCollectionsPath(QStringLiteral("/s"), QStringLiteral("42")),
             QStringLiteral("/s/userdata/42/config/cloudstorage/cloud-storage-namespace-1.json"));
}

QTEST_APPLESS_MAIN(SteamCollectionsTest)
#include "steamcollections_test.moc"
