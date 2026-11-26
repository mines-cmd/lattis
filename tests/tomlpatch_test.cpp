#include "tomlpatch.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <toml++/toml.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <vector>

class TomlPatchTest : public QObject
{
    Q_OBJECT

private slots:
    void setExistingKey();
    void setExistingKeyPreservesTrailingComment();
    void setKeyMissingInExistingSection();
    void setKeyInEmptySection();
    void setKeyMissingSection();
    void setKeyMissingSectionNoTrailingNewline();
    void setList();
    void removeExistingKey();
    void removeMiddleKeyKeepsOthers();
    void removeKeyWithComment();
    void removeMultilineList();
    void removeMissingKeyIsNoop();
    void removeMissingSectionIsNoop();
    void parseErrorIsReported();
    void fileRoundTrip();
    void fileThatCantBeReadIsAnError();
    void fileThatDoesntParseIsLeftAlone();
    void fileInAReadOnlyFolderIsLeftAlone();
    void rootSetExistingKey();
    void rootAddKeyAfterLastKey();
    void rootAddKeyToCommentOnlyFile();
    void rootAddKeyToEmptyFile();
    void rootAddKeyBeforeSection();
    void rootAddKeyWithSectionOnly();
    void rootAddKeyAfterMultilineValue();
    void rootSetKeyNamingSectionFails();
    void rootRemoveKey();
    void rootRemoveLeavesSectionKey();
    void blockSetExistingKey();
    void blockSetMultilineArray();
    void blockAddKey();
    void blockAddKeyToLastBlockWithoutNewline();
    void blockSetMissingBlockAppendsIt();
    void blockSetKeyNamingTableFails();
    void blockRemoveKey();
    void blockRemoveMissingIsNoop();
    void blockMatchesOtherKey();
    void appendBlockToFile();
    void appendBlockToEmptyFile();
    void removeFirstBlock();
    void removeMiddleBlock();
    void removeLastBlock();
    void removeOnlyBlock();
    void removeMissingBlockIsNoop();
    void moveBlockUp();
    void moveBlockDown();
    void moveBlockToEnd();
    void moveBlockBeforeItselfIsNoop();
    void moveMissingBlockFails();
    void blocksNextToSections();
    void blockKeysInFileOrder();
    void setValueSkipsSubTable();
    void setValueIntoSectionMadeOfSubTables();
    void appendBlockAfterSubTable();
    void removeBlockTakesSubTable();
    void removeBlockWithSameSubTableAbove();
    void moveBlockTakesSubTable();
    void setBlockValueSkipsSubTable();
    void invalidValueFails();
    void randomDocuments();
};

namespace {

// Comments above headers, a comment paragraph of its own, a commented-out key below a
// block's last key, and a multi-line array.
const QString kPlatforms = QStringLiteral(
    "# Platforms, in rail order.\n"
    "\n"
    "# Nintendo Switch\n"
    "[[platform]]\n"
    "id = \"switch\"\n"
    "name = \"Switch\"\n"
    "emulators = [\n"
    "  \"ryubing\",\n"
    "]\n"
    "# rom_folders = [\"~/old\"]\n"
    "\n"
    "# Handhelds below.\n"
    "\n"
    "[[platform]]\n"
    "id = \"3ds\"\n"
    "name = \"3DS\"\n"
    "\n"
    "# Steam\n"
    "[[platform]]\n"
    "id = \"steam\"\n"
    "name = \"Steam\"\n");

const QString kSwitchBlock = QStringLiteral(
    "# Nintendo Switch\n"
    "[[platform]]\n"
    "id = \"switch\"\n"
    "name = \"Switch\"\n"
    "emulators = [\n"
    "  \"ryubing\",\n"
    "]\n"
    "# rom_folders = [\"~/old\"]\n");

const QString k3dsBlock = QStringLiteral("[[platform]]\nid = \"3ds\"\nname = \"3DS\"\n");
const QString kSteamBlock = QStringLiteral("# Steam\n[[platform]]\nid = \"steam\"\nname = \"Steam\"\n");
const QString kHeader = QStringLiteral("# Platforms, in rail order.\n\n");
const QString kParagraph = QStringLiteral("# Handhelds below.\n\n");

TomlPatch::Block platform(const QString &id)
{
    return {QStringLiteral("platform"), id};
}

} // namespace

void TomlPatchTest::blockSetExistingKey()
{
    const auto r = TomlPatch::setBlockValue(kPlatforms, platform("3ds"), "name", "\"Nintendo 3DS\"");
    QVERIFY(r.ok);
    QString expected = kPlatforms;
    expected.replace("name = \"3DS\"", "name = \"Nintendo 3DS\"");
    QCOMPARE(r.text, expected);
}

