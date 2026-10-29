#include "librarytestbase.h"

#include "mediafolders.h"

#include <QSignalSpy>

using namespace Qt::StringLiterals;

// Library::allMedia(): every game's clips and screenshots in batches.
class LibraryMediaTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void everyGamesMediaArrivesInBatches();
    void aLibraryWithoutMediaEndsWithDone();
    void aNewCallDropsTheOlderOnesBatches();
    void ownFoldersJoinSteamsMediaNewestFirst();
    void aFolderPointingIntoSteamsScreenshotsShowsThemOnce();
    void linuxAndProtonGamesGetTheirFolders();
    void romGamesKeepTheirEmulatorsScreenshots();
    void aHugeFolderIsLimited();
    void videoThumbnailsArriveAfterTheList();
    void allMediaIncludesOwnFolders();

private:
    struct Result
    {
        QVariantList items;
        int batches = 0;
        int doneAt = -1;
    };
    Result collect(Library &library);
    void touch(const QString &relative) { writeFile(relative, "x"); }
    void setMediaFolders(Library &library, const QString &id, const QStringList &folders);
    void touchAt(const QString &path, int daysAfter);
    QVariantList media(Library &library, const QString &id, bool *limited = nullptr);
};

// The emulators' screenshot folders are looked up under the home folder.
struct HomeGuard
{
    explicit HomeGuard(const QString &home) : saved(qgetenv("HOME")) { qputenv("HOME", home.toUtf8()); }
    ~HomeGuard() { qputenv("HOME", saved); }
    QByteArray saved;
};

LibraryMediaTest::Result LibraryMediaTest::collect(Library &library)
{
    Result result;
    QObject::connect(&library, &Library::allMediaReady, &library, [&](const QVariantList &batch, bool done) {
        result.items += batch;
        if (done)
            result.doneAt = result.batches;
        ++result.batches;
    });
    library.allMedia();
    if (!QTest::qWaitFor([&] { return result.doneAt >= 0; }, 10000))
        qWarning("allMedia: no last batch");
    // Nothing more comes after done.
    QTest::qWait(50);
    return result;
}

void LibraryMediaTest::everyGamesMediaArrivesInBatches()
{
    const HomeGuard home(m_installs.home);
    const QString user = m_data + QStringLiteral("/steam/userdata/39734273");
    const QString shots = user + QStringLiteral("/760/remote/%1/screenshots/");
    touch(shots.arg(1145360) + QStringLiteral("20260301120000_1.jpg"));
    touch(shots.arg(730) + QStringLiteral("20260302120000_1.png"));
    touch(shots.arg(99999) + QStringLiteral("20260303120000_1.jpg")); // an app the library doesn't have
    copyTree(QStringLiteral(STEAM_DATA_DIR "/userdata/39734273/gamerecordings/clips/clip_8000_20260215_090000"),
             user + QStringLiteral("/gamerecordings/clips/clip_1145360_20260215_090000"));

    const QString azahar = m_installs.home + QStringLiteral("/.var/app/org.azahar_emu.Azahar/data/azahar-emu/screenshots/");
    touch(azahar + QStringLiteral("Kid Icarus Uprising_20260101_100000.png")); // the title's colon left out
    touch(azahar + QStringLiteral("Fire Emblem Awakening_20260102_100000.png"));
    touch(azahar + QStringLiteral("Another Game_20260103_100000.png"));
    const QString ryubing = m_installs.home + QStringLiteral("/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/screenshots/");
    touch(ryubing + QStringLiteral("Pikmin 4_20260104_100000.png"));

    auto library = openLibrary();
    const Result result = collect(*library);
    QVERIFY2(result.doneAt >= 0, "the last batch never came");
    QCOMPARE(result.doneAt, result.batches - 1);
    QVERIFY(result.batches >= 3); // Steam, Azahar's games, Ryubing's games, then done

    QMap<QString, QStringList> perGame;
    for (const QVariant &v : result.items) {
        const QVariantMap item = v.toMap();
        QVERIFY(!item.value("gameId").toString().isEmpty());
        QVERIFY(!item.value("thumbnail").toString().isEmpty() || item.value("kind").toString() == u"clip");
        perGame[item.value("gameId").toString()].append(item.value("kind").toString());
    }
    for (QStringList &kinds : perGame)
        kinds.sort();
    QCOMPARE(perGame, (QMap<QString, QStringList>{{"steam-1145360", {"clip", "screenshot"}},
                                                 {"steam-730", {"screenshot"}},
                                                 {"3ds-kid-icarus-uprising", {"screenshot"}},
                                                 {"3ds-fire-emblem-awakening", {"screenshot"}},
                                                 {"switch-pikmin-4", {"screenshot"}}}));

    for (const QVariant &v : result.items) {
        const QVariantMap item = v.toMap();
        if (item.value("kind").toString() == u"clip") {
            QCOMPARE(item.value("length").toString(), QStringLiteral("0:03"));
            QVERIFY(!item.value("videoDir").toString().isEmpty());
        } else {
            QCOMPARE(item.value("image"), item.value("thumbnail"));
        }
        QVERIFY(item.value("time").toDateTime().isValid());
    }
}

