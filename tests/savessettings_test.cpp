#include "savessettings.h"
#include "fakehttpserver.h"
#include "testfiles.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

#ifndef SAVES_FIXTURE
#error SAVES_FIXTURE must name tests/data/saves
#endif

class SavesSettingsTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<FakeHttpServer> m_server;
    QString m_data;
    QString m_manifest;

    QString settingsFile() const { return m_data + QStringLiteral("/settings.toml"); }

    SavesSettings::Setup setup() const { return {m_data, m_manifest}; }

    static QByteArray manifest() { return readFile(QStringLiteral(SAVES_FIXTURE "/manifest.yaml")); }

    // Starts a download and waits for it to end.
    static void finished(SavesSettings &settings, const std::function<void()> &start)
    {
        QSignalSpy updating(&settings, &SavesSettings::updatingChanged);
        start();
        QCOMPARE(updating.size(), 1); // started at once; the end comes through the event loop
        QVERIFY(updating.wait(10000));
        QVERIFY(!settings.updating());
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_data = m_dir->filePath(QStringLiteral("data"));
        m_manifest = m_dir->filePath(QStringLiteral("manifest"));
        QVERIFY(QDir().mkpath(m_data));
        m_server = std::make_unique<FakeHttpServer>();
        qputenv("LATTIS_SAVES_MANIFEST_URL", m_server->url(QStringLiteral("/manifest.yaml")).toString().toUtf8());
    }

    void cleanup()
    {
        qunsetenv("LATTIS_SAVES_MANIFEST_URL");
        m_server.reset();
        m_dir.reset();
    }

    void aFailedDownloadSaysWhyAndKeepsNothing()
    {
        m_server->on("/manifest.yaml", {500, "broken"});
        SavesSettings settings(setup());
        QCOMPARE(settings.status(), QStringLiteral("Not downloaded yet"));

        finished(settings, [&] { settings.ensureManifest(); });
        QVERIFY2(settings.status().startsWith(QStringLiteral("Couldn't download: ")), qPrintable(settings.status()));
        QVERIFY(!settings.downloaded());
        QVERIFY(!QFileInfo::exists(m_manifest + QStringLiteral("/manifest.yaml")));
        QVERIFY(!QFileInfo::exists(m_manifest + QStringLiteral("/index.bin")));

        // An answer that isn't a manifest changes nothing either.
        m_server->clear("/manifest.yaml");
        m_server->on("/manifest.yaml", {200, "- just\n- a list\n"});
        finished(settings, [&] { settings.update(); });
        QVERIFY2(settings.status().startsWith(QStringLiteral("Couldn't download: ")), qPrintable(settings.status()));
        QVERIFY(!settings.downloaded());
        QVERIFY(!QFileInfo::exists(m_manifest + QStringLiteral("/manifest.yaml")));

        m_server->clear("/manifest.yaml");
        m_server->on("/manifest.yaml", {200, ""});
        finished(settings, [&] { settings.update(); });
        QCOMPARE(settings.status(), QStringLiteral("Couldn't download: the reply was empty"));
    }

    void ensureManifestFetchesOnlyWhenThereIsNone()
    {
        m_server->on("/manifest.yaml", {200, manifest(), 200, "ETag: \"v1\"\r\n"});
        SavesSettings settings(setup());
        QSignalSpy state(&settings, &SavesSettings::stateChanged);

        finished(settings, [&] {
            settings.ensureManifest();
            QCOMPARE(settings.status(), QStringLiteral("Downloading"));
            // Pressed again while it runs: one download.
            settings.ensureManifest();
            settings.update();
        });
        QCOMPARE(m_server->requestsTo("/manifest.yaml").size(), 1);
        QVERIFY(settings.downloaded());
        QVERIFY2(settings.status().startsWith(QStringLiteral("Downloaded ")), qPrintable(settings.status()));
        QCOMPARE(readFile(m_manifest + QStringLiteral("/manifest.yaml")), manifest());
        QVERIFY(QFileInfo::exists(m_manifest + QStringLiteral("/index.bin")));
        QVERIFY(state.size() >= 2);

        settings.ensureManifest();
        QVERIFY(!settings.updating());
        QCOMPARE(m_server->requestsTo("/manifest.yaml").size(), 1);

        // A second instance reads what's on disk.
        SavesSettings again(setup());
        QVERIFY(again.downloaded());
        again.ensureManifest();
        QVERIFY(!again.updating());
    }

    // Update is the user's "check now": it fetches the whole file, not only when the ETag changed.
    void updateFetchesAgainWithoutTheETag()
    {
        m_server->on("/manifest.yaml", {200, manifest(), 0, "ETag: \"v1\"\r\n"});
        SavesSettings settings(setup());
        finished(settings, [&] { settings.ensureManifest(); });
        QCOMPARE(m_server->requestsTo("/manifest.yaml").size(), 1);

        // The server goes wrong: the status says so, and the manifest already there stays.
        m_server->clear("/manifest.yaml");
        m_server->on("/manifest.yaml", {404, "gone"});
        finished(settings, [&] { settings.update(); });
        const QList<FakeHttpServer::Request> requests = m_server->requestsTo("/manifest.yaml");
        QCOMPARE(requests.size(), 2);
        QVERIFY(!requests.last().headers.contains("if-none-match"));
        QVERIFY(settings.status().startsWith(QStringLiteral("Couldn't download: ")));
        QVERIFY(settings.downloaded());
        QCOMPARE(readFile(m_manifest + QStringLiteral("/manifest.yaml")), manifest());

        // The next good answer clears the error.
        m_server->clear("/manifest.yaml");
        m_server->on("/manifest.yaml", {200, manifest(), 0, "ETag: \"v2\"\r\n"});
        finished(settings, [&] { settings.update(); });
        QVERIFY2(settings.status().startsWith(QStringLiteral("Downloaded ")), qPrintable(settings.status()));
    }

    void theSettingsFileIsReadAtStart()
    {
        writeFile(settingsFile(), "[backup]\nauto_folder = \"~/Saves\"\nauto_keep = 12\n");
        SavesSettings settings(setup());
        QCOMPARE(settings.folder(), QStringLiteral("~/Saves"));
        QCOMPARE(settings.keep(), 12);

        writeFile(settingsFile(), "[backup]\nauto_keep = \"many\"\n");
        settings.refresh();
        QCOMPARE(settings.folder(), settings.defaultFolder());
        QCOMPARE(settings.keep(), 5);
    }

    void setFolderAndSetKeepWriteTheSettingsFile()
    {
        writeFile(settingsFile(), "# mine\n[general]\nsession_minutes = 3\n");
        SavesSettings settings(setup());
        QCOMPARE(settings.folder(), QStringLiteral("~/.local/share/lattis-saves"));
        QSignalSpy changed(&settings, &SavesSettings::settingsChanged);

        QVERIFY(settings.setFolder(QStringLiteral("  ~/Game \"Saves\"  ")).isEmpty());
        QCOMPARE(settings.folder(), QStringLiteral("~/Game \"Saves\""));
        QVERIFY(readFile(settingsFile()).contains("auto_folder = \"~/Game \\\"Saves\\\"\""));
        QVERIFY(readFile(settingsFile()).startsWith("# mine\n[general]\nsession_minutes = 3\n"));
        QCOMPARE(changed.size(), 1);

        QVERIFY(settings.setFolder(QStringLiteral("   ")).isEmpty());
        QCOMPARE(settings.folder(), settings.defaultFolder());
        QVERIFY(!readFile(settingsFile()).contains("auto_folder"));

        const QByteArray before = readFile(settingsFile());
        QCOMPARE(settings.setKeep(0), QStringLiteral("Keep between 1 and 100 backups"));
        QCOMPARE(settings.setKeep(101), QStringLiteral("Keep between 1 and 100 backups"));
        QCOMPARE(readFile(settingsFile()), before);
        QCOMPARE(settings.keep(), 5);

        QVERIFY(settings.setKeep(100).isEmpty());
        QCOMPARE(settings.keep(), 100);
        QVERIFY(readFile(settingsFile()).contains("auto_keep = 100"));
        // The default is no line at all.
        QVERIFY(settings.setKeep(5).isEmpty());
        QCOMPARE(settings.keep(), 5);
        QVERIFY(!readFile(settingsFile()).contains("auto_keep"));
    }

    void aSettingsFileThatCantBeWrittenSaysSo()
    {
        if (::geteuid() == 0)
            QSKIP("root writes anywhere");
        SavesSettings settings(setup());
        QVERIFY(QFile::setPermissions(m_data, QFile::ReadOwner | QFile::ExeOwner));
        const auto restore = qScopeGuard([&] { QFile::setPermissions(m_data, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
        QSignalSpy changed(&settings, &SavesSettings::settingsChanged);

        const QString folder = settings.setFolder(QStringLiteral("~/Saves"));
        QVERIFY2(folder.startsWith(QStringLiteral("Couldn't save auto_folder in settings.toml: ")), qPrintable(folder));
        QCOMPARE(settings.folder(), settings.defaultFolder());
        const QString keep = settings.setKeep(3);
        QVERIFY2(keep.startsWith(QStringLiteral("Couldn't save auto_keep in settings.toml: ")), qPrintable(keep));
        QCOMPARE(settings.keep(), 5);
        QVERIFY(changed.isEmpty());
        QVERIFY(!QFileInfo::exists(settingsFile()));
    }
};

QTEST_MAIN(SavesSettingsTest)
#include "savessettings_test.moc"
