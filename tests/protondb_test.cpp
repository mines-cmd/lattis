#include "fakehttpserver.h"
#include "metadatahttp.h"
#include "protondb.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
constexpr qint64 kNow = 1'800'000'000;
constexpr qint64 kDay = 24 * 3600;
QByteArray pathFor(qint64 appId)
{
    return "/api/v1/reports/summaries/" + QByteArray::number(appId) + ".json";
}

QByteArray answer(const QString &tier, int total = 10, const QString &trending = {})
{
    return QJsonDocument(QJsonObject{{"tier", tier}, {"total", total}, {"trendingTier", trending.isEmpty() ? tier : trending}})
        .toJson(QJsonDocument::Compact);
}

void writeCache(const QString &dir, const QJsonObject &apps)
{
    QFile file(QDir(dir).filePath(QStringLiteral("protondb.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(QJsonObject{{"apps", apps}}).toJson(QJsonDocument::Compact));
}

QJsonObject cached(const QString &tier, qint64 fetched, int total = 5)
{
    return {{"tier", tier}, {"trending", tier}, {"total", total}, {"fetched", fetched}};
}

QJsonObject cacheApps(const QString &dir)
{
    QFile file(QDir(dir).filePath(QStringLiteral("protondb.json")));
    return file.open(QIODevice::ReadOnly)
        ? QJsonDocument::fromJson(file.readAll()).object().value(QLatin1String("apps")).toObject()
        : QJsonObject();
}

// A ProtonDb on the fake server, with no wait and no spacing between requests.
struct Setup
{
    explicit Setup(const QString &cacheDir) : http(cacheDir, endpoints(server))
    {
        http.setProtonDbInterval(0);
        protonDb = std::make_unique<ProtonDb>(&http, cacheDir);
        protonDb->setWaitMs(0);
        protonDb->setNow(kNow);
    }

    static MetadataEndpoints endpoints(const FakeHttpServer &server)
    {
        MetadataEndpoints points;
        points.protonDb = server.url();
        return points;
    }

    FakeHttpServer server;
    MetadataHttp http;
    std::unique_ptr<ProtonDb> protonDb;
};
}

class ProtonDbTest : public QObject
{
    Q_OBJECT

private slots:
    void tiersFromFixtures_data();
    void tiersFromFixtures();
    void noReportsFromFixture();
    void freshEntryAsksNothing();
    void staleEntryShowsAtOnceAndRefreshes();
    void noReportsAgesOutInADay();
    void failureIsNotCachedAndRetriedOncePerRun();
    void badJsonIsAFailure();
    void waitSkipsGamesPassedOver();
    void oneRequestAtATime();
    void cacheSurvivesARestart();
};

void ProtonDbTest::tiersFromFixtures_data()
{
    QTest::addColumn<qint64>("appId");
    QTest::addColumn<QString>("tier");
    QTest::addColumn<QString>("trending");
    QTest::addColumn<int>("total");
    QTest::newRow("native") << qint64(910001) << "native" << "native" << 120;
    QTest::newRow("platinum") << qint64(1145360) << "platinum" << "platinum" << 759;
    QTest::newRow("gold") << qint64(910002) << "gold" << "silver" << 340;
    QTest::newRow("silver") << qint64(910003) << "silver" << "silver" << 48;
    QTest::newRow("bronze") << qint64(910004) << "bronze" << "bronze" << 12;
    QTest::newRow("borked") << qint64(910005) << "borked" << "borked" << 31;
    QTest::newRow("pending") << qint64(910006) << "pending" << "pending" << 3;
}

void ProtonDbTest::tiersFromFixtures()
{
    QFETCH(qint64, appId);
    QFETCH(QString, tier);
    QFETCH(QString, trending);
    QFETCH(int, total);
    QTemporaryDir cache;
    MetadataHttp http(cache.path());
    http.setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
    http.setFixtureDelay(0);
    ProtonDb protonDb(&http, cache.path());
    protonDb.setWaitMs(0);
    QSignalSpy changed(&protonDb, &ProtonDb::changed);

    QCOMPARE(protonDb.summary(appId).value("state"), "checking");
    QVERIFY(changed.wait(5000));
    QCOMPARE(changed.first().first().toLongLong(), appId);
    const QVariantMap s = protonDb.summary(appId);
    QCOMPARE(s.value("state"), "ready");
    QCOMPARE(s.value("tier"), tier);
    QCOMPARE(s.value("trending"), trending);
    QCOMPARE(s.value("total"), total);
    QVERIFY(!http.hasNetworkManager());
}

void ProtonDbTest::noReportsFromFixture()
{
    QTemporaryDir cache;
    MetadataHttp http(cache.path());
    http.setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
    http.setFixtureDelay(0);
    ProtonDb protonDb(&http, cache.path());
    protonDb.setWaitMs(0);
    QSignalSpy changed(&protonDb, &ProtonDb::changed);

    protonDb.summary(999999999);
    QVERIFY(changed.wait(5000));
    QCOMPARE(protonDb.summary(999999999).value("state"), "none");
    QVERIFY(cacheApps(cache.path()).value("999999999").toObject().value("none").toBool());
    QVERIFY(!http.hasNetworkManager());
}

void ProtonDbTest::freshEntryAsksNothing()
{
    QTemporaryDir cache;
    writeCache(cache.path(), {{"100", cached("gold", kNow - 6 * kDay, 77)}});
    Setup s(cache.path());
    s.server.on(pathFor(100), {200, answer("platinum")});

    const QVariantMap summary = s.protonDb->summary(100);
    QCOMPARE(summary.value("state"), "ready");
    QCOMPARE(summary.value("tier"), "gold");
    QCOMPARE(summary.value("total"), 77);
    QTest::qWait(100);
    QVERIFY(s.server.requests.isEmpty());
}

void ProtonDbTest::staleEntryShowsAtOnceAndRefreshes()
{
    QTemporaryDir cache;
    writeCache(cache.path(), {{"100", cached("gold", kNow - 8 * kDay)}});
    Setup s(cache.path());
    s.server.on(pathFor(100), {200, answer("platinum", 900)});
    QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);

    const QVariantMap before = s.protonDb->summary(100);
    QCOMPARE(before.value("state"), "ready");
    QCOMPARE(before.value("tier"), "gold");
    QVERIFY(changed.wait(5000));
    QCOMPARE(s.server.requestsTo(pathFor(100)).size(), 1);
    const QVariantMap after = s.protonDb->summary(100);
    QCOMPARE(after.value("tier"), "platinum");
    QCOMPARE(after.value("total"), 900);
    QCOMPARE(cacheApps(cache.path()).value("100").toObject().value("tier").toString(), "platinum");
}

void ProtonDbTest::noReportsAgesOutInADay()
{
    QTemporaryDir cache;
    writeCache(cache.path(), {{"1", QJsonObject{{"none", true}, {"fetched", kNow - 12 * 3600}}},
                              {"2", QJsonObject{{"none", true}, {"fetched", kNow - 2 * kDay}}}});
    Setup s(cache.path());
    s.server.on(pathFor(1), {404, "{}"});
    s.server.on(pathFor(2), {200, answer("silver")});
    QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);

    QCOMPARE(s.protonDb->summary(1).value("state"), "none");
    QTest::qWait(100);
    QVERIFY(s.server.requests.isEmpty());

    QCOMPARE(s.protonDb->summary(2).value("state"), "none");
    QVERIFY(changed.wait(5000));
    QCOMPARE(s.protonDb->summary(2).value("tier"), "silver");
}

