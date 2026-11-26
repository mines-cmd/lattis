#include "librarytestbase.h"

namespace {
// A folder made read-only for as long as it lives. locked() is false where that does nothing (root).
class ReadOnlyFolder
{
public:
    explicit ReadOnlyFolder(const QString &path) : m_path(path)
    {
        QDir().mkpath(path);
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
    }
    ~ReadOnlyFolder() { open(); }
    void open() { QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner); }
    bool locked() const { return !QFileInfo(m_path).isWritable(); }

private:
    QString m_path;
};

// A file nobody may read for as long as it lives.
class UnreadableFile
{
public:
    explicit UnreadableFile(const QString &path) : m_path(path) { QFile::setPermissions(path, QFileDevice::Permissions{}); }
    ~UnreadableFile() { QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner); }
    bool locked() const { return !QFile(m_path).open(QIODevice::ReadOnly); }

private:
    QString m_path;
};

QStringList tagNames(const Library &library)
{
    QStringList out;
    for (const TagDef &t : library.store().snapshot().content.tags)
        out.append(t.name);
    return out;
}

QString platformName(const Library &library, const QString &id)
{
    for (const Platform &p : library.store().snapshot().content.platforms) {
        if (p.id == id)
            return p.name;
    }
    return {};
}

bool isFavorite(const Library &library, const QString &id)
{
    return library.gameAt(library.rowOfGame(id)).value("favorite").toBool();
}
}

// The library tests on writes that fail: what the user is told, and that nothing on disk or in the list changed.
class LibraryWritesTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void gameValueInAReadOnlyFolder();
    void emptiedGameFileThatCantGo();
    void gameFileThatCantBeRead();
    void platformsFileThatCantBeWrittenOrRead();
    void tagsFileThatCantBeWrittenOrRead();
    void newGameFileThatCantBeWritten();
    void romIndexThatCantBeWritten();
    void brokenRomIndexAddsNothing();
    void modManagerThatCantBeWritten();

private:
    QString games() const { return m_data + QStringLiteral("/library/games"); }
};

#define REQUIRE_LOCKED(lock)                                                                                           \
    if (!(lock).locked())                                                                                              \
        QSKIP("permissions don't stop this user (root)");

void LibraryWritesTest::gameValueInAReadOnlyFolder()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-metroid-dread");
    const QByteArray before = readFile(gamePath(id));
    QVERIFY(!isFavorite(*library, id));
    ReadOnlyFolder lock(games());
    REQUIRE_LOCKED(lock);

    const QString error = library->setGameValue(id, QStringLiteral("favorite"), true);
    lock.open();
    QVERIFY2(error.startsWith(QStringLiteral("Could not save library/games/%1.toml: ").arg(id)), qPrintable(error));
    QCOMPARE(readFile(gamePath(id)), before);
    QVERIFY(!isFavorite(*library, id));
    QCOMPARE(QDir(games()).entryList({QStringLiteral("*.toml.*")}, QDir::Files | QDir::Hidden), QStringList());
}

void LibraryWritesTest::emptiedGameFileThatCantGo()
{
    auto library = openLibrary();
    // Celeste is only in the Steam index; a favorite gives it a file that holds only that.
    const QString id = QStringLiteral("steam-504230");
    QCOMPARE(library->setGameValue(id, QStringLiteral("favorite"), true), QString());
    QCOMPARE(readFile(gamePath(id)), QByteArray("favorite = true\n"));
    ReadOnlyFolder lock(games());
    REQUIRE_LOCKED(lock);

    const QString error = library->setGameValue(id, QStringLiteral("favorite"), false);
    lock.open();
    QCOMPARE(error, QStringLiteral("Could not remove library/games/%1.toml").arg(id));
    QCOMPARE(readFile(gamePath(id)), QByteArray("favorite = true\n"));
    QVERIFY(isFavorite(*library, id));
    QCOMPARE(library->gameAt(library->rowOfGame(id)).value("title").toString(), QStringLiteral("Celeste"));

    // Once it can, the undone change takes the file with it.
    QCOMPARE(library->setGameValue(id, QStringLiteral("favorite"), false), QString());
    QVERIFY(!QFileInfo::exists(gamePath(id)));
    QVERIFY(!isFavorite(*library, id));
}