void TomlPatchTest::blockSetMultilineArray()
{
    const auto r = TomlPatch::setBlockValue(kPlatforms, platform("switch"), "emulators",
                                            "[\"ryubing\", \"yuzu\"]");
    QVERIFY(r.ok);
    QString expected = kPlatforms;
    expected.replace("emulators = [\n  \"ryubing\",\n]", "emulators = [\"ryubing\", \"yuzu\"]");
    QCOMPARE(r.text, expected);
}

void TomlPatchTest::blockAddKey()
{
    // After the last key: below the multi-line array, above the commented-out key.
    auto r = TomlPatch::setBlockValue(kPlatforms, platform("switch"), "icon", "\"switch\"");
    QVERIFY(r.ok);
    QString expected = kPlatforms;
    expected.replace("]\n# rom_folders", "]\nicon = \"switch\"\n# rom_folders");
    QCOMPARE(r.text, expected);

    r = TomlPatch::setBlockValue(kPlatforms, platform("3ds"), "icon", "\"3ds\"");
    QVERIFY(r.ok);
    expected = kPlatforms;
    expected.replace("name = \"3DS\"\n", "name = \"3DS\"\nicon = \"3ds\"\n");
    QCOMPARE(r.text, expected);
}

void TomlPatchTest::blockAddKeyToLastBlockWithoutNewline()
{
    const QString in = QStringLiteral("[[platform]]\nid = \"switch\"");
    const auto r = TomlPatch::setBlockValue(in, platform("switch"), "name", "\"Switch\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[[platform]]\nid = \"switch\"\nname = \"Switch\"\n"));
}

void TomlPatchTest::blockSetMissingBlockAppendsIt()
{
    const TomlPatch::Block ryubing{QStringLiteral("emulator"), QStringLiteral("ryubing")};
    auto r = TomlPatch::setBlockValue(kPlatforms, ryubing, "args", "[\"--fullscreen\"]");
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms
                         + "\n[[emulator]]\nid = \"ryubing\"\nargs = [\"--fullscreen\"]\n");

    r = TomlPatch::setBlockValue(QStringLiteral("# Emulators\n"), ryubing, "path", "\"~/a\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("# Emulators\n\n[[emulator]]\nid = \"ryubing\"\npath = \"~/a\"\n"));
}

void TomlPatchTest::blockSetKeyNamingTableFails()
{
    const QString in = QStringLiteral("[[emulator]]\nid = \"x\"\n[emulator.saves]\nnative = \"~\"\n");
    const TomlPatch::Block x{QStringLiteral("emulator"), QStringLiteral("x")};
    QVERIFY(!TomlPatch::setBlockValue(in, x, "saves", "1").ok);
    const auto r = TomlPatch::removeBlockValue(in, x, "saves");
    QVERIFY(r.ok);
    QCOMPARE(r.text, in);
}

void TomlPatchTest::blockRemoveKey()
{
    auto r = TomlPatch::removeBlockValue(kPlatforms, platform("switch"), "emulators");
    QVERIFY(r.ok);
    QString expected = kPlatforms;
    expected.remove("emulators = [\n  \"ryubing\",\n]\n");
    QCOMPARE(r.text, expected);

    r = TomlPatch::removeBlockValue(kPlatforms, platform("steam"), "name");
    QVERIFY(r.ok);
    expected = kPlatforms;
    expected.remove("name = \"Steam\"\n");
    QCOMPARE(r.text, expected);
}

void TomlPatchTest::blockRemoveMissingIsNoop()
{
    auto r = TomlPatch::removeBlockValue(kPlatforms, platform("3ds"), "icon");
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms);
    r = TomlPatch::removeBlockValue(kPlatforms, platform("wii"), "name");
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms);
}

void TomlPatchTest::blockMatchesOtherKey()
{
    const TomlPatch::Block byName{QStringLiteral("platform"), QStringLiteral("3DS"),
                                  QStringLiteral("name")};
    const auto r = TomlPatch::setBlockValue(kPlatforms, byName, "icon", "\"3ds\"");
    QVERIFY(r.ok);
    QVERIFY(r.text.contains("name = \"3DS\"\nicon = \"3ds\"\n"));
}

void TomlPatchTest::appendBlockToFile()
{
    auto r = TomlPatch::appendBlock(kPlatforms, "platform", {{"id", "\"wii\""}, {"name", "\"Wii\""}});
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms + "\n[[platform]]\nid = \"wii\"\nname = \"Wii\"\n");

    // A new array goes at the end, without piling up blank lines.
    r = TomlPatch::appendBlock(kPlatforms + "\n\n", "emulator", {{"id", "\"x\""}});
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms + "\n[[emulator]]\nid = \"x\"\n");
}