void LibraryMediaTest::aLibraryWithoutMediaEndsWithDone()
{
    const HomeGuard home(m_installs.home);
    auto library = openLibrary();
    const Result result = collect(*library);
    QCOMPARE(result.items.size(), 0);
    QCOMPARE(result.batches, 1);
    QCOMPARE(result.doneAt, 0);
}

void LibraryMediaTest::aNewCallDropsTheOlderOnesBatches()
{
    const HomeGuard home(m_installs.home);
    touch(m_data + QStringLiteral("/steam/userdata/39734273/760/remote/730/screenshots/20260302120000_1.png"));
    auto library = openLibrary();
    int done = 0;
    int items = 0;
    QObject::connect(library.get(), &Library::allMediaReady, library.get(), [&](const QVariantList &batch, bool last) {
        items += batch.size();
        done += last ? 1 : 0;
    });
    library->allMedia();
    library->allMedia();
    QTRY_VERIFY(done >= 1);
    QTest::qWait(100);
    QCOMPARE(done, 1);
    QCOMPARE(items, 1);
}

void LibraryMediaTest::setMediaFolders(Library &library, const QString &id, const QStringList &folders)
{
    QCOMPARE(library.store().setGameValues(id, {{QStringLiteral("media_folders"), folders}}), QString());
}

void LibraryMediaTest::touchAt(const QString &path, int daysAfter)
{
    touch(path);
    QFile file(path);
    QVERIFY(file.open(QIODevice::Append));
    QVERIFY(file.setFileTime(QDateTime(QDate(2026, 1, 1), QTime(12, 0)).addDays(daysAfter),
                             QFileDevice::FileModificationTime));
}

QVariantList LibraryMediaTest::media(Library &library, const QString &id, bool *limited)
{
    QVariantList items;
    bool got = false;
    if (limited)
        *limited = false;
    QObject::connect(&library, &Library::mediaLimited, &library, [&](const QString &game) {
        if (limited && game == id)
            *limited = true;
    });
    QObject::connect(&library, &Library::mediaReady, &library, [&](const QString &game, const QVariantList &list) {
        if (game == id) {
            items = list;
            got = true;
        }
    });
    library.media(id);
    if (!QTest::qWaitFor([&] { return got; }, 10000))
        qWarning("media: no mediaReady");
    return items;
}

void LibraryMediaTest::ownFoldersJoinSteamsMediaNewestFirst()
{
    const HomeGuard home(m_installs.home);
    const QString shots = m_data + QStringLiteral("/steam/userdata/39734273/760/remote/1145360/screenshots/");
    touch(shots + QStringLiteral("20260301120000_1.jpg")); // Steam reads the time from the name
    const QString clips = m_installs.home + QStringLiteral("/Videos/Hades/");
    touchAt(clips + QStringLiteral("old.png"), 31);                    // February
    touchAt(clips + QStringLiteral("deep/new.mkv"), 130);              // May
    touchAt(clips + QStringLiteral("deep/.hidden.png"), 140);
    touchAt(clips + QStringLiteral("notes.txt"), 150);

    auto library = openLibrary();
    setMediaFolders(*library, QStringLiteral("steam-1145360"), {QStringLiteral("~/Videos/Hades")});
    const QVariantList items = media(*library, QStringLiteral("steam-1145360"));
    QStringList got;
    for (const QVariant &v : items) {
        const QVariantMap item = v.toMap();
        got.append(item.value("kind").toString() + u':' + QFileInfo(item.value("id").toString()).fileName());
    }
    QVERIFY2(got.indexOf(u"video:new.mkv"_s) >= 0 && got.indexOf(u"screenshot:old.png"_s) >= 0, qPrintable(got.join(u' ')));
    QVERIFY(got.indexOf(u"video:new.mkv"_s) < got.indexOf(u"screenshot:20260301120000_1.jpg"_s));
    QVERIFY(got.indexOf(u"screenshot:20260301120000_1.jpg"_s) < got.indexOf(u"screenshot:old.png"_s));
    QCOMPARE(got.count(u"video:new.mkv"_s), 1);
    QVERIFY(!got.contains(u"screenshot:.hidden.png"_s));

    for (const QVariant &v : items) {
        const QVariantMap item = v.toMap();
        if (item.value("kind").toString() != u"video")
            continue;
        QCOMPARE(item.value("id").toString(), clips + QStringLiteral("deep/new.mkv"));
        QCOMPARE(item.value("folder").toString(), clips + QStringLiteral("deep"));
        QCOMPARE(QUrl(item.value("file").toString()).toLocalFile(), clips + QStringLiteral("deep/new.mkv"));
        QVERIFY(item.value("thumbnail").toString().isEmpty());
        QVERIFY(item.value("time").toDateTime().isValid());
    }
}

