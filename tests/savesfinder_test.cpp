#include "savesfinder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#ifndef SAVES_FIXTURE
#error SAVES_FIXTURE must name tests/data/saves
#endif

using namespace SavesFinder;

namespace {
SavesIndex fixtureIndex()
{
    QFile file(QStringLiteral(SAVES_FIXTURE "/manifest.yaml"));
    return file.open(QIODevice::ReadOnly) ? SavesIndexFile::parse(file.readAll()).value : SavesIndex();
}

SavesGame game(const QString &name, const QStringList &paths, int steamId = 0)
{
    SavesGame g;
    g.name = name;
    g.steamId = steamId;
    for (const QString &p : paths)
        g.paths.append({p, {}});
    return g;
}

void touch(const QString &path)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
}
}

class SavesFinderTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    Env m_env;
    QString m_prefix;

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QDir(m_dir.path()).removeRecursively();
        QDir().mkpath(m_dir.path());
        m_env.home = m_dir.filePath("home");
        m_env.xdgData = m_env.home + "/.local/share";
        m_env.xdgConfig = m_env.home + "/.config";
        m_env.user = "someone";
        m_prefix = m_dir.filePath("compatdata/77/pfx");
        QDir().mkpath(m_env.home);
        QDir().mkpath(m_prefix + "/drive_c/users/steamuser");
    }

    void matchesBySteamIdThenTitle();
    void titleMatchFoldsCaseAndPunctuation();
    void aliasMatchesButNeverReplacesAName();
    void chosenGameWinsAndAWrongOneMatchesNothing();
    void linuxGameExpandsLinuxPlaceholders();
    void linuxGameSkipsWindowsOnlyEntries();
    void protonGameExpandsIntoItsPrefix();
    void protonPrefixFindsPathsWhateverTheirCase();
    void wineUserPicksTheUserFolder();
    void storeFolderComesBeforeTheManifest();
    void storeFolderStaysInsideItsPrefix();
    void missingPathsAreSkipped();
    void globsExpandWithoutFollowingLinks();
    void doubleStarMatchesZeroOrMoreFolders();
    void parentTraversalAndBroadPathsAreRefused();
    void ownPathsStandAloneAndAddToAMatch();
    void copiesAreOneGame();
    void noMatchNoPaths();
    void anEpicSaveFolderThatGoesUpIsRefused();
};

void SavesFinderTest::matchesBySteamIdThenTitle()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    Query q;
    q.steamAppId = 1145360;
    q.title = "Something else entirely";
    Match how;
    QCOMPARE(matcher.find(q, &how)->name, QStringLiteral("Hades"));
    QCOMPARE(how, Match::SteamId);

    q.steamAppId = 0;
    q.title = "Celeste";
    QCOMPARE(matcher.find(q, &how)->name, QStringLiteral("Celeste"));
    QCOMPARE(how, Match::Title);

    q.steamAppId = 424242; // not in the manifest: the title is tried
    QCOMPARE(matcher.find(q)->name, QStringLiteral("Celeste"));
}

void SavesFinderTest::titleMatchFoldsCaseAndPunctuation()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    Query q;
    q.title = "  hADES!  ";
    QVERIFY(matcher.find(q));
    q.title = "Hades II";
    QVERIFY(!matcher.find(q));
    q.title = "";
    QVERIFY(!matcher.find(q));
}

void SavesFinderTest::aliasMatchesButNeverReplacesAName()
{
    SavesIndex index;
    index.games << game("Alpha", {"<home>/a"}) << game("Beta", {"<home>/b"});
    index.games[0].aliases = {"Beta", "Alpha Remaster"};
    const Matcher matcher(index);
    Query q;
    q.title = "Alpha Remaster";
    QCOMPARE(matcher.find(q)->name, QStringLiteral("Alpha"));
    q.title = "Beta";
    QCOMPARE(matcher.find(q)->name, QStringLiteral("Beta"));
}

void SavesFinderTest::chosenGameWinsAndAWrongOneMatchesNothing()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    Query q;
    q.title = "Celeste";
    q.steamAppId = 504230;
    q.savesGame = "Hades";
    Match how;
    QCOMPARE(matcher.find(q, &how)->name, QStringLiteral("Hades"));
    QCOMPARE(how, Match::Chosen);

    q.savesGame = "Not In The Manifest";
    QVERIFY(!matcher.find(q, &how));
    QCOMPARE(how, Match::None);
}

