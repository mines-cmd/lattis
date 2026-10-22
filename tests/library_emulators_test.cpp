#include "librarytestbase.h"

namespace {

QVariantMap emulatorIn(const Library &library, const QString &id)
{
    for (const QVariant &e : library.emulatorList()) {
        if (e.toMap().value("id") == id)
            return e.toMap();
    }
    return {};
}

} // namespace

// The library tests on emulators: the installs found, their values.
class LibraryEmulatorsTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void emulatorListShowsInstalls();
    void emulatorValuesWriteOnlyChangedKeys();
    void emulatorValuesKeepHandEdits();
    void userEmulatorValues();
    void addBlankEmulator();
    void addCopiedEmulator();
    void addEmulatorChecksTheId();
    void removeEmulator();
    void pinnedGamesStayPinned();
    void userEmulatorEdits();
};

void LibraryEmulatorsTest::emulatorListShowsInstalls()
{
    auto library = openLibrary();
    const QVariantList list = library->emulatorList();
    QCOMPARE(list.size(), 12);
    QCOMPARE(list.at(0).toMap().value("id"), QStringLiteral("azahar")); // presets by id
    const QVariantMap ryubing = emulatorIn(*library, "ryubing");
    QCOMPARE(ryubing.value("name"), QStringLiteral("Ryubing"));
    QCOMPARE(ryubing.value("platforms").toStringList(), QStringList{"Switch"});
    QVERIFY(ryubing.value("preset").toBool());
    QVERIFY(ryubing.value("found").toBool());
    QVERIFY(!ryubing.value("detecting").toBool());
    QCOMPARE(ryubing.value("installKind"), QStringLiteral("flatpak"));
    QCOMPARE(ryubing.value("install"), QStringLiteral("Flatpak io.github.ryubing.Ryujinx"));
    QCOMPARE(ryubing.value("command"), QStringLiteral("flatpak run io.github.ryubing.Ryujinx ROM"));
    const QVariantMap azahar = emulatorIn(*library, "azahar");
    QVERIFY(azahar.value("found").toBool());
    QCOMPARE(azahar.value("installKind"), QStringLiteral("flatpak"));
}

void LibraryEmulatorsTest::emulatorValuesWriteOnlyChangedKeys()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    QVERIFY(!QFile::exists(file));
    auto library = openLibrary();

    QCOMPARE(library->setEmulatorValue("ryubing", "path", "~/Apps/Ryujinx.AppImage"), QString());
    QCOMPARE(library->setEmulatorValue("ryubing", "args", QStringList{"--fullscreen"}), QString());
    const QByteArray header = "# Emulators. Ryubing and Azahar are built in: list one here to set its install or\n"
                              "# change a key. Any other id defines a new emulator; see emulators.reference.toml.\n";
    QCOMPARE(readFile(file), header + "\n[[emulator]]\nid = \"ryubing\"\npath = \"~/Apps/Ryujinx.AppImage\"\n"
                                      "args = [\"--fullscreen\"]\n");
    QTRY_COMPARE(emulatorIn(*library, "ryubing").value("custom").toStringList(), QStringList({"args", "path"}));
    QVariantMap ryubing = emulatorIn(*library, "ryubing");
    QCOMPARE(ryubing.value("path"), QStringLiteral("~/Apps/Ryujinx.AppImage"));
    QVERIFY(ryubing.value("pathMissing").toBool());
    QCOMPARE(ryubing.value("args").toStringList(), QStringList{"--fullscreen"});
    QCOMPARE(ryubing.value("custom").toStringList(), QStringList({"args", "path"}));

    // Back to the preset's values: the keys, then the block, go away.
    QCOMPARE(library->setEmulatorValue("ryubing", "path", QString()), QString());
    QCOMPARE(library->setEmulatorValue("ryubing", "args", QStringList()), QString());
    QCOMPARE(readFile(file), header);
    QTRY_VERIFY(emulatorIn(*library, "ryubing").value("custom").toStringList().isEmpty());
    ryubing = emulatorIn(*library, "ryubing");
    QCOMPARE(ryubing.value("path"), QString());
    QVERIFY(ryubing.value("custom").toStringList().isEmpty());
    QCOMPARE(ryubing.value("installKind"), QStringLiteral("flatpak"));

    QVERIFY(!library->setEmulatorValue("ryubing", "command", QStringList{"x"}).isEmpty());
    QVERIFY(!library->setEmulatorValue("citra", "path", "/x").isEmpty());
    QCOMPARE(readFile(file), header);
}