void LibraryWritesTest::gameFileThatCantBeRead()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-metroid-dread");
    const QByteArray before = readFile(gamePath(id));
    QString error;
    {
        UnreadableFile lock(gamePath(id));
        REQUIRE_LOCKED(lock);
        error = library->setGameValue(id, QStringLiteral("favorite"), true);
    }
    QVERIFY2(error.startsWith(QStringLiteral("Could not read library/games/%1.toml: ").arg(id)), qPrintable(error));
    QCOMPARE(readFile(gamePath(id)), before);
    QVERIFY(!isFavorite(*library, id));
}

void LibraryWritesTest::platformsFileThatCantBeWrittenOrRead()
{
    auto library = openLibrary();
    const QString path = m_data + QStringLiteral("/library/platforms.toml");
    const QByteArray before = readFile(path);
    const QString name = platformName(*library, QStringLiteral("switch"));
    QVERIFY(!name.isEmpty());

    QString error;
    {
        ReadOnlyFolder lock(m_data + QStringLiteral("/library"));
        REQUIRE_LOCKED(lock);
        error = library->setPlatformValue(QStringLiteral("switch"), QStringLiteral("name"), QStringLiteral("Switch!"));
    }
    QVERIFY2(error.startsWith(QStringLiteral("Could not save library/platforms.toml: ")), qPrintable(error));
    QCOMPARE(readFile(path), before);
    QCOMPARE(platformName(*library, QStringLiteral("switch")), name);

    {
        UnreadableFile lock(path);
        error = library->setPlatformValue(QStringLiteral("switch"), QStringLiteral("name"), QStringLiteral("Switch!"));
    }
    QVERIFY2(error.startsWith(QStringLiteral("Could not read library/platforms.toml: ")), qPrintable(error));
    QCOMPARE(readFile(path), before);
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(platformName(*library, QStringLiteral("switch")), name);
}

void LibraryWritesTest::tagsFileThatCantBeWrittenOrRead()
{
    auto library = openLibrary();
    const QString path = m_data + QStringLiteral("/library/tags.toml");
    const QByteArray before = readFile(path);
    const QStringList tags = tagNames(*library);
    QVERIFY(!tags.isEmpty());

    QString error;
    {
        ReadOnlyFolder lock(m_data + QStringLiteral("/library"));
        REQUIRE_LOCKED(lock);
        error = library->addTag(QStringLiteral("Couch co-op"), 1);
    }
    QVERIFY2(error.startsWith(QStringLiteral("Could not save library/tags.toml: ")), qPrintable(error));
    QCOMPARE(readFile(path), before);
    QCOMPARE(tagNames(*library), tags);

    {
        UnreadableFile lock(path);
        error = library->addTag(QStringLiteral("Couch co-op"), 1);
    }
    QVERIFY2(error.startsWith(QStringLiteral("Could not read library/tags.toml: ")), qPrintable(error));
    QCOMPARE(readFile(path), before);
    QCOMPARE(tagNames(*library), tags);
}