void SavesFinderTest::linuxGameExpandsLinuxPlaceholders()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    touch(m_env.xdgData + "/Celeste/Saves/0.celeste");
    touch(m_env.home + "/.everywhere/save.dat");
    touch(m_dir.filePath("games/Hades/Saves/slot1.sav"));

    Query celeste;
    celeste.id = "linux-celeste";
    celeste.title = "Celeste";
    const Found a = find(matcher, {celeste}, m_env);
    QCOMPARE(a.matched, QStringLiteral("Celeste"));
    QCOMPARE(a.paths, QStringList{m_env.xdgData + "/Celeste/Saves"});

    Query hades;
    hades.title = "Hades";
    hades.baseDir = m_dir.filePath("games/Hades");
    QCOMPARE(find(matcher, {hades}, m_env).paths, QStringList{m_dir.filePath("games/Hades/Saves")});

    Query every;
    every.title = "Everywhere";
    QCOMPARE(find(matcher, {every}, m_env).paths, QStringList{m_env.home + "/.everywhere"});
}

void SavesFinderTest::linuxGameSkipsWindowsOnlyEntries()
{
    SavesIndex index;
    index.games << game("Win", {"<winAppData>/Win"}) << game("Mixed", {"<xdgConfig>/mixed", "<winDocuments>/mixed"});
    index.games[0].paths[0].when = {{"windows", ""}};
    const Matcher matcher(index);
    touch(m_env.xdgConfig + "/mixed/a");
    touch(m_env.home + "/Win/a");

    Query win;
    win.title = "Win";
    QVERIFY(find(matcher, {win}, m_env).paths.isEmpty());
    Query mixed;
    mixed.title = "Mixed";
    QCOMPARE(find(matcher, {mixed}, m_env).paths, QStringList{m_env.xdgConfig + "/mixed"});
}

void SavesFinderTest::protonGameExpandsIntoItsPrefix()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    const QString user = m_prefix + "/drive_c/users/steamuser";
    touch(user + "/AppData/Roaming/Celeste/Saves/0.celeste");
    touch(user + "/Documents/Saved Games/Hades/Profile1.sav");
    touch(m_env.xdgData + "/Celeste/Saves/native.celeste"); // a Linux entry; not for a Proton game

    Query celeste;
    celeste.title = "Celeste";
    celeste.prefix = m_prefix;
    QCOMPARE(find(matcher, {celeste}, m_env).paths, QStringList{user + "/AppData/Roaming/Celeste/Saves"});

    Query hades;
    hades.steamAppId = 1145360;
    hades.prefix = m_prefix;
    QCOMPARE(find(matcher, {hades}, m_env).paths, QStringList{user + "/Documents/Saved Games/Hades"});
}

void SavesFinderTest::protonPrefixFindsPathsWhateverTheirCase()
{
    SavesIndex index;
    index.games << game("Cased", {"<winDocuments>/My Games/Cased/*.sav"});
    const Matcher matcher(index);
    touch(m_prefix + "/drive_c/users/steamuser/documents/my games/cased/A.SAV");

    Query q;
    q.title = "Cased";
    q.prefix = m_prefix;
    QCOMPARE(find(matcher, {q}, m_env).paths, QStringList{m_prefix + "/drive_c/users/steamuser/documents/my games/cased/A.SAV"});

    Query native;
    native.title = "Cased";
    QVERIFY(find(matcher, {native}, m_env).paths.isEmpty());
}

void SavesFinderTest::missingPathsAreSkipped()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    Query q;
    q.title = "Celeste";
    const Found found = find(matcher, {q}, m_env);
    QCOMPARE(found.matched, QStringLiteral("Celeste"));
    QVERIFY(found.paths.isEmpty());
}

void SavesFinderTest::globsExpandWithoutFollowingLinks()
{
    SavesIndex index;
    index.games << game("Globby", {"<home>/saves/*/slot?.sav", "<home>/saves/*/profile"});
    const Matcher matcher(index);
    touch(m_env.home + "/saves/one/slot1.sav");
    touch(m_env.home + "/saves/one/slot22.sav");
    touch(m_env.home + "/saves/two/slot2.sav");
    touch(m_dir.filePath("outside/slot9.sav"));
    QVERIFY(QFile::link(m_dir.filePath("outside"), m_env.home + "/saves/linked"));
    touch(m_env.home + "/saves/three/profile/a");

    Query q;
    q.title = "Globby";
    QCOMPARE(find(matcher, {q}, m_env).paths,
             (QStringList{m_env.home + "/saves/one/slot1.sav", m_env.home + "/saves/three/profile",
                          m_env.home + "/saves/two/slot2.sav"}));
}

