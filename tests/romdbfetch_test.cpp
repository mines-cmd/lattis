#include "fakehttpserver.h"
#include "romdbfetch.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class RomDbFetchTest : public QObject
{
    Q_OBJECT

private slots:
    void keepsAnAnswerUnderTheLimit();
    void stopsABodyOverTheLimit();
};

void RomDbFetchTest::keepsAnAnswerUnderTheLimit()
{
    FakeHttpServer server;
    server.on("/ps2.tsv", {200, "SLUS-20312\tGran Turismo 4\n"});
    QTemporaryDir dir;
    const QString path = dir.filePath("ps2.tsv");
    QVERIFY(RomDbFetch::download(server.url("/").toString(), "ps2", path));
    QVERIFY(QFile::exists(path));
}

void RomDbFetchTest::stopsABodyOverTheLimit()
{
    FakeHttpServer server;
    QByteArray big = "SLUS-20312\tGran Turismo 4\n";
    big += QByteArray(17 * 1024 * 1024, 'x');
    server.on("/ps2.tsv", {200, big});
    QTemporaryDir dir;
    const QString path = dir.filePath("ps2.tsv");
    QVERIFY(!RomDbFetch::download(server.url("/").toString(), "ps2", path));
    QVERIFY(!QFile::exists(path));
    QVERIFY(!QFile::exists(path + ".partial"));
}

QTEST_GUILESS_MAIN(RomDbFetchTest)
#include "romdbfetch_test.moc"