void ProtonDbTest::failureIsNotCachedAndRetriedOncePerRun()
{
    QTemporaryDir cache;
    Setup s(cache.path());
    s.server.on(pathFor(100), {500, "oops"});
    QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);

    QCOMPARE(s.protonDb->summary(100).value("state"), "checking");
    QVERIFY(changed.wait(5000));
    QCOMPARE(s.protonDb->summary(100).value("state"), "failed");
    QVERIFY(cacheApps(cache.path()).isEmpty());
    QVERIFY(!QFile::exists(QDir(cache.path()).filePath("protondb.json")));

    // The chip re-evaluating on the failure doesn't ask again; selecting the game again does, once.
    QTest::qWait(100);
    QCOMPARE(s.server.requestsTo(pathFor(100)).size(), 1);
    s.protonDb->summary(200);
    QVERIFY(changed.wait(5000));
    changed.clear();
    QCOMPARE(s.protonDb->summary(100).value("state"), "checking");
    QVERIFY(changed.wait(5000));
    QCOMPARE(s.server.requestsTo(pathFor(100)).size(), 2);
    QCOMPARE(s.protonDb->summary(100).value("state"), "failed");

    s.protonDb->summary(200);
    s.protonDb->summary(100);
    QTest::qWait(100);
    QCOMPARE(s.server.requestsTo(pathFor(100)).size(), 2);
    QCOMPARE(s.protonDb->summary(100).value("state"), "failed");

    // A retry that works is cached.
    ProtonDb again(&s.http, cache.path());
    again.setWaitMs(0);
    again.setNow(kNow);
    s.server.clear(pathFor(100));
    s.server.on(pathFor(100), {200, answer("bronze")});
    QSignalSpy againChanged(&again, &ProtonDb::changed);
    again.summary(100);
    QVERIFY(againChanged.wait(5000));
    QCOMPARE(again.summary(100).value("tier"), "bronze");
}