void SavesFinderTest::doubleStarMatchesZeroOrMoreFolders()
{
    SavesIndex index;
    index.games << game("Deep", {"<home>/saves/*/**/*.sav"});
    const Matcher matcher(index);
    touch(m_env.home + "/saves/id1/a.sav");
    touch(m_env.home + "/saves/id1/sub/deeper/b.sav");
    touch(m_env.home + "/saves/id1/c.txt");

    Query q;
    q.title = "Deep";
    QCOMPARE(find(matcher, {q}, m_env).paths,
             (QStringList{m_env.home + "/saves/id1/a.sav", m_env.home + "/saves/id1/sub/deeper/b.sav"}));
}

void SavesFinderTest::parentTraversalAndBroadPathsAreRefused()
{
    SavesIndex index;
    index.games << game("Sneaky", {"<home>/../outside", "<home>", "<xdgData>", "<home>/ok"});
    const Matcher matcher(index);
    touch(m_dir.filePath("outside/a"));
    touch(m_env.home + "/ok/a");
    touch(m_env.xdgData + "/a");

    Query q;
    q.title = "Sneaky";
    QCOMPARE(find(matcher, {q}, m_env).paths, QStringList{m_env.home + "/ok"});

    Query own;
    own.savePaths = {"~", "~/ok/../../outside"};
    QVERIFY(find(matcher, {own}, m_env).paths.isEmpty());
}

void SavesFinderTest::ownPathsStandAloneAndAddToAMatch()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    touch(m_env.home + "/mysaves/a.sav");
    touch(m_env.home + "/more/b.sav");
    touch(m_env.home + "/more/c.sav");

    Query own;
    own.id = "linux-unknown";
    own.title = "A Game Nobody Listed";
    own.savePaths = {"~/mysaves", "~/more/*.sav", "~/nowhere"};
    Found found = find(matcher, {own}, m_env);
    QVERIFY(found.matched.isEmpty());
    QCOMPARE(found.match, Match::None);
    QCOMPARE(found.paths, (QStringList{m_env.home + "/more/b.sav", m_env.home + "/more/c.sav", m_env.home + "/mysaves"}));

    touch(m_env.xdgData + "/Celeste/Saves/0.celeste");
    own.title = "Celeste";
    found = find(matcher, {own}, m_env);
    QCOMPARE(found.matched, QStringLiteral("Celeste"));
    QCOMPARE(found.paths.size(), 4);
    QVERIFY(found.paths.contains(m_env.xdgData + "/Celeste/Saves"));

    // An own path inside a found folder isn't taken twice.
    own.savePaths = {"~/.local/share/Celeste/Saves/0.celeste"};
    QCOMPARE(find(matcher, {own}, m_env).paths, QStringList{m_env.xdgData + "/Celeste/Saves"});
}

void SavesFinderTest::copiesAreOneGame()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    const QString user = m_prefix + "/drive_c/users/steamuser";
    touch(user + "/AppData/Roaming/Celeste/Saves/0.celeste");
    touch(m_env.xdgData + "/Celeste/Saves/0.celeste");

    Query steam;
    steam.id = "steam-504230";
    steam.steamAppId = 504230;
    steam.title = "Celeste";
    steam.prefix = m_prefix;
    Query native;
    native.id = "linux-celeste";
    native.title = "Celeste";

    const Found found = find(matcher, {steam, native}, m_env);
    QCOMPARE(found.gameId, QStringLiteral("steam-504230"));
    QCOMPARE(found.matched, QStringLiteral("Celeste"));
    QCOMPARE(found.match, Match::SteamId);
    QCOMPARE(found.paths, (QStringList{user + "/AppData/Roaming/Celeste/Saves", m_env.xdgData + "/Celeste/Saves"}));

    QVERIFY(find(matcher, {}, m_env).gameId.isEmpty());
}

void SavesFinderTest::noMatchNoPaths()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    Query q;
    q.title = "Nothing Like It";
    const Found found = find(matcher, {q}, m_env);
    QVERIFY(found.matched.isEmpty());
    QVERIFY(found.paths.isEmpty());
}

void SavesFinderTest::wineUserPicksTheUserFolder()
{
    SavesIndex index;
    index.games << game("Wined", {"<winAppData>/Wined", "<home>/Documents/Wined", "<winDrive>/users/<osUserName>/Wined"});
    const Matcher matcher(index);
    const QString steam = m_prefix + "/drive_c/users/steamuser";
    const QString real = m_prefix + "/drive_c/users/someone";
    touch(steam + "/AppData/Roaming/Wined/steam.sav");
    touch(steam + "/Documents/Wined/steam.sav");
    touch(real + "/AppData/Roaming/Wined/real.sav");
    touch(real + "/Documents/Wined/real.sav");

    Query q;
    q.title = "Wined";
    q.prefix = m_prefix;
    QCOMPARE(find(matcher, {q}, m_env).paths,
             (QStringList{steam + "/AppData/Roaming/Wined", steam + "/Documents/Wined"}));

    q.wineUser = "someone";
    QCOMPARE(find(matcher, {q}, m_env).paths,
             (QStringList{real + "/AppData/Roaming/Wined", real + "/Documents/Wined"}));
}