void TomlPatchTest::appendBlockToEmptyFile()
{
    auto r = TomlPatch::appendBlock({}, "platform", {{"id", "\"wii\""}});
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[[platform]]\nid = \"wii\"\n"));

    r = TomlPatch::appendBlock(QStringLiteral("[[platform"), "platform", {{"id", "\"wii\""}});
    QVERIFY(!r.ok);
}

void TomlPatchTest::removeFirstBlock()
{
    // Its comment and commented-out key go with it; the paragraph and header comment stay.
    const auto r = TomlPatch::removeBlock(kPlatforms, platform("switch"));
    QVERIFY(r.ok);
    QCOMPARE(r.text, kHeader + kParagraph + k3dsBlock + "\n" + kSteamBlock);
}

void TomlPatchTest::removeMiddleBlock()
{
    const auto r = TomlPatch::removeBlock(kPlatforms, platform("3ds"));
    QVERIFY(r.ok);
    QCOMPARE(r.text, kHeader + kSwitchBlock + "\n" + kParagraph + kSteamBlock);
}

void TomlPatchTest::removeLastBlock()
{
    const auto r = TomlPatch::removeBlock(kPlatforms, platform("steam"));
    QVERIFY(r.ok);
    QCOMPARE(r.text, kHeader + kSwitchBlock + "\n" + kParagraph + k3dsBlock);
}

void TomlPatchTest::removeOnlyBlock()
{
    const QString in = QStringLiteral("# Emulators\n\n[[emulator]]\nid = \"x\"\n\n");
    const auto r = TomlPatch::removeBlock(in, {QStringLiteral("emulator"), QStringLiteral("x")});
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("# Emulators\n"));
}

void TomlPatchTest::removeMissingBlockIsNoop()
{
    const auto r = TomlPatch::removeBlock(kPlatforms, platform("wii"));
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms);
}

void TomlPatchTest::moveBlockUp()
{
    const auto r = TomlPatch::moveBlock(kPlatforms, platform("steam"), "switch");
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.text, kHeader + kSteamBlock + "\n" + kSwitchBlock + "\n" + kParagraph + k3dsBlock);
}

void TomlPatchTest::moveBlockDown()
{
    const auto r = TomlPatch::moveBlock(kPlatforms, platform("switch"), "steam");
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.text, kHeader + kParagraph + k3dsBlock + "\n" + kSwitchBlock + "\n" + kSteamBlock);
}

void TomlPatchTest::moveBlockToEnd()
{
    const auto r = TomlPatch::moveBlock(kPlatforms, platform("switch"), {});
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.text, kHeader + kParagraph + k3dsBlock + "\n" + kSteamBlock + "\n" + kSwitchBlock);
}

void TomlPatchTest::moveBlockBeforeItselfIsNoop()
{
    const auto r = TomlPatch::moveBlock(kPlatforms, platform("3ds"), "3ds");
    QVERIFY(r.ok);
    QCOMPARE(r.text, kPlatforms);
}

void TomlPatchTest::moveMissingBlockFails()
{
    QVERIFY(!TomlPatch::moveBlock(kPlatforms, platform("wii"), "switch").ok);
    QVERIFY(!TomlPatch::moveBlock(kPlatforms, platform("switch"), "wii").ok);
}

void TomlPatchTest::blockKeysInFileOrder()
{
    QCOMPARE(TomlPatch::blockKeys(kPlatforms, platform("switch")),
             QStringList({"id", "name", "emulators"}));
    QCOMPARE(TomlPatch::blockKeys(kPlatforms, platform("wii")), QStringList());
    QCOMPARE(TomlPatch::blockValues(kPlatforms, "platform", "id"), QStringList({"switch", "3ds", "steam"}));
    QCOMPARE(TomlPatch::blockValues(kPlatforms, "emulator", "id"), QStringList());
    QCOMPARE(TomlPatch::blockKeys(QStringLiteral("[[platform"), platform("wii")), QStringList());
}