void ProtonDbTest::badJsonIsAFailure()
{
    QTemporaryDir cache;
    Setup s(cache.path());
    s.server.on(pathFor(1), {200, "<html>not json</html>"});
    s.server.on(pathFor(2), {200, R"({"total": 4})"});
    s.server.on(pathFor(3), {200, "[1, 2]"});
    QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);

    for (const qint64 appId : {1, 2, 3}) {
        s.protonDb->summary(appId);
        QVERIFY(changed.wait(5000));
        QCOMPARE(s.protonDb->summary(appId).value("state"), "failed");
    }
    QVERIFY(!QFile::exists(QDir(cache.path()).filePath("protondb.json")));
}

void ProtonDbTest::waitSkipsGamesPassedOver()
{
    QTemporaryDir cache;
    Setup s(cache.path());
    s.protonDb->setWaitMs(150);
    s.server.on(pathFor(1), {200, answer("gold")});
    s.server.on(pathFor(2), {200, answer("silver")});
    s.server.on(pathFor(3), {200, answer("native")});
    QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);

    s.protonDb->summary(1);
    QTest::qWait(50);
    s.protonDb->summary(2);
    QTest::qWait(50);
    s.protonDb->summary(3);
    QVERIFY(changed.wait(5000));
    QCOMPARE(changed.first().first().toLongLong(), 3);
    QTest::qWait(300);
    QCOMPARE(s.server.requests.size(), 1);
    QCOMPARE(s.server.requestsTo(pathFor(3)).size(), 1);
}

void ProtonDbTest::oneRequestAtATime()
{
    QTemporaryDir cache;
    Setup s(cache.path());
    s.server.on(pathFor(1), {200, answer("gold"), 300});
    s.server.on(pathFor(2), {200, answer("silver")});
    QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);

    s.protonDb->summary(1);
    QTest::qWait(100);
    s.protonDb->summary(2);
    QTest::qWait(100);
    QCOMPARE(s.server.requests.size(), 1); // 2 waits for 1's answer
    QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 2, 5000);
    QCOMPARE(s.server.requests.size(), 2);
    // A reply for a game no longer shown still lands in the cache.
    QCOMPARE(s.protonDb->summary(1).value("tier"), "gold");
    QCOMPARE(s.protonDb->summary(2).value("tier"), "silver");
}

void ProtonDbTest::cacheSurvivesARestart()
{
    QTemporaryDir cache;
    {
        Setup s(cache.path());
        s.server.on(pathFor(100), {200, answer("gold", 33, "silver")});
        s.server.on(pathFor(200), {404, "{}"});
        QSignalSpy changed(s.protonDb.get(), &ProtonDb::changed);
        s.protonDb->summary(100);
        QVERIFY(changed.wait(5000));
        s.protonDb->summary(200);
        QVERIFY(changed.wait(5000));
    }
    Setup s(cache.path());
    const QVariantMap a = s.protonDb->summary(100);
    QCOMPARE(a.value("tier"), "gold");
    QCOMPARE(a.value("trending"), "silver");
    QCOMPARE(a.value("total"), 33);
    QCOMPARE(s.protonDb->summary(200).value("state"), "none");
    QTest::qWait(100);
    QVERIFY(s.server.requests.isEmpty());
}

QTEST_GUILESS_MAIN(ProtonDbTest)
#include "protondb_test.moc"
