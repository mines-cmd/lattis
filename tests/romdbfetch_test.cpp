#include "fakehttpserver.h"
#include "romdbfetch.h"
#include "testfiles.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {
const QByteArray kDatabase = "SLUS-20312\tGran Turismo 4\n";
const QByteArray kOld = "SLUS-20001\tOld Game\n";
}

class RomDbFetchTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void keepsAnAnswerUnderTheLimit();
    void stopsABodyOverTheLimit();
    void anErrorAnswerKeepsTheOldDatabase();
    void anEmptyOrErrorPageAnswerIsNotADatabase();
    void aRedirectToHttpIsRefused();
    void aRedirectThatStaysHttpsIsFollowed();
    void aSlowAnswerIsAbortedAtTheTimeout();
    void cancelledBeforeOrDuringTheDownload();
    void anUnknownPlatformAsksNothing();
    void theFixturesModeAsksNothing();
    void aPartialFileThatCantBeWrittenKeepsTheOldDatabase();
    void anOldPartialFileIsReplaced();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    QString path() const { return m_dir->filePath(QStringLiteral("ps2.tsv")); }
};

void RomDbFetchTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    qunsetenv("LATTIS_METADATA_FIXTURES");
    qunsetenv("LATTIS_ROMDB_URL");
}

void RomDbFetchTest::keepsAnAnswerUnderTheLimit()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, kDatabase});
    writeFile(path(), kOld);
    QVERIFY(RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kDatabase);
    QVERIFY(!QFile::exists(path() + ".partial"));
}

void RomDbFetchTest::stopsABodyOverTheLimit()
{
    FakeHttpServer server;
    QByteArray big = kDatabase;
    big += QByteArray(17 * 1024 * 1024, 'x');
    server.on("/ps2.tsv", {200, big});
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QVERIFY(!QFile::exists(path()));
    QVERIFY(!QFile::exists(path() + ".partial"));
}

void RomDbFetchTest::anErrorAnswerKeepsTheOldDatabase()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {404, kDatabase});
    writeFile(path(), kOld);
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kOld);
    QVERIFY(!QFile::exists(path() + ".partial"));

    server.clear("/ps2.tsv");
    server.on("/ps2.tsv", {500, "oops"});
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kOld);
}

void RomDbFetchTest::anEmptyOrErrorPageAnswerIsNotADatabase()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, {}});
    writeFile(path(), kOld);
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kOld);

    // A captive portal or a host's "not found" page, answered with a 200.
    server.clear("/ps2.tsv");
    server.on("/ps2.tsv", {200, "<!DOCTYPE html>\n<html><body><h1>Sign in to the network</h1></body></html>\n"});
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kOld);
    QVERIFY(!QFile::exists(path() + ".partial"));
}

void RomDbFetchTest::aRedirectToHttpIsRefused()
{
    FakeHttpServer plain;
    plain.on("/ps2.tsv", {200, kDatabase});
    FakeHttpServer secure;
    QVERIFY(secure.useTls(QStringLiteral(TLS_FIXTURE)));
    secure.on("/ps2.tsv", {302, {}, 0, "Location: " + plain.url("/ps2.tsv").toEncoded() + "\r\n"});
    writeFile(path(), kOld);

    QVERIFY(!RomDbFetch::download(secure.url("/").toString(), "ps2", path()));
    QCOMPARE(secure.requestsTo("/ps2.tsv").size(), 1);
    QVERIFY(plain.requests.isEmpty());
    QCOMPARE(readFile(path()), kOld);
}

void RomDbFetchTest::aRedirectThatStaysHttpsIsFollowed()
{
    FakeHttpServer secure;
    QVERIFY(secure.useTls(QStringLiteral(TLS_FIXTURE)));
    secure.on("/ps2.tsv", {302, {}, 0, "Location: " + secure.url("/moved/ps2.tsv").toEncoded() + "\r\n"});
    secure.on("/moved/ps2.tsv", {200, kDatabase});
    QVERIFY(RomDbFetch::download(secure.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kDatabase);
}

void RomDbFetchTest::aSlowAnswerIsAbortedAtTheTimeout()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, kDatabase, 3000});
    writeFile(path(), kOld);
    QElapsedTimer clock;
    clock.start();
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path(), 200));
    QVERIFY2(clock.elapsed() < 2000, qPrintable(QString::number(clock.elapsed())));
    QCOMPARE(readFile(path()), kOld);
    QVERIFY(!QFile::exists(path() + ".partial"));
}

void RomDbFetchTest::cancelledBeforeOrDuringTheDownload()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, kDatabase, 3000});

    // Before: nothing is asked.
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path(), 20000, [] { return true; }));
    QVERIFY(server.requests.isEmpty());

    // During: the poll sees the flag and the reply goes, long before its answer or the timeout.
    bool stop = false;
    QTimer::singleShot(150, [&] { stop = true; });
    QElapsedTimer clock;
    clock.start();
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path(), 20000, [&] { return stop; }));
    QVERIFY2(clock.elapsed() < 2000, qPrintable(QString::number(clock.elapsed())));
    QCOMPARE(server.requests.size(), 1);
    QVERIFY(!QFile::exists(path()));
    QVERIFY(!QFile::exists(path() + ".partial"));
}

void RomDbFetchTest::anUnknownPlatformAsksNothing()
{
    FakeHttpServer server;
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "no-such-platform", path()));
    QVERIFY(server.requests.isEmpty());
}

void RomDbFetchTest::theFixturesModeAsksNothing()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, kDatabase});
    qputenv("LATTIS_METADATA_FIXTURES", m_dir->path().toLocal8Bit());
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QVERIFY(server.requests.isEmpty());

    // Unless the test names a database folder too.
    qputenv("LATTIS_ROMDB_URL", server.url("/").toEncoded());
    QVERIFY(RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kDatabase);
}

void RomDbFetchTest::aPartialFileThatCantBeWrittenKeepsTheOldDatabase()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, kDatabase});
    writeFile(path(), kOld);
    QVERIFY(QDir().mkpath(path() + ".partial"));
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kOld);

    // A folder that isn't there: nothing is made.
    const QString missing = m_dir->filePath(QStringLiteral("gone/ps2.tsv"));
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", missing));
    QVERIFY(!QFileInfo::exists(m_dir->filePath(QStringLiteral("gone"))));
}

void RomDbFetchTest::anOldPartialFileIsReplaced()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, kDatabase});
    writeFile(path() + ".partial", "SLUS-1\tleft from a crash, longer than the new one\n");
    QVERIFY(RomDbFetch::download(server.url("/").toString(), "ps2", path()));
    QCOMPARE(readFile(path()), kDatabase);
    QVERIFY(!QFile::exists(path() + ".partial"));
}

QTEST_GUILESS_MAIN(RomDbFetchTest)
#include "romdbfetch_test.moc"