void TomlPatchTest::blocksNextToSections()
{
    // A [section] after the blocks ends the last block; its keys aren't the block's.
    const QString in = QStringLiteral(
        "[[platform]]\nid = \"a\"\n\n[[platform]]\nid = \"b\"\n\n[launch]\nwhile_running = \"stay\"\n");
    auto r = TomlPatch::setBlockValue(in, platform("b"), "name", "\"B\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral(
        "[[platform]]\nid = \"a\"\n\n[[platform]]\nid = \"b\"\nname = \"B\"\n\n"
        "[launch]\nwhile_running = \"stay\"\n"));

    // New and moved blocks go below the array's last block, above the section.
    r = TomlPatch::moveBlock(in, platform("a"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral(
        "[[platform]]\nid = \"b\"\n\n[[platform]]\nid = \"a\"\n\n[launch]\nwhile_running = \"stay\"\n"));

    r = TomlPatch::moveBlock(in, platform("b"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.text, in);

    r = TomlPatch::appendBlock(in, "platform", {{"id", "\"c\""}});
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral(
        "[[platform]]\nid = \"a\"\n\n[[platform]]\nid = \"b\"\n\n[[platform]]\nid = \"c\"\n\n"
        "[launch]\nwhile_running = \"stay\"\n"));
}

void TomlPatchTest::rootSetExistingKey()
{
    const QString in = QStringLiteral("# Hades\ntitle = \"Hades\"\nfavorite = false # star\n");
    const auto r = TomlPatch::setValue(in, {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("# Hades\ntitle = \"Hades\"\nfavorite = true # star\n"));
}

void TomlPatchTest::rootAddKeyAfterLastKey()
{
    const QString in = QStringLiteral("title = \"Hades\"\nyear = 2020\n\n# notes\n");
    const auto r = TomlPatch::setValue(in, {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("title = \"Hades\"\nyear = 2020\nfavorite = true\n\n# notes\n"));
}

void TomlPatchTest::rootAddKeyToCommentOnlyFile()
{
    const QString in = QStringLiteral("# My notes\n# more\n\n# later\n");
    const auto r = TomlPatch::setValue(in, {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("# My notes\n# more\nfavorite = true\n\n# later\n"));
}

void TomlPatchTest::rootAddKeyToEmptyFile()
{
    auto r = TomlPatch::setValue({}, {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("favorite = true\n"));

    r = TomlPatch::setValue(QStringLiteral("# only a comment"), {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("# only a comment\nfavorite = true\n"));
}

void TomlPatchTest::rootAddKeyBeforeSection()
{
    const QString in = QStringLiteral("title = \"Hades\"\n\n[extra]\nnote = 1\n");
    const auto r = TomlPatch::setValue(in, {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("title = \"Hades\"\nfavorite = true\n\n[extra]\nnote = 1\n"));
}

void TomlPatchTest::rootAddKeyWithSectionOnly()
{
    const QString in = QStringLiteral("# Header\n\n[[platform]]\nid = \"switch\"\n");
    const auto r = TomlPatch::setValue(in, {}, "version", "1");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("# Header\nversion = 1\n\n[[platform]]\nid = \"switch\"\n"));
}

void TomlPatchTest::rootAddKeyAfterMultilineValue()
{
    const QString in = QStringLiteral("description = \"\"\"\nLine one\n[not a section]\"\"\"\n");
    const auto r = TomlPatch::setValue(in, {}, "favorite", "true");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral(
        "description = \"\"\"\nLine one\n[not a section]\"\"\"\nfavorite = true\n"));
}

void TomlPatchTest::rootSetKeyNamingSectionFails()
{
    const QString in = QStringLiteral("[extra]\nnote = 1\n");
    const auto r = TomlPatch::setValue(in, {}, "extra", "true");
    QVERIFY(!r.ok);
}

void TomlPatchTest::rootRemoveKey()
{
    const QString in = QStringLiteral("title = \"Hades\"\nfavorite = true # star\nyear = 2020\n");
    const auto r = TomlPatch::removeValue(in, {}, "favorite");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("title = \"Hades\"\nyear = 2020\n"));
}

void TomlPatchTest::rootRemoveLeavesSectionKey()
{
    const QString in = QStringLiteral("title = \"Hades\"\n\n[extra]\nnote = 1\n");
    const auto r = TomlPatch::removeValue(in, {}, "extra");
    QVERIFY(r.ok);
    QCOMPARE(r.text, in);
}

void TomlPatchTest::setExistingKey()
{
    const QString in = QStringLiteral("[palette]\naccent = \"#1a8cff\"\n");
    const auto r = TomlPatch::setValue(in, "palette", "accent", "\"#ff0000\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[palette]\naccent = \"#ff0000\"\n"));
}

void TomlPatchTest::setExistingKeyPreservesTrailingComment()
{
    const QString in = QStringLiteral("[layout]\nradius = 8 # corners\nspacing = 4\n");
    const auto r = TomlPatch::setValue(in, "layout", "radius", "12");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[layout]\nradius = 12 # corners\nspacing = 4\n"));
}

void TomlPatchTest::setKeyMissingInExistingSection()
{
    const QString in = QStringLiteral("[layout]\nradius = 8\nspacing = 4\n");
    const auto r = TomlPatch::setValue(in, "layout", "margin", "16");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[layout]\nradius = 8\nspacing = 4\nmargin = 16\n"));
}

void TomlPatchTest::setKeyInEmptySection()
{
    const QString in = QStringLiteral("extends = \"default\"\n\n[palette]\n\n[layout]\nradius = 8\n");
    const auto r = TomlPatch::setValue(in, "palette", "accent", "\"#ff0000\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral(
        "extends = \"default\"\n\n[palette]\naccent = \"#ff0000\"\n\n[layout]\nradius = 8\n"));
}

void TomlPatchTest::setKeyMissingSection()
{
    const QString in = QStringLiteral("extends = \"default\"\n\n[layout]\nradius = 8\n");
    const auto r = TomlPatch::setValue(in, "palette", "accent", "\"#ff0000\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral(
        "extends = \"default\"\n\n[layout]\nradius = 8\n\n[palette]\naccent = \"#ff0000\"\n"));
}