void LibraryEmulatorsTest::emulatorValuesKeepHandEdits()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    writeFile(file, "# Mine.\n\n# Ryubing, from the Flatpak.\n[[emulator]]\nid = \"ryubing\"\n"
                    "args = [\"--fullscreen\"] # big\n");
    auto library = openLibrary();
    QCOMPARE(library->setEmulatorValue("ryubing", "args", QStringList{"-f", "a b"}), QString());
    QCOMPARE(library->setEmulatorValue("azahar", "path", "/opt/azahar"), QString());
    QCOMPARE(readFile(file), QByteArray("# Mine.\n\n# Ryubing, from the Flatpak.\n[[emulator]]\nid = \"ryubing\"\n"
                                        "args = [\"-f\", \"a b\"] # big\n\n"
                                        "[[emulator]]\nid = \"azahar\"\npath = \"/opt/azahar\"\n"));

    // Removing Ryubing's last change takes its comment along.
    QCOMPARE(library->setEmulatorValue("ryubing", "args", QStringList()), QString());
    QCOMPARE(readFile(file), QByteArray("# Mine.\n\n[[emulator]]\nid = \"azahar\"\npath = \"/opt/azahar\"\n"));
}

void LibraryEmulatorsTest::userEmulatorValues()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    const QByteArray citra = "[[emulator]]\nid = \"citra\"\nname = \"Citra\"\nplatforms = [\"3ds\"]\n"
                             "path = \"~/citra.AppImage\"\ncommand = [\"{exe}\", \"{args}\", \"{rom}\"]\n"
                             "extensions = [\"3ds\"]\n";
    writeFile(file, citra);
    auto library = openLibrary();
    QVERIFY(!emulatorIn(*library, "citra").value("preset").toBool());

    // A user-defined emulator keeps its block, even with nothing but its definition.
    QCOMPARE(library->setEmulatorValue("citra", "args", QStringList{"-f"}), QString());
    QCOMPARE(library->setEmulatorValue("citra", "args", QStringList()), QString());
    QCOMPARE(readFile(file), citra);
}

void LibraryEmulatorsTest::addBlankEmulator()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    auto library = openLibrary();
    QCOMPARE(library->emulatorIdFor("My Emulator"), QStringLiteral("my-emulator"));
    QCOMPARE(library->addEmulator("My Emulator"), QString());
    QVERIFY(readFile(file).endsWith("[[emulator]]\nid = \"my-emulator\"\nname = \"My Emulator\"\n"
                                    "command = [\"{exe}\", \"{args}\", \"{rom}\"]\nextensions = []\n"));
    QTRY_VERIFY(!emulatorIn(*library, "my-emulator").isEmpty());
    const QVariantMap added = emulatorIn(*library, "my-emulator");
    QVERIFY(!added.value("preset").toBool());
    QVERIFY(added.value("fileOnly").toStringList().isEmpty());
    QCOMPARE(added.value("commandArgs").toStringList(), QStringList({"{exe}", "{args}", "{rom}"}));

    QVERIFY(!library->addEmulator("   ").isEmpty());
    QVERIFY(!library->addEmulator("!!!").isEmpty());
}

void LibraryEmulatorsTest::addCopiedEmulator()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    const QString platforms = m_data + QStringLiteral("/library/platforms.toml");
    auto library = openLibrary();
    QCOMPARE(library->addEmulator("Citra", "azahar"), QString());

    const QByteArray written = readFile(file);
    QVERIFY(written.contains("id = \"citra\"\nname = \"Citra\"\nplatforms = [\"3ds\"]\n"));
    QVERIFY(written.contains("extensions = [\"3ds\", \"cci\", \"cxi\", \"3dsx\", \"app\", \"elf\"]"));
    for (const char *left : {"flatpak", "executables", "appimage", "release", "release_asset", "release_inside", "saves", "keys"})
        QVERIFY2(!written.contains(QByteArray("\n") + left + " ="), left);

    // Last, so the built-in is still tried first.
    QVERIFY(readFile(platforms).contains("id = \"3ds\"\nname = \"3DS\"\nicon = \"3ds\"\n"
                                         "emulators = [\"azahar\", \"citra\"]\n"));

    QVERIFY(!library->addEmulator("Other", "nothing").isEmpty());
}

void LibraryEmulatorsTest::addEmulatorChecksTheId()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    auto library = openLibrary();
    QCOMPARE(library->addEmulator("Azahar"), QStringLiteral("There's already an emulator with the id azahar"));

    // Defined in the file but not yet in the snapshot.
    writeFile(file, "[[emulator]]\nid = \"mine\"\nname = \"Mine\"\n");
    QCOMPARE(library->addEmulator("Mine"), QStringLiteral("There's already an emulator with the id mine"));

    QCOMPARE(library->addEmulator("Copy", "azahar"), QString());
    QCOMPARE(library->addEmulator("Copy", "azahar"), QStringLiteral("There's already an emulator with the id copy"));
    const QByteArray platforms = readFile(m_data + QStringLiteral("/library/platforms.toml"));
    QCOMPARE(platforms.count("\"copy\""), 1); // not joined twice
}