void LibraryMediaTest::aFolderPointingIntoSteamsScreenshotsShowsThemOnce()
{
    const HomeGuard home(m_installs.home);
    const QString shots = m_data + QStringLiteral("/steam/userdata/39734273/760/remote/1145360/screenshots/");
    touch(shots + QStringLiteral("20260301120000_1.jpg"));
    touch(shots + QStringLiteral("thumbnails/20260301120000_1.jpg"));
    touchAt(m_installs.home + QStringLiteral("/Pictures/own.png"), 10);

    auto library = openLibrary();
    setMediaFolders(*library, QStringLiteral("steam-1145360"),
                    {shots, m_installs.home + QStringLiteral("/Pictures")});
    int screenshots = 0;
    QStringList names;
    for (const QVariant &v : media(*library, QStringLiteral("steam-1145360"))) {
        const QVariantMap item = v.toMap();
        if (item.value("kind").toString() != u"screenshot")
            continue;
        ++screenshots;
        names.append(QFileInfo(item.value("id").toString()).fileName());
    }
    names.sort();
    QCOMPARE(names, (QStringList{u"20260301120000_1.jpg"_s, u"own.png"_s}));
    QCOMPARE(screenshots, 2);
}

void LibraryMediaTest::linuxAndProtonGamesGetTheirFolders()
{
    const HomeGuard home(m_installs.home);
    touchAt(m_installs.home + QStringLiteral("/Videos/OBS/kart.mp4"), 5);
    touchAt(m_installs.home + QStringLiteral("/Videos/OBS/kart.png"), 6);

    auto library = openLibrary();
    QCOMPARE(media(*library, QStringLiteral("linux-supertuxkart")).size(), 0);
    setMediaFolders(*library, QStringLiteral("linux-supertuxkart"), {QStringLiteral("~/Videos/OBS")});
    setMediaFolders(*library, QStringLiteral("proton-diablo-iv"), {QStringLiteral("~/Videos/OBS")});
    for (const QString &id : {u"linux-supertuxkart"_s, u"proton-diablo-iv"_s}) {
        const QVariantList items = media(*library, id);
        QCOMPARE(items.size(), 2);
        QCOMPARE(items.at(0).toMap().value("kind").toString(), u"screenshot"_s); // the newer
        QCOMPARE(items.at(1).toMap().value("kind").toString(), u"video"_s);
    }
    // A folder that isn't there (an unplugged drive) adds nothing.
    setMediaFolders(*library, QStringLiteral("linux-factorio"), {QStringLiteral("~/Gone")});
    QCOMPARE(media(*library, QStringLiteral("linux-factorio")).size(), 0);
}

void LibraryMediaTest::romGamesKeepTheirEmulatorsScreenshots()
{
    const HomeGuard home(m_installs.home);
    touchAt(m_installs.home + QStringLiteral("/Videos/OBS/pikmin.png"), 5);
    const QString ryubing = m_installs.home + QStringLiteral("/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/screenshots/");
    touch(ryubing + QStringLiteral("Pikmin 4_20260104_100000.png"));

    auto library = openLibrary();
    setMediaFolders(*library, QStringLiteral("switch-pikmin-4"), {QStringLiteral("~/Videos/OBS")});
    const QVariantList items = media(*library, QStringLiteral("switch-pikmin-4"));
    QCOMPARE(items.size(), 1);
    QVERIFY(items.at(0).toMap().value("id").toString().startsWith(u"Pikmin 4"_s));
}