void TomlPatchTest::setKeyMissingSectionNoTrailingNewline()
{
    const QString in = QStringLiteral("extends = \"default\"");
    const auto r = TomlPatch::setValue(in, "palette", "accent", "\"#ff0000\"");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("extends = \"default\"\n\n[palette]\naccent = \"#ff0000\"\n"));
}

void TomlPatchTest::setList()
{
    const QString in = QStringLiteral("[details]\nstats = [\"play_time\"]\n");
    const auto r = TomlPatch::setValue(in, "details", "stats", "[\"play_time\", \"last_played\"]");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[details]\nstats = [\"play_time\", \"last_played\"]\n"));
}

void TomlPatchTest::removeExistingKey()
{
    const QString in = QStringLiteral("[palette]\naccent = \"#ff0000\"\n");
    const auto r = TomlPatch::removeValue(in, "palette", "accent");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[palette]\n"));
}

void TomlPatchTest::removeMiddleKeyKeepsOthers()
{
    const QString in = QStringLiteral("[layout]\nmargin = 16\nradius = 8\nspacing = 4\n");
    const auto r = TomlPatch::removeValue(in, "layout", "radius");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[layout]\nmargin = 16\nspacing = 4\n"));
}

void TomlPatchTest::removeKeyWithComment()
{
    const QString in = QStringLiteral("[layout]\nradius = 8 # corners\nspacing = 4\n");
    const auto r = TomlPatch::removeValue(in, "layout", "radius");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[layout]\nspacing = 4\n"));
}

void TomlPatchTest::removeMultilineList()
{
    const QString in = QStringLiteral(
        "[details]\nstats = [\n  \"play_time\",\n  \"last_played\",\n]\nother = true\n");
    const auto r = TomlPatch::removeValue(in, "details", "stats");
    QVERIFY(r.ok);
    QCOMPARE(r.text, QStringLiteral("[details]\nother = true\n"));
}

void TomlPatchTest::removeMissingKeyIsNoop()
{
    const QString in = QStringLiteral("[layout]\nradius = 8\n");
    const auto r = TomlPatch::removeValue(in, "layout", "margin");
    QVERIFY(r.ok);
    QCOMPARE(r.text, in);
}

void TomlPatchTest::removeMissingSectionIsNoop()
{
    const QString in = QStringLiteral("[layout]\nradius = 8\n");
    const auto r = TomlPatch::removeValue(in, "rail", "width");
    QVERIFY(r.ok);
    QCOMPARE(r.text, in);
}

void TomlPatchTest::parseErrorIsReported()
{
    const QString in = QStringLiteral("[layout\nradius = 8\n");
    const auto r = TomlPatch::setValue(in, "layout", "radius", "12");
    QVERIFY(!r.ok);
    QVERIFY(!r.error.isEmpty());
}

void TomlPatchTest::fileRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("custom.toml");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("extends = \"default\"\n\n[palette]\naccent = \"#1a8cff\"\n");
    file.close();

    QString error;
    QVERIFY(TomlPatch::setValueInFile(path, "palette", "accent", "\"#ff0000\"", &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QFile readBack(path);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(readBack.readAll()),
             QStringLiteral("extends = \"default\"\n\n[palette]\naccent = \"#ff0000\"\n"));

    QVERIFY(TomlPatch::removeValueInFile(path, "palette", "accent", &error));
    QFile readBack2(path);
    QVERIFY(readBack2.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(readBack2.readAll()),
             QStringLiteral("extends = \"default\"\n\n[palette]\n"));
}