void LibraryEmulatorsTest::removeEmulator()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    const QString platforms = m_data + QStringLiteral("/library/platforms.toml");
    auto library = openLibrary();
    QCOMPARE(library->addEmulator("Citra", "azahar"), QString());
    QTRY_VERIFY(!emulatorIn(*library, "citra").isEmpty());
    QCOMPARE(library->addEmulator("Keep"), QString());

    QCOMPARE(library->removeEmulator("citra"), QString());
    QVERIFY(!readFile(file).contains("citra"));
    QVERIFY(readFile(file).contains("id = \"keep\""));
    QVERIFY(!readFile(platforms).contains("citra"));
    QVERIFY(readFile(platforms).contains("emulators = [\"azahar\"]"));
    QTRY_VERIFY(emulatorIn(*library, "citra").isEmpty());

    QVERIFY(!library->removeEmulator("azahar").isEmpty());
    QVERIFY(!library->removeEmulator("citra").isEmpty());
    QVERIFY(emulatorIn(*library, "azahar").value("preset").toBool());
}

void LibraryEmulatorsTest::pinnedGamesStayPinned()
{
    auto library = openLibrary();
    QCOMPARE(library->addEmulator("Citra", "azahar"), QString());
    const QString game = m_data + QStringLiteral("/library/games/3ds-pokemon-ultra-sun.toml");
    QVERIFY(QFile::exists(game));
    QByteArray text = readFile(game);
    text += "emulator = \"citra\"\n";
    writeFile(game, text);
    QTRY_COMPARE(library->pinnedGames("citra"), 1);
    QCOMPARE(library->removeEmulator("citra"), QString());
    QVERIFY(readFile(game).contains("emulator = \"citra\""));
    QTRY_COMPARE(library->pinnedGames("citra"), 1);
}

void LibraryEmulatorsTest::userEmulatorEdits()
{
    const QString file = m_data + QStringLiteral("/library/emulators.toml");
    writeFile(file, "[[emulator]]\nid = \"mine\"\nname = \"Mine\"\ncommand = [\"{exe}\", \"{rom}\"]\n"
                    "extensions = [\"iso\"]\nsaves = { native = [\"~/x\"] }\n");
    auto library = openLibrary();
    QTRY_VERIFY(!emulatorIn(*library, "mine").isEmpty());
    QCOMPARE(emulatorIn(*library, "mine").value("fileOnly").toStringList(), QStringList{"saves"});

    QCOMPARE(library->setEmulatorValue("mine", "name", "Mine 2"), QString());
    QCOMPARE(library->setEmulatorValue("mine", "flatpak", "org.example.Mine"), QString());
    QCOMPARE(library->setEmulatorValue("mine", "platforms", QStringList{"3ds", "switch"}), QString());
    QCOMPARE(library->setEmulatorValue("mine", "extensions", QStringList{".ISO, bin  .Cue"}), QString());
    QVERIFY(readFile(file).contains("name = \"Mine 2\""));
    QVERIFY(readFile(file).contains("flatpak = \"org.example.Mine\""));
    QVERIFY(readFile(file).contains("platforms = [\"3ds\", \"switch\"]"));
    QVERIFY(readFile(file).contains("extensions = [\"iso\", \"bin\", \"cue\"]"));

    QVERIFY(!library->setEmulatorValue("mine", "name", "  ").isEmpty());
    QVERIFY(!library->setEmulatorValue("mine", "command", QStringList{"run", "{rom}"}).isEmpty());
    QVERIFY(!library->setEmulatorValue("mine", "command", QStringList()).isEmpty());
    QVERIFY(!library->setEmulatorValue("mine", "saves", QStringList{"x"}).isEmpty());
    QCOMPARE(library->setEmulatorValue("mine", "command", QStringList{"{exe}", "-x", "{rom}"}), QString());
    QVERIFY(readFile(file).contains("command = [\"{exe}\", \"-x\", \"{rom}\"]"));

    // A built-in's other keys stay in the file.
    QVERIFY(!library->setEmulatorValue("azahar", "name", "A").isEmpty());
}

QTEST_GUILESS_MAIN(LibraryEmulatorsTest)

#include "library_emulators_test.moc"
