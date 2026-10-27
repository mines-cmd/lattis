#include "librarytestbase.h"

// Library::allMedia(): every game's clips and screenshots in batches.
class LibraryMediaTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void everyGamesMediaArrivesInBatches();
    void aLibraryWithoutMediaEndsWithDone();
    void aNewCallDropsTheOlderOnesBatches();

private:
    struct Result
    {
        QVariantList items;
        int batches = 0;
        int doneAt = -1;
    };
    Result collect(Library &library);
    void touch(const QString &relative) { writeFile(relative, "x"); }
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

QTEST_MAIN(LibraryMediaTest)
#include "library_media_test.moc"