void LibraryMediaTest::aHugeFolderIsLimited()
{
    const HomeGuard home(m_installs.home);
    const QString folder = m_installs.home + QStringLiteral("/Pictures/Big/");
    for (int i = 0; i < MediaFolders::kLimit + 5; ++i)
        touch(folder + QStringLiteral("%1.png").arg(i));
    touchAt(m_installs.home + QStringLiteral("/Pictures/Big/newest.png"), 3000);

    auto library = openLibrary();
    setMediaFolders(*library, QStringLiteral("linux-supertuxkart"), {QStringLiteral("~/Pictures/Big")});
    bool limited = false;
    const QVariantList items = media(*library, QStringLiteral("linux-supertuxkart"), &limited);
    QCOMPARE(items.size(), MediaFolders::kLimit);
    QVERIFY(limited);
    QCOMPARE(QFileInfo(items.first().toMap().value("id").toString()).fileName(), u"newest.png"_s);

    setMediaFolders(*library, QStringLiteral("linux-factorio"), {QStringLiteral("~/Pictures/Big/nowhere")});
    media(*library, QStringLiteral("linux-factorio"), &limited);
    QVERIFY(!limited);
}

void LibraryMediaTest::videoThumbnailsArriveAfterTheList()
{
    const HomeGuard home(m_installs.home);
    const QString video = m_installs.home + QStringLiteral("/Videos/OBS/clip.mp4");
    QDir().mkpath(m_installs.home + QStringLiteral("/Videos/OBS"));
    QVERIFY(QFile::copy(QStringLiteral(MEDIA_HOME_DIR "/Videos/OBS/Belmont's Curse/2026-10-02 21-00-00.mp4"), video));

    auto library = openLibrary();
    setMediaFolders(*library, QStringLiteral("linux-supertuxkart"), {QStringLiteral("~/Videos/OBS")});
    QSignalSpy ready(library.get(), &Library::videoThumbnailReady);
    const QVariantList first = media(*library, QStringLiteral("linux-supertuxkart"));
    QCOMPARE(first.size(), 1);
    QVERIFY(first.at(0).toMap().value("thumbnail").toString().isEmpty());
    QVERIFY(first.at(0).toMap().value("length").toString().isEmpty());

    QVERIFY2(ready.wait(20000), "no thumbnail was made");
    QCOMPARE(ready.first().at(0).toString(), first.at(0).toMap().value("file").toString());
    const QString thumbnail = ready.first().at(1).toString();
    QVERIFY(QFile::exists(QUrl(thumbnail).toLocalFile()));
    QCOMPARE(ready.first().at(2).toString(), u"0:03"_s);

    // The second listing has it already.
    const QVariantList second = media(*library, QStringLiteral("linux-supertuxkart"));
    QCOMPARE(second.at(0).toMap().value("thumbnail").toString(), thumbnail);
    QCOMPARE(second.at(0).toMap().value("length").toString(), u"0:03"_s);
}

void LibraryMediaTest::allMediaIncludesOwnFolders()
{
    const HomeGuard home(m_installs.home);
    touchAt(m_installs.home + QStringLiteral("/Videos/OBS/kart.png"), 6);
    touchAt(m_installs.home + QStringLiteral("/Videos/OBS/kart.mp4"), 5);
    const QString shots = m_data + QStringLiteral("/steam/userdata/39734273/760/remote/1145360/screenshots/");
    touch(shots + QStringLiteral("20260301120000_1.jpg"));

    auto library = openLibrary();
    setMediaFolders(*library, QStringLiteral("linux-supertuxkart"), {QStringLiteral("~/Videos/OBS")});
    setMediaFolders(*library, QStringLiteral("steam-1145360"), {shots});
    const Result result = collect(*library);
    QMap<QString, QStringList> perGame;
    for (const QVariant &v : result.items) {
        const QVariantMap item = v.toMap();
        perGame[item.value("gameId").toString()].append(item.value("kind").toString());
    }
    for (QStringList &kinds : perGame)
        kinds.sort();
    QCOMPARE(perGame.value(u"linux-supertuxkart"_s), (QStringList{u"screenshot"_s, u"video"_s}));
    QCOMPARE(perGame.value(u"steam-1145360"_s), (QStringList{u"screenshot"_s})); // Steam's, once
}

QTEST_MAIN(LibraryMediaTest)
#include "library_media_test.moc"