void LibraryWritesTest::newGameFileThatCantBeWritten()
{
    const QString rom = m_installs.home + QStringLiteral("/Downloads/SUPER MARIO ODYSSEY.nsp");
    writeFile(rom, {});
    auto library = openLibrary();
    const int rows = library->rowCount();
    const QStringList before = QDir(games()).entryList(QDir::Files | QDir::Hidden);

    QVariantMap added;
    {
        ReadOnlyFolder lock(games());
        REQUIRE_LOCKED(lock);
        added = library->addGame(rom, QStringLiteral("switch"), QStringLiteral("Super Mario Odyssey"), {}, {});
    }
    const QString error = added.value("error").toString();
    QVERIFY2(error.startsWith(QStringLiteral("Could not save library/games/switch-super-mario-odyssey.toml: ")),
             qPrintable(error));
    QVERIFY(added.value("id").toString().isEmpty());
    QCOMPARE(QDir(games()).entryList(QDir::Files | QDir::Hidden), before);
    QCOMPARE(library->rowOfGame(QStringLiteral("switch-super-mario-odyssey")), -1);
    QCOMPARE(library->rowCount(), rows);

    // Asked again once it can: added under the same id.
    added = library->addGame(rom, QStringLiteral("switch"), QStringLiteral("Super Mario Odyssey"), {}, {});
    QCOMPARE(added.value("error").toString(), QString());
    QCOMPARE(added.value("id").toString(), QStringLiteral("switch-super-mario-odyssey"));
}

void LibraryWritesTest::romIndexThatCantBeWritten()
{
    auto library = openLibrary();
    const QString rom = m_data + QStringLiteral("/roms/switch/Sub/Celeste.nsp");
    writeFile(rom, {});
    const QByteArray before = readFile(romIndexPath("switch"));
    QVERIFY(!before.isEmpty());

    LibraryStore::AddedGame added;
    {
        ReadOnlyFolder lock(m_data + QStringLiteral("/library/roms"));
        REQUIRE_LOCKED(lock);
        added = library->store().addGame(rom, QStringLiteral("switch"), QStringLiteral("Celeste"));
    }
    QVERIFY2(added.error.startsWith(QStringLiteral("Could not save library/roms/switch.toml: ")), qPrintable(added.error));
    QVERIFY(added.id.isEmpty());
    QCOMPARE(readFile(romIndexPath("switch")), before);
    QCOMPARE(library->rowOfGame(QStringLiteral("switch-celeste")), -1);
    QVERIFY(library->store().gameWithRom(rom).isEmpty());
}

void LibraryWritesTest::brokenRomIndexAddsNothing()
{
    auto library = openLibrary();
    const QByteArray broken = "[roms\n\"x.nsp\" = 1\n";
    writeFile(romIndexPath("switch"), broken);
    QTRY_VERIFY_WITH_TIMEOUT(anyContains(library->problems(), QStringLiteral("library/roms/switch.toml")), 5000);
    const QString rom = m_data + QStringLiteral("/roms/switch/Sub/Celeste.nsp");
    writeFile(rom, {});

    const LibraryStore::AddedGame added =
        library->store().addGame(rom, QStringLiteral("switch"), QStringLiteral("Celeste"));
    QCOMPARE(added.error, QStringLiteral("library/roms/switch.toml has an error, so the game wasn't added"));
    QVERIFY(added.id.isEmpty());
    QCOMPARE(readFile(romIndexPath("switch")), broken);
    QVERIFY(!QFileInfo::exists(gamePath("switch-celeste")));
    QCOMPARE(library->store().setRomEntries(QStringLiteral("switch"), {}),
             QStringLiteral("library/roms/switch.toml has an error, so the change wasn't saved"));
    QCOMPARE(readFile(romIndexPath("switch")), broken);
}

void LibraryWritesTest::modManagerThatCantBeWritten()
{
    auto library = openLibrary();
    const QString folder = m_data + QStringLiteral("/library/mod-managers");
    QVariantMap added;
    {
        ReadOnlyFolder lock(m_data + QStringLiteral("/library"));
        REQUIRE_LOCKED(lock);
        added = library->addModManager({{"name", "Tool"}, {"exe", "/opt/tool.sh"}, {"games", QVariantList{}}});
    }
    const QString error = added.value("error").toString();
    QVERIFY2(error.startsWith(QStringLiteral("Could not save library/mod-managers/tool.toml: ")), qPrintable(error));
    QVERIFY(added.value("id").toString().isEmpty());
    QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/tool.toml")));
    QVERIFY(library->modManagers().isEmpty());
}

QTEST_GUILESS_MAIN(LibraryWritesTest)

#include "library_writes_test.moc"
