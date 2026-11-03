#include "savesindex.h"
#include "savesmanifest.h"

#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#ifndef SAVES_FIXTURE
#error SAVES_FIXTURE must name tests/data/saves
#endif

namespace {
QByteArray fixture()
{
    QFile file(QStringLiteral(SAVES_FIXTURE "/manifest.yaml"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Answers every request with `body` and an ETag, or with 304 when If-None-Match carries that ETag.
class EtagServer : public QTcpServer
{
public:
    QByteArray body;
    QByteArray etag = "\"v1\"";
    int requests = 0;
    int notModified = 0;

    EtagServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] { answer(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    QString urlString() const { return QStringLiteral("http://127.0.0.1:%1/manifest.yaml").arg(serverPort()); }

private:
    void answer(QTcpSocket *socket)
    {
        const QByteArray request = socket->readAll();
        ++requests;
        QByteArray reply;
        if (request.contains("If-None-Match: " + etag)) {
            ++notModified;
            reply = "HTTP/1.1 304 Not Modified\r\nETag: " + etag + "\r\nConnection: close\r\n\r\n";
        } else {
            reply = "HTTP/1.1 200 OK\r\nETag: " + etag + "\r\nContent-Length: " + QByteArray::number(body.size())
                + "\r\nConnection: close\r\n\r\n" + body;
        }
        socket->write(reply);
        socket->disconnectFromHost();
    }
};
}

class SavesManifestTest : public QObject
{
    Q_OBJECT

    static const SavesGame *find(const SavesIndex &index, const QString &name)
    {
        for (const SavesGame &game : index.games) {
            if (game.name == name)
                return &game;
        }
        return nullptr;
    }

private slots:
    void keepsGamesWithSavesAndTheirIds();
    void keepsSavesNotConfig();
    void keepsOnlyLinuxAndWindowsEntries();
    void keepsConstraints();
    void collectsAliases();
    void leavesOutRegistryAndGamesWithoutFiles();
    void refusesFilesThatAreNotManifests();
    void indexRoundTrips();
    void indexInAnotherFormatIsNotLoaded();
    void updateFromAFileUrl();
    void updateAsksOnlyForWhatChanged();
    void aBadReplyChangesNothing();
    void anOlderIndexIsMadeAgainWithoutTheNetwork();
    void checkIsDueOncePerDay();
};

void SavesManifestTest::keepsGamesWithSavesAndTheirIds()
{
    const Parsed<SavesIndex> parsed = SavesIndexFile::parse(fixture());
    QVERIFY(parsed.ok);
    QVERIFY(parsed.warnings.isEmpty());
    QStringList names;
    for (const SavesGame &game : parsed.value.games)
        names << game.name;
    QCOMPARE(names, (QStringList{"Celeste", "Hades", "Everywhere"}));

    const SavesGame *celeste = find(parsed.value, "Celeste");
    QCOMPARE(celeste->steamId, 504230);
    QCOMPARE(celeste->gogId, 1100000001);
    QCOMPARE(celeste->installDirs, QStringList{"Celeste"});
    const SavesGame *hades = find(parsed.value, "Hades");
    QCOMPARE(hades->steamId, 1145360);
    QCOMPARE(hades->gogId, 0);
    QCOMPARE(hades->installDirs, (QStringList{"Hades", "Hades II"}));
}

void SavesManifestTest::keepsSavesNotConfig()
{
    const SavesIndex index = SavesIndexFile::parse(fixture()).value;
    QStringList celeste;
    for (const SavesPath &p : find(index, "Celeste")->paths)
        celeste << p.path;
    QVERIFY(!celeste.contains("<xdgConfig>/Celeste/settings.celeste"));

    // Tagged both ways counts as a save; no tag at all too.
    QCOMPARE(find(index, "Everywhere")->paths.size(), 1);
    QCOMPARE(find(index, "Hades")->paths.size(), 2);
}

void SavesManifestTest::keepsOnlyLinuxAndWindowsEntries()
{
    const SavesIndex index = SavesIndexFile::parse(fixture()).value;
    QStringList celeste;
    for (const SavesPath &p : find(index, "Celeste")->paths)
        celeste << p.path;
    QCOMPARE(celeste, (QStringList{"<winAppData>/Celeste/Saves", "<xdgData>/Celeste/Saves"}));
    QVERIFY(!find(index, "Menu Only"));
}

void SavesManifestTest::keepsConstraints()
{
    const SavesIndex index = SavesIndexFile::parse(fixture()).value;
    const SavesPath &base = find(index, "Hades")->paths.at(0);
    QCOMPARE(base.path, QStringLiteral("<base>/Saves"));
    QCOMPARE(base.when.size(), 2);
    QCOMPARE(base.when.at(0).os, QStringLiteral("windows"));
    QCOMPARE(base.when.at(0).store, QStringLiteral("steam"));
    QCOMPARE(base.when.at(1).os, QStringLiteral("linux"));
    QVERIFY(base.when.at(1).store.isEmpty());
    QVERIFY(find(index, "Everywhere")->paths.at(0).when.isEmpty());
}

void SavesManifestTest::collectsAliases()
{
    const SavesIndex index = SavesIndexFile::parse(fixture()).value;
    QCOMPARE(find(index, "Hades")->aliases, QStringList{"Hades Demo"});
    QCOMPARE(find(index, "Celeste")->aliases, QStringList{"The Celeste Game"});
    QVERIFY(!find(index, "Hades Demo"));
}

void SavesManifestTest::leavesOutRegistryAndGamesWithoutFiles()
{
    const SavesIndex index = SavesIndexFile::parse(fixture()).value;
    QVERIFY(!find(index, "No Files"));
    for (const SavesPath &p : find(index, "Celeste")->paths)
        QVERIFY(!p.path.startsWith("HKEY"));
}

void SavesManifestTest::refusesFilesThatAreNotManifests()
{
    for (const QByteArray &bad : {QByteArray(), QByteArray("<html><body>404: Not Found</body></html>"),
                                  QByteArray("just a line"), QByteArray("- a\n- list\n"),
                                  QByteArray("Game: [unclosed\n  files: {"), QByteArray("Game:\n  steam:\n    id: 1\n")}) {
        const Parsed<SavesIndex> parsed = SavesIndexFile::parse(bad);
        QVERIFY2(!parsed.ok, bad.constData());
        QVERIFY(!parsed.warnings.isEmpty());
    }
}

void SavesManifestTest::indexRoundTrips()
{
    QTemporaryDir dir;
    const SavesIndex index = SavesIndexFile::parse(fixture()).value;
    const QString path = dir.filePath("sub/index.bin");
    QVERIFY(SavesIndexFile::save(path, index));
    const std::optional<SavesIndex> back = SavesIndexFile::load(path);
    QVERIFY(back);
    QCOMPARE(back->games.size(), index.games.size());
    for (qsizetype i = 0; i < index.games.size(); ++i) {
        const SavesGame &a = index.games.at(i);
        const SavesGame &b = back->games.at(i);
        QCOMPARE(b.name, a.name);
        QCOMPARE(b.aliases, a.aliases);
        QCOMPARE(b.steamId, a.steamId);
        QCOMPARE(b.gogId, a.gogId);
        QCOMPARE(b.installDirs, a.installDirs);
        QCOMPARE(b.paths.size(), a.paths.size());
        for (qsizetype j = 0; j < a.paths.size(); ++j) {
            QCOMPARE(b.paths.at(j).path, a.paths.at(j).path);
            QCOMPARE(b.paths.at(j).when.size(), a.paths.at(j).when.size());
        }
    }
}

void SavesManifestTest::indexInAnotherFormatIsNotLoaded()
{
    QTemporaryDir dir;
    QVERIFY(!SavesIndexFile::load(dir.filePath("missing.bin")));
    QFile file(dir.filePath("junk.bin"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not an index at all");
    file.close();
    QVERIFY(!SavesIndexFile::load(dir.filePath("junk.bin")));

    QVERIFY(SavesIndexFile::save(dir.filePath("index.bin"), SavesIndexFile::parse(fixture()).value));
    QFile index(dir.filePath("index.bin"));
    QVERIFY(index.open(QIODevice::ReadWrite));
    index.seek(4);
    index.write("\xff\xff\xff\xff", 4);
    index.close();
    QVERIFY(!SavesIndexFile::load(dir.filePath("index.bin")));

    QVERIFY(SavesIndexFile::save(dir.filePath("cut.bin"), SavesIndexFile::parse(fixture()).value));
    QFile cut(dir.filePath("cut.bin"));
    QVERIFY(cut.open(QIODevice::ReadWrite));
    cut.resize(cut.size() / 2);
    cut.close();
    QVERIFY(!SavesIndexFile::load(dir.filePath("cut.bin")));
}

void SavesManifestTest::updateFromAFileUrl()
{
    QTemporaryDir dir;
    const QString url = QUrl::fromLocalFile(QStringLiteral(SAVES_FIXTURE "/manifest.yaml")).toString();
    const SavesManifest::UpdateResult result = SavesManifest::update(dir.filePath("m"), url);
    QCOMPARE(result.status, SavesManifest::UpdateResult::Updated);
    QVERIFY(QFile::exists(dir.filePath("m/manifest.yaml")));
    const std::optional<SavesIndex> index = SavesManifest::loadIndex(dir.filePath("m"));
    QVERIFY(index);
    QCOMPARE(index->games.size(), 3);
    const SavesManifest::Meta meta = SavesManifest::readMeta(dir.filePath("m"));
    QVERIFY(meta.fetched.isValid());
    QVERIFY(meta.checked.isValid());
}

void SavesManifestTest::updateAsksOnlyForWhatChanged()
{
    QTemporaryDir dir;
    EtagServer server;
    server.body = fixture();
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QString folder = dir.filePath("m");

    QCOMPARE(SavesManifest::update(folder, server.urlString()).status, SavesManifest::UpdateResult::Updated);
    QCOMPARE(SavesManifest::readMeta(folder).etag, QStringLiteral("\"v1\""));

    QCOMPARE(SavesManifest::update(folder, server.urlString()).status, SavesManifest::UpdateResult::Unchanged);
    QCOMPARE(server.notModified, 1);
    QVERIFY(SavesManifest::loadIndex(folder));

    // Update now downloads again even though nothing changed.
    QCOMPARE(SavesManifest::update(folder, server.urlString(), true).status, SavesManifest::UpdateResult::Updated);
    QCOMPARE(server.notModified, 1);

    server.etag = "\"v2\"";
    QCOMPARE(SavesManifest::update(folder, server.urlString()).status, SavesManifest::UpdateResult::Updated);
    QCOMPARE(SavesManifest::readMeta(folder).etag, QStringLiteral("\"v2\""));
}

void SavesManifestTest::aBadReplyChangesNothing()
{
    QTemporaryDir dir;
    EtagServer server;
    server.body = fixture();
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QString folder = dir.filePath("m");
    QCOMPARE(SavesManifest::update(folder, server.urlString()).status, SavesManifest::UpdateResult::Updated);

    server.body = "<html>rate limited</html>";
    server.etag = "\"v2\"";
    const SavesManifest::UpdateResult result = SavesManifest::update(folder, server.urlString());
    QCOMPARE(result.status, SavesManifest::UpdateResult::Failed);
    QVERIFY(!result.error.isEmpty());
    QCOMPARE(SavesManifest::readMeta(folder).etag, QStringLiteral("\"v1\""));
    const std::optional<SavesIndex> index = SavesManifest::loadIndex(folder);
    QVERIFY(index);
    QCOMPARE(index->games.size(), 3);

    server.close();
    const SavesManifest::UpdateResult offline = SavesManifest::update(folder, server.urlString(), false, 2000);
    QCOMPARE(offline.status, SavesManifest::UpdateResult::Failed);
    QVERIFY(SavesManifest::loadIndex(folder));
}

void SavesManifestTest::anOlderIndexIsMadeAgainWithoutTheNetwork()
{
    QTemporaryDir dir;
    EtagServer server;
    server.body = fixture();
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QString folder = dir.filePath("m");
    QCOMPARE(SavesManifest::update(folder, server.urlString()).status, SavesManifest::UpdateResult::Updated);

    QVERIFY(QFile::remove(folder + "/index.bin"));
    QVERIFY(!SavesManifest::loadIndex(folder));
    const QString gone = server.urlString();
    server.close();
    SavesManifest::update(folder, gone, false, 2000);
    const std::optional<SavesIndex> index = SavesManifest::loadIndex(folder);
    QVERIFY(index);
    QCOMPARE(index->games.size(), 3);
}

void SavesManifestTest::checkIsDueOncePerDay()
{
    const QDateTime now(QDate(2026, 11, 3), QTime(12, 0), QTimeZone::UTC);
    SavesManifest::Meta meta;
    QVERIFY(SavesManifest::checkDue(meta, now));
    meta.checked = now.addSecs(-23 * 3600);
    QVERIFY(!SavesManifest::checkDue(meta, now));
    meta.checked = now.addSecs(-25 * 3600);
    QVERIFY(SavesManifest::checkDue(meta, now));
}

QTEST_MAIN(SavesManifestTest)
#include "savesmanifest_test.moc"