namespace {
const QString kSteamOld = QStringLiteral(
    "[steam]\n"
    "enabled = true\n"
    "\n"
    "[steam.presets.gamescope]\n"
    "label = \"Gamescope\"\n"
    "\n"
    "[other]\n"
    "x = 1\n");

const QString kEmulators = QStringLiteral(
    "[[emulator]]\n"
    "id = \"a\"\n"
    "\n"
    "[emulator.platform_extensions]\n"
    "switch = [\"nsp\"]\n"
    "\n"
    "# second\n"
    "[[emulator]]\n"
    "id = \"b\"\n"
    "\n"
    "[emulator.platform_extensions]\n"
    "gba = [\"gba\"]\n");

TomlPatch::Block emulator(const QString &id)
{
    return {QStringLiteral("emulator"), id};
}

void expectSameTable(const QString &text, const QString &expected)
{
    QVERIFY(toml::parse(text.toStdString()) == toml::parse(expected.toStdString()));
}
}

void TomlPatchTest::setValueSkipsSubTable()
{
    const auto r = TomlPatch::setValue(kSteamOld, "steam", "default_presets", "[\"gamescope\"]");
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(r.text.contains(
        QStringLiteral("enabled = true\ndefault_presets = [\"gamescope\"]\n\n[steam.presets")));
}

void TomlPatchTest::setValueIntoSectionMadeOfSubTables()
{
    const auto r = TomlPatch::setValue(QStringLiteral("[a.b]\nx = 1\n"), "a", "k", "2");
    QVERIFY2(r.ok, qPrintable(r.error));
    expectSameTable(r.text, QStringLiteral("[a]\nk = 2\n[a.b]\nx = 1\n"));
}

void TomlPatchTest::appendBlockAfterSubTable()
{
    const auto r = TomlPatch::appendBlock(kEmulators, "emulator", {{"id", "\"c\""}});
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(r.text.endsWith(QStringLiteral("gba = [\"gba\"]\n\n[[emulator]]\nid = \"c\"\n")));
}

void TomlPatchTest::removeBlockTakesSubTable()
{
    const auto r = TomlPatch::removeBlock(kEmulators, emulator("a"));
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.text, QStringLiteral("# second\n[[emulator]]\nid = \"b\"\n\n"
                                    "[emulator.platform_extensions]\ngba = [\"gba\"]\n"));
}

void TomlPatchTest::removeBlockWithSameSubTableAbove()
{
    const auto r = TomlPatch::removeBlock(kEmulators, emulator("b"));
    QVERIFY2(r.ok, qPrintable(r.error));
    expectSameTable(r.text, QStringLiteral("[[emulator]]\nid = \"a\"\n"
                                           "[emulator.platform_extensions]\nswitch = [\"nsp\"]\n"));
}

void TomlPatchTest::moveBlockTakesSubTable()
{
    const auto r = TomlPatch::moveBlock(kEmulators, emulator("a"), QString());
    QVERIFY2(r.ok, qPrintable(r.error));
    expectSameTable(r.text, QStringLiteral(
        "[[emulator]]\nid = \"b\"\n[emulator.platform_extensions]\ngba = [\"gba\"]\n"
        "[[emulator]]\nid = \"a\"\n[emulator.platform_extensions]\nswitch = [\"nsp\"]\n"));
}

void TomlPatchTest::setBlockValueSkipsSubTable()
{
    const auto r = TomlPatch::setBlockValue(kEmulators, emulator("a"), "name", "\"A\"");
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(r.text.contains(
        QStringLiteral("id = \"a\"\nname = \"A\"\n\n[emulator.platform_extensions]")));
}

void TomlPatchTest::invalidValueFails()
{
    QVERIFY(!TomlPatch::setValue(kSteamOld, "steam", "k", "not a value").ok);
    QVERIFY(!TomlPatch::setValue(kSteamOld, "steam", "k", "1\n[evil]").ok);
    QVERIFY(!TomlPatch::appendBlock(kEmulators, "emulator", {{"id", "\"c\"\n[x]"}}).ok);
}