void SavesFinderTest::storeFolderComesBeforeTheManifest()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    const QString steam = m_prefix + "/drive_c/users/steamuser";
    const QString real = m_prefix + "/drive_c/users/someone";
    touch(steam + "/AppData/Roaming/Celeste/Saves/steam.celeste");
    touch(real + "/AppData/Roaming/Celeste/Saves/real.celeste");
    touch(real + "/AppData/LocalLow/Studio/Celeste/slot.sav");

    Query q;
    q.id = "epic-app";
    q.title = "Celeste";
    q.prefix = m_prefix;
    q.wineUser = "someone";
    q.storeFolders = {real + "/AppData/LocalLow/Studio/Celeste"};
    q.savePaths = {m_env.home + "/mine"};
    touch(m_env.home + "/mine/own.sav");

    // The title would match the manifest, but Epic's folder is used instead; the user's own paths still count.
    Found found = find(matcher, {q}, m_env);
    QCOMPARE(found.match, Match::Epic);
    QVERIFY(found.matched.isEmpty());
    QCOMPARE(found.paths, (QStringList{real + "/AppData/LocalLow/Studio/Celeste", m_env.home + "/mine"}));

    // No store folder: the manifest, in the same user's folder.
    q.storeFolders.clear();
    found = find(matcher, {q}, m_env);
    QCOMPARE(found.match, Match::Title);
    QCOMPARE(found.paths, (QStringList{real + "/AppData/Roaming/Celeste/Saves", m_env.home + "/mine"}));

    // A store folder that isn't there yet doesn't bring the manifest back.
    q.storeFolders = {real + "/AppData/LocalLow/Nobody"};
    found = find(matcher, {q}, m_env);
    QCOMPARE(found.match, Match::Epic);
    QCOMPARE(found.paths, QStringList{m_env.home + "/mine"});
}

void SavesFinderTest::storeFolderStaysInsideItsPrefix()
{
    const SavesIndex index = fixtureIndex();
    const Matcher matcher(index);
    touch(m_prefix + "/drive_c/users/steamuser/documents/my games/Cased/A.sav");

    Query q;
    q.prefix = m_prefix;
    q.storeFolders = {m_prefix + "/drive_c/users/steamuser/Documents/My Games/*"};
    QCOMPARE(find(matcher, {q}, m_env).paths,
             QStringList{m_prefix + "/drive_c/users/steamuser/documents/my games/Cased"});

    q.storeFolders = {m_prefix + "/drive_c/users/steamuser", m_prefix + "/drive_c/../x", "relative/path"};
    QVERIFY(find(matcher, {q}, m_env).paths.isEmpty());
}

// Epic's CloudSaveFolder is a store folder the finder doesn't write itself: one that climbs out of the install
// folder with `..` must be refused like any other.
void SavesFinderTest::anEpicSaveFolderThatGoesUpIsRefused()
{
    const QString root = m_dir.filePath("heroic");
    const QString install = m_env.home + "/Games/Heroic/Game";
    touch(root + "/GamesConfig/EpicApp.json");
    QFile config(root + "/GamesConfig/EpicApp.json");
    QVERIFY(config.open(QIODevice::WriteOnly));
    config.write(R"({"EpicApp": {"winePrefix": ")" + m_prefix.toUtf8() + R"("}})");
    config.close();
    touch(root + "/legendaryConfig/legendary/metadata/EpicApp.json");
    QFile metadata(root + "/legendaryConfig/legendary/metadata/EpicApp.json");
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    metadata.write(R"({"metadata": {"customAttributes": {"CloudSaveFolder": {"value": "{InstallDir}/../../../Documents"}}}})");
    metadata.close();
    touch(install + "/game.exe");
    touch(m_env.home + "/Documents/taxes.pdf");

    Query q;
    q.id = "epic-app";
    q.title = "Nobody Listed";
    q.baseDir = install;
    q.heroicRoot = root;
    q.heroicApp = "EpicApp";
    const SavesIndex none;
    const Found found = find(Matcher(none), {q}, m_env);
    QEXPECT_FAIL("", "S3.6: cleanPath collapses the .. before the finder can refuse it", Continue);
    QVERIFY2(!found.paths.contains(m_env.home + "/Documents"), qPrintable(found.paths.join(", ")));
}

QTEST_MAIN(SavesFinderTest)
#include "savesfinder_test.moc"