namespace {
using Keys = std::map<std::string, int>;

struct Section
{
    bool header = true;
    Keys keys;
    bool hasSub = false;
    Keys subKeys;
};

struct BlockModel
{
    std::string id;
    Keys keys;
    bool hasSub = false;
    Keys subKeys;
};

struct Model
{
    std::map<std::string, Section> sections;
    std::vector<BlockModel> blocks;
};

// With `rng`, the text gets comments and blank lines the patch has to keep out of the way.
std::string keysText(const Keys &keys, std::mt19937 *rng)
{
    std::string out;
    for (const auto &[k, v] : keys) {
        out += k + " = " + std::to_string(v) + "\n";
        if (rng && (*rng)() % 5 == 0)
            out += "# note\n";
        if (rng && (*rng)() % 7 == 0)
            out += "\n";
    }
    return out;
}

std::string sectionText(const std::string &name, const Section &s, std::mt19937 *rng)
{
    std::string out;
    if (rng && (*rng)() % 3 == 0)
        out += "# about " + name + "\n";
    if (s.header)
        out += "[" + name + "]\n" + keysText(s.keys, rng);
    if (s.hasSub)
        out += "[" + name + ".sub]\n" + keysText(s.subKeys, rng);
    if (rng && (*rng)() % 2 == 0)
        out += "\n";
    return out;
}

std::string blockText(const BlockModel &b, std::mt19937 *rng)
{
    std::string out;
    if (rng && (*rng)() % 3 == 0)
        out += "# about " + b.id + "\n";
    out += "[[blk]]\nid = \"" + b.id + "\"\n" + keysText(b.keys, rng);
    if (b.hasSub)
        out += "[blk.sub]\n" + keysText(b.subKeys, rng);
    if (rng && (*rng)() % 2 == 0)
        out += "\n";
    return out;
}

std::string modelText(const Model &m, std::mt19937 *rng)
{
    std::vector<std::string> names;
    for (const auto &entry : m.sections)
        names.push_back(entry.first);
    std::string out;
    size_t si = 0;
    size_t bi = 0;
    while (si < names.size() || bi < m.blocks.size()) {
        const bool section = bi == m.blocks.size()
            || (si < names.size() && (!rng || (*rng)() % 2 == 0));
        if (section) {
            out += sectionText(names[si], m.sections.at(names[si]), rng);
            si++;
        } else {
            out += blockText(m.blocks[bi++], rng);
        }
    }
    return out;
}

Keys randomKeys(std::mt19937 &rng)
{
    Keys keys;
    const int n = rng() % 4;
    for (int i = 0; i < n; ++i)
        keys["k" + std::to_string(rng() % 5)] = int(rng() % 100);
    return keys;
}

Model randomModel(std::mt19937 &rng)
{
    Model m;
    const int sections = rng() % 4;
    for (int i = 0; i < sections; ++i) {
        Section s;
        s.keys = randomKeys(rng);
        s.hasSub = rng() % 2;
        if (s.hasSub)
            s.subKeys = randomKeys(rng);
        s.header = !s.hasSub || !s.keys.empty() || rng() % 2;
        m.sections["s" + std::to_string(i)] = s;
    }
    const int blocks = rng() % 5;
    for (int i = 0; i < blocks; ++i) {
        BlockModel b;
        b.id = "b" + std::to_string(i);
        b.keys = randomKeys(rng);
        b.hasSub = rng() % 2;
        if (b.hasSub)
            b.subKeys = randomKeys(rng);
        m.blocks.push_back(b);
    }
    return m;
}
}

// Random documents with sub-tables, edited four times each: every edit must succeed and parse to
// the table a model of the edit says, checked here with toml++ and not with the patch's own check.
void TomlPatchTest::randomDocuments()
{
    std::mt19937 rng(20261124);
    int checked = 0;
    for (int doc = 0; doc < 3000; ++doc) {
        Model model = randomModel(rng);
        QString text = QString::fromStdString(modelText(model, &rng));
        for (int step = 0; step < 4; ++step) {
            const int op = rng() % 5;
            TomlPatch::Result r;
            if (op == 0) {
                const std::string name = "s" + std::to_string(rng() % 5);
                const std::string key = "k" + std::to_string(rng() % 5);
                const int value = int(rng() % 100);
                r = TomlPatch::setValue(text, QString::fromStdString(name),
                                        QString::fromStdString(key), QString::number(value));
                Section &s = model.sections[name];
                s.keys[key] = value;
                s.header = true;
            } else if (op == 1) {
                const std::string id = "n" + std::to_string(doc * 10 + step);
                r = TomlPatch::appendBlock(
                    text, "blk", {{"id", "\"" + QString::fromStdString(id) + "\""}, {"k0", "1"}});
                BlockModel b;
                b.id = id;
                b.keys["k0"] = 1;
                model.blocks.push_back(b);
            } else if (model.blocks.empty()) {
                continue;
            } else {
                const size_t i = rng() % model.blocks.size();
                const TomlPatch::Block block{"blk", QString::fromStdString(model.blocks[i].id)};
                if (op == 2) {
                    r = TomlPatch::removeBlock(text, block);
                    model.blocks.erase(model.blocks.begin() + i);
                } else if (op == 3) {
                    const std::string key = "k" + std::to_string(rng() % 5);
                    const int value = int(rng() % 100);
                    r = TomlPatch::setBlockValue(text, block, QString::fromStdString(key),
                                                 QString::number(value));
                    model.blocks[i].keys[key] = value;
                } else {
                    const size_t j = rng() % (model.blocks.size() + 1);
                    const bool toEnd = j == model.blocks.size();
                    const std::string before = toEnd ? "" : model.blocks[j].id;
                    r = TomlPatch::moveBlock(text, block, QString::fromStdString(before));
                    if (j != i) {
                        BlockModel moving = model.blocks[i];
                        model.blocks.erase(model.blocks.begin() + i);
                        auto at = model.blocks.end();
                        if (!toEnd) {
                            at = std::find_if(model.blocks.begin(), model.blocks.end(),
                                              [&](const BlockModel &b) { return b.id == before; });
                        }
                        model.blocks.insert(at, moving);
                    }
                }
            }
            QVERIFY2(r.ok, qPrintable(QStringLiteral("doc %1 step %2 op %3: %4\n%5")
                                          .arg(doc).arg(step).arg(op).arg(r.error, text)));
            const QString expected = QString::fromStdString(modelText(model, nullptr));
            if (toml::parse(r.text.toStdString()) != toml::parse(expected.toStdString())) {
                QFAIL(qPrintable(QStringLiteral("doc %1 step %2 op %3 differs:\n%4\nfrom:\n%5")
                                     .arg(doc).arg(step).arg(op).arg(r.text, text)));
            }
            text = r.text;
            checked++;
        }
    }
    QVERIFY(checked > 5000);
}

namespace {
QByteArray contentOf(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void put(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
}
}

void TomlPatchTest::fileThatCantBeReadIsAnError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString missing = dir.filePath("missing.toml");
    QString error;
    QVERIFY(!TomlPatch::setValueInFile(missing, "palette", "accent", "\"#ff0000\"", &error));
    QCOMPARE(error, QStringLiteral("could not read %1").arg(missing));
    QVERIFY(!QFileInfo::exists(missing));
    error.clear();
    QVERIFY(!TomlPatch::removeValueInFile(missing, "palette", "accent", &error));
    QCOMPARE(error, QStringLiteral("could not read %1").arg(missing));
    QVERIFY(!QFileInfo::exists(missing));

    // No error to fill in is fine too.
    QVERIFY(!TomlPatch::setValueInFile(missing, "palette", "accent", "\"#ff0000\""));
}

void TomlPatchTest::fileThatDoesntParseIsLeftAlone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("custom.toml");
    const QByteArray broken = "[palette\naccent = \"#1a8cff\"\n";
    put(path, broken);
    QString error;
    QVERIFY(!TomlPatch::setValueInFile(path, "palette", "accent", "\"#ff0000\"", &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(contentOf(path), broken);
    error.clear();
    QVERIFY(!TomlPatch::removeValueInFile(path, "palette", "accent", &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(contentOf(path), broken);

    // A value that would break the file is refused the same way.
    put(path, "[palette]\naccent = \"#1a8cff\"\n");
    error.clear();
    QVERIFY(!TomlPatch::setValueInFile(path, "palette", "accent", "1\n[evil]", &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(contentOf(path), QByteArray("[palette]\naccent = \"#1a8cff\"\n"));
}

void TomlPatchTest::fileInAReadOnlyFolderIsLeftAlone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.filePath("ro");
    QVERIFY(QDir().mkpath(folder));
    const QString path = folder + "/custom.toml";
    const QByteArray before = "[palette]\naccent = \"#1a8cff\"\n";
    put(path, before);
    QVERIFY(QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto writable = [&] {
        QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    };
    if (QFileInfo(folder).isWritable()) {
        writable();
        QSKIP("the folder stays writable (running as root)");
    }

    // Unchanged: nothing to write, so it works.
    QString error;
    QVERIFY(TomlPatch::setValueInFile(path, "palette", "accent", "\"#1a8cff\"", &error));
    QVERIFY(TomlPatch::removeValueInFile(path, "palette", "missing", &error));
    QVERIFY(error.isEmpty());

    QVERIFY(!TomlPatch::setValueInFile(path, "palette", "accent", "\"#ff0000\"", &error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!TomlPatch::removeValueInFile(path, "palette", "accent", &error));
    QVERIFY(!error.isEmpty());
    const QStringList left = QDir(folder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    writable();
    QCOMPARE(contentOf(path), before);
    QCOMPARE(left, QStringList{"custom.toml"});
}

QTEST_MAIN(TomlPatchTest)
#include "tomlpatch_test.moc"
