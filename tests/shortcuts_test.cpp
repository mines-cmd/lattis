#include "binaryvdf.h"
#include "binaryvdfbuilder.h"
#include "steamshortcuts.h"

#include <QFile>
#include <QTest>

using namespace Qt::StringLiterals;
using BinaryVdf::Node;

class ShortcutsTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripBuilt();
    void roundTripFakeTree();
    void realFile();
    void realFileEdits();
    void writeFromNodes();
    void childIgnoresCase();
    void truncated();
    void garbage();
    void readShortcuts();
    void appIds();
    void newAppId();
    void splitExe();
    void addShortcut();
    void addToEmptyFile();
    void updateOneField();
    void updateKeepsQuoting();
    void updateUnchanged();
    void updateInsertsMissingField();
    void removeRenumbers();
};

namespace {

const QString kFakeShortcuts = QStringLiteral(STEAM_DATA_DIR "/userdata/39734273/config/shortcuts.vdf");
// A trimmed copy of a real shortcuts.vdf: Steam's own shortcuts, Steam ROM Manager's, and
// ones with arguments inside `exe`; paths changed to /home/user.
const QString kRealShortcuts = QStringLiteral(VDF_DATA_DIR "/shortcuts.vdf");

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

// Every value type, unknown field names, nesting, and the fields in an odd order.
QByteArray everyType()
{
    VdfBytes b;
    b.map("root")
        .str("name", "Caf\xc3\xa9 \"quoted\"")
        .str("empty", "")
        .i32("negative", -1974090208)
        .f32("scale", 1.5f)
        .u64("big", 0xFEDCBA9876543210ull)
        .i64("signed64", -5)
        .map("nested")
        .map("deeper")
        .str("SomethingNew", "kept")
        .end()
        .end()
        .i32("zAfter", 7)
        .end();
    b.str("second", "top level");
    return b.end().bytes();
}

BinaryVdf::Document parseFile(const QString &path)
{
    return BinaryVdf::parse(readAll(path));
}

// A shortcut's fields as bytes, to show an edit didn't touch them.
QByteArray bytesOf(const Node &shortcut)
{
    Node holder;
    holder.children = {shortcut};
    return BinaryVdf::write(holder);
}

const Node &shortcutAt(const BinaryVdf::Document &doc, int i)
{
    return doc.root.child("shortcuts")->children.at(i);
}

} // namespace

void ShortcutsTest::roundTripBuilt()
{
    const QByteArray bytes = everyType();
    const BinaryVdf::Document doc = BinaryVdf::parse(bytes);
    QVERIFY2(doc.ok, qPrintable(doc.error));
    QCOMPARE(BinaryVdf::write(doc.root), bytes);

    const Node *root = doc.root.child("root");
    QVERIFY(root);
    QCOMPARE(root->children.size(), 8);
    QCOMPARE(root->child("name")->toString(), u"Café \"quoted\""_s);
    QCOMPARE(root->child("negative")->toInt32(), -1974090208);
    QCOMPARE(root->child("big")->toUInt64(), 0xFEDCBA9876543210ull);
    QCOMPARE(root->child("scale")->type, BinaryVdf::Float);
    QCOMPARE(root->child("signed64")->type, BinaryVdf::Int64);
    QCOMPARE(root->child("nested")->child("deeper")->child("SomethingNew")->toString(), u"kept"_s);
    QCOMPARE(root->children.last().key, QByteArray("zAfter"));
    QCOMPARE(doc.root.child("second")->toString(), u"top level"_s);
    // Another type's value isn't read as this one.
    QCOMPARE(root->child("name")->toInt32(), 0);
    QCOMPARE(root->child("negative")->toString(), QString());
}

void ShortcutsTest::roundTripFakeTree()
{
    // The fake tree's file is the one fakeTreeShortcuts() describes.
    const QByteArray bytes = readAll(kFakeShortcuts);
    QCOMPARE(bytes, fakeTreeShortcuts());
    const BinaryVdf::Document doc = BinaryVdf::parse(bytes);
    QVERIFY2(doc.ok, qPrintable(doc.error));
    QCOMPARE(BinaryVdf::write(doc.root), bytes);
}

void ShortcutsTest::realFile()
{
    const QByteArray bytes = readAll(kRealShortcuts);
    const BinaryVdf::Document doc = BinaryVdf::parse(bytes);
    QVERIFY2(doc.ok, qPrintable(doc.error));
    QCOMPARE(BinaryVdf::write(doc.root), bytes);

    const QList<SteamShortcut> s = SteamShortcuts::read(doc.root);
    QCOMPARE(s.size(), 5);
    QList<QString> names;
    for (const SteamShortcut &c : s)
        names.append(c.name);
    QCOMPARE(names, (QList<QString>{u"Fluffy MM"_s, u"ShinRyuModManager.exe"_s, u"Monster Hunter 4 Ultimate"_s,
                                    u"Super Smash Bros. Ultimate"_s, u"Mortal Shell 2"_s}));

    // Made by Steam: exe quoted, StartDir not (even with spaces), with a trailing slash.
    const SteamShortcut &shell = s.at(4);
    QCOMPARE(shell.appId, 2320877088u);
    QCOMPARE(shell.exe, u"/home/user/Games/Mortal Shell II/MortalShell2.exe"_s);
    QVERIFY(shell.exeArgs.isEmpty());
    QCOMPARE(shell.startDir, u"/home/user/Games/Mortal Shell II/"_s);
    QCOMPARE(shell.launchOptions, u" mangohud %command%"_s);
    QVERIFY(shell.lastPlayTime > 0);
    QVERIFY(!shell.hidden);
    QCOMPARE(s.at(1).startDir, u"/home/user/Steam/Lost Judgment/runtime/media/"_s);

    // Made by Steam ROM Manager: unquoted, the ROM in the launch options.
    const SteamShortcut &mh4u = s.at(2);
    QCOMPARE(mh4u.appId, 2976696489u);
    QCOMPARE(mh4u.exe, u"/home/user/AppImages/azahar.appimage"_s);
    QVERIFY(mh4u.exeArgs.isEmpty());
    QCOMPARE(mh4u.startDir, u"/home/user/AppImages"_s);
    QCOMPARE(mh4u.icon, u"/home/user/AppImages/.icons/citra"_s);
    QVERIFY(mh4u.launchOptions.endsWith(u".3ds\""_s));

    // The ROM inside exe, after the program.
    const SteamShortcut &smash = s.at(3);
    QCOMPARE(smash.appId, 4253812108u);
    QCOMPARE(smash.exe, u"/home/user/AppImages/ryujinx.appimage"_s);
    QVERIFY(smash.exeArgs.startsWith(u"--fullscreen \"/home/user/Games/switch/Super Smash Bros Ultimate"_s));
    QVERIFY(smash.exeArgs.endsWith(u".nsp\""_s));
    QVERIFY(smash.launchOptions.isEmpty());
    QCOMPARE(SteamShortcuts::read(doc.root).at(0).appId, 3193880877u);
}

void ShortcutsTest::realFileEdits()
{
    const QByteArray bytes = readAll(kRealShortcuts);
    BinaryVdf::Document doc = BinaryVdf::parse(bytes);
    QVERIFY(doc.ok);
    const BinaryVdf::Document before = doc;
    QList<SteamShortcut> shortcuts = SteamShortcuts::read(doc.root);

    // Writing back every shortcut as read changes nothing.
    for (const SteamShortcut &s : std::as_const(shortcuts))
        QVERIFY(SteamShortcuts::update(&doc.root, s));
    QCOMPARE(BinaryVdf::write(doc.root), bytes);

    // Editing one: the other four keep every byte.
    SteamShortcut shell = shortcuts.at(4);
    shell.launchOptions = u"gamemoderun %command%"_s;
    shell.name = u"Mortal Shell II"_s;
    QVERIFY(SteamShortcuts::update(&doc.root, shell));
    for (int i = 0; i < 4; ++i)
        QCOMPARE(bytesOf(shortcutAt(doc, i)), bytesOf(shortcutAt(before, i)));
    QCOMPARE(SteamShortcuts::read(BinaryVdf::parse(BinaryVdf::write(doc.root)).root).at(4), shell);

    // The Smash shortcut's program changes; the ROM after it stays.
    SteamShortcut smash = shortcuts.at(3);
    smash.exe = u"/home/user/AppImages/ryubing.appimage"_s;
    QVERIFY(SteamShortcuts::update(&doc.root, smash));
    QCOMPARE(SteamShortcuts::read(doc.root).at(3).exeArgs, shortcuts.at(3).exeArgs);

    // Added after them, then removed: the file as it was, but for the Mortal Shell edit.
    SteamShortcut added;
    added.name = u"New Game"_s;
    added.exe = u"/home/user/Games/New Game/game.exe"_s;
    added.startDir = u"/home/user/Games/New Game/"_s;
    QSet<quint32> taken;
    for (const SteamShortcut &s : std::as_const(shortcuts))
        taken.insert(s.appId);
    added.appId = SteamShortcuts::newAppId(SteamShortcuts::exeField(added.exe), added.name, taken);
    QVERIFY(!taken.contains(added.appId));
    QVERIFY(SteamShortcuts::add(&doc.root, added));
    QCOMPARE(doc.root.child("shortcuts")->children.last().key, QByteArray("5"));
    QVERIFY(SteamShortcuts::update(&doc.root, shortcuts.at(3)));
    QVERIFY(SteamShortcuts::remove(&doc.root, added.appId));
    QVERIFY(SteamShortcuts::update(&doc.root, shortcuts.at(4)));
    QCOMPARE(BinaryVdf::write(doc.root), bytes);

    // Removing the first renumbers the rest, their fields untouched.
    QVERIFY(SteamShortcuts::remove(&doc.root, shortcuts.at(0).appId));
    for (int i = 0; i < 4; ++i) {
        QCOMPARE(shortcutAt(doc, i).key, QByteArray::number(i));
        QCOMPARE(shortcutAt(doc, i).children, shortcutAt(before, i + 1).children);
    }
}

void ShortcutsTest::writeFromNodes()
{
    Node root;
    root.children = {Node::map("shortcuts", {Node::int32("appid", -1), Node::string("appname", u"Ünï"_s),
                                             Node::uint64("big", 42)})};
    const QByteArray expected =
        VdfBytes().map("shortcuts").i32("appid", -1).str("appname", "\xc3\x9cn\xc3\xaf").u64("big", 42).end().end().bytes();
    QCOMPARE(BinaryVdf::write(root), expected);
    QCOMPARE(BinaryVdf::write(Node()), QByteArray("\x08"));
}

void ShortcutsTest::childIgnoresCase()
{
    // Older Steams wrote "AppName"; the keys are matched as Steam does.
    const BinaryVdf::Document doc =
        BinaryVdf::parse(VdfBytes().map("shortcuts").str("AppName", "Old").end().end().bytes());
    QVERIFY(doc.ok);
    QCOMPARE(doc.root.child("SHORTCUTS")->child("appname")->toString(), u"Old"_s);
    QVERIFY(!doc.root.child("shortcut"));
}

void ShortcutsTest::truncated()
{
    // Cut anywhere, the file is reported, not read as fewer shortcuts.
    const QByteArray bytes = fakeTreeShortcuts();
    for (qsizetype n = 0; n < bytes.size(); ++n) {
        const BinaryVdf::Document doc = BinaryVdf::parse(bytes.left(n));
        QVERIFY2(!doc.ok, qPrintable(QString::number(n)));
        QVERIFY(doc.error.startsWith(u"byte "_s));
        QVERIFY(doc.root.children.isEmpty());
    }
    QCOMPARE(BinaryVdf::parse({}).error, u"byte 0: the file ends before its end marker"_s);
    QCOMPARE(BinaryVdf::parse(VdfBytes().map("shortcuts").map("0").str("appname", "x").bytes()).error,
             u"byte 25: the file ends inside \"0\""_s);
}

void ShortcutsTest::garbage()
{
    QCOMPARE(BinaryVdf::parse("\"shortcuts\"\n{\n}\n").error, u"byte 0: unknown field type 0x22"_s);
    QCOMPARE(BinaryVdf::parse(VdfBytes().map("x").raw(QByteArray("\x09key\0", 5)).bytes()).error,
             u"byte 3: unknown field type 0x09"_s);
    QCOMPARE(BinaryVdf::parse(fakeTreeShortcuts() + "extra").error,
             u"byte %1: more after the end marker"_s.arg(fakeTreeShortcuts().size()));
    // A string without its NUL, a number cut short.
    QVERIFY(!BinaryVdf::parse(VdfBytes().raw(QByteArray("\x01name", 5)).bytes()).ok);
    QVERIFY(!BinaryVdf::parse(VdfBytes().raw(QByteArray("\x02n\0\x01\x02", 5)).bytes()).ok);
    // Nested too deeply for any real file: an error rather than a crash.
    VdfBytes deep;
    for (int i = 0; i < 100000; ++i)
        deep.map("m");
    QCOMPARE(BinaryVdf::parse(deep.bytes()).error, u"byte 195: nested too deeply"_s);
    // Random bytes never crash.
    QByteArray noise(4096, Qt::Uninitialized);
    for (int round = 0; round < 200; ++round) {
        for (qsizetype i = 0; i < noise.size(); ++i)
            noise[i] = char((i * 7919 + round * 104729) >> (i % 5));
        BinaryVdf::parse(noise);
    }
}

void ShortcutsTest::readShortcuts()
{
    const QList<SteamShortcut> shortcuts = SteamShortcuts::read(parseFile(kFakeShortcuts).root);
    QCOMPARE(shortcuts.size(), 3);

    const SteamShortcut &cave = shortcuts.at(0);
    QCOMPARE(cave.appId, 2900000001u); // stored as a negative int32
    QCOMPARE(cave.name, u"Cave Story"_s);
    QCOMPARE(cave.exe, u"/home/user/Games/Cave Story/Doukutsu.exe"_s);
    QVERIFY(cave.exeArgs.isEmpty());
    QCOMPARE(cave.startDir, u"/home/user/Games/Cave Story/"_s);
    QCOMPARE(cave.launchOptions, u" mangohud %command%"_s);
    QCOMPARE(cave.lastPlayTime, 1788000000);
    QVERIFY(!cave.hidden);

    const SteamShortcut &mh4u = shortcuts.at(1);
    QCOMPARE(mh4u.appId, 3100000002u);
    QCOMPARE(mh4u.exe, u"/home/user/AppImages/azahar.appimage"_s);
    QCOMPARE(mh4u.startDir, u"/home/user/AppImages"_s);
    QCOMPARE(mh4u.launchOptions, u"\"/home/user/Games/3ds/Monster Hunter 4 Ultimate.3ds\""_s);
    QCOMPARE(mh4u.lastPlayTime, 0);
    QVERIFY(mh4u.hidden);

    // Arguments inside `exe` stay apart from the program, and out of the launch options.
    const SteamShortcut &smash = shortcuts.at(2);
    QCOMPARE(smash.exe, u"/home/user/AppImages/ryujinx.appimage"_s);
    QCOMPARE(smash.exeArgs, u"--fullscreen \"/home/user/Games/switch/Smash.nsp\""_s);
    QVERIFY(smash.launchOptions.isEmpty());

    // An old shortcut without an appid has the one Steam derives from exe and name; a file
    // without shortcuts has none.
    const BinaryVdf::Document old = BinaryVdf::parse(
        VdfBytes().map("shortcuts").map("0").str("AppName", "Cave Story").str("exe", "\"/home/user/Games/Cave Story/Doukutsu.exe\"").end().end().end().bytes());
    QCOMPARE(SteamShortcuts::read(old.root).value(0).appId, 3114973893u);
    QVERIFY(SteamShortcuts::read(Node()).isEmpty());
}

void ShortcutsTest::appIds()
{
    // Mortal Shell 2's, from the user's Steam: play time is under the signed form.
    QCOMPARE(SteamShortcuts::signedAppId(2320877088u), -1974090208);
    QCOMPARE(SteamShortcuts::gameId(2320877088u), 9968091191029268480ull);
    QCOMPARE(SteamShortcuts::gameId(2320877088u) >> 32, 2320877088ull);
}

void ShortcutsTest::newAppId()
{
    const QString exe = SteamShortcuts::exeField(u"/home/user/Games/Cave Story/Doukutsu.exe"_s);
    QCOMPARE(exe, u"\"/home/user/Games/Cave Story/Doukutsu.exe\""_s);
    // crc32 of exe + name, with the top bit set.
    QCOMPARE(SteamShortcuts::newAppId(exe, u"Cave Story"_s, {}), 3114973893u);
    QCOMPARE(SteamShortcuts::newAppId(u"/home/user/AppImages/azahar.appimage"_s, u"Monster Hunter 4 Ultimate"_s, {}),
             3212209690u);
    // Taken: the next free one.
    QCOMPARE(SteamShortcuts::newAppId(exe, u"Cave Story"_s, {3114973893u, 3114973894u}), 3114973895u);
    // Top bit always set, wrapping past the last ID.
    QSet<quint32> all = {0xFFFFFFFEu, 0xFFFFFFFFu};
    QVERIFY(SteamShortcuts::newAppId(exe, u"Cave Story"_s, all) & 0x80000000u);
}

void ShortcutsTest::splitExe()
{
    QString program, args;
    SteamShortcuts::splitExe(u"\"/home/user/Games/Mortal Shell II/MortalShell2.exe\""_s, &program, &args);
    QCOMPARE(program, u"/home/user/Games/Mortal Shell II/MortalShell2.exe"_s);
    QVERIFY(args.isEmpty());
    SteamShortcuts::splitExe(u"/home/user/AppImages/azahar.appimage"_s, &program, &args);
    QCOMPARE(program, u"/home/user/AppImages/azahar.appimage"_s);
    QVERIFY(args.isEmpty());
    SteamShortcuts::splitExe(u"\"/a b/ryujinx.appimage\" --fullscreen \"/g/x y.nsp\""_s, &program, &args);
    QCOMPARE(program, u"/a b/ryujinx.appimage"_s);
    QCOMPARE(args, u"--fullscreen \"/g/x y.nsp\""_s);
    SteamShortcuts::splitExe(u"/usr/bin/env FOO=1 game"_s, &program, &args);
    QCOMPARE(program, u"/usr/bin/env"_s);
    QCOMPARE(args, u"FOO=1 game"_s);
    SteamShortcuts::splitExe(u"\"/unclosed/quote.exe"_s, &program, &args);
    QCOMPARE(program, u"/unclosed/quote.exe"_s);
    QVERIFY(args.isEmpty());
    QCOMPARE(SteamShortcuts::exeField(u"/a b/x"_s, u"--y"_s), u"\"/a b/x\" --y"_s);
    QCOMPARE(SteamShortcuts::unquoted(u"\"/a b/\""_s), u"/a b/"_s);
    QCOMPARE(SteamShortcuts::unquoted(u"/a b/"_s), u"/a b/"_s);
    QCOMPARE(SteamShortcuts::unquoted(u"\""_s), u"\""_s);
}

void ShortcutsTest::addShortcut()
{
    BinaryVdf::Document doc = parseFile(kFakeShortcuts);
    const BinaryVdf::Document before = doc;
    SteamShortcut s;
    s.appId = 3114973893u;
    s.name = u"Mortal Shell 2"_s;
    s.exe = u"/home/user/Games/Mortal Shell II/MortalShell2.exe"_s;
    s.startDir = u"/home/user/Games/Mortal Shell II/"_s;
    s.icon = u"/home/user/.local/share/lattis/art/proton-mortal-shell-2/icon.png"_s;
    s.launchOptions = u"mangohud %command%"_s;
    QVERIFY(SteamShortcuts::add(&doc.root, s));

    // The others untouched, the new one last, numbered next, with Steam's fields in its order.
    for (int i = 0; i < 3; ++i)
        QCOMPARE(bytesOf(shortcutAt(doc, i)), bytesOf(shortcutAt(before, i)));
    const QByteArray expected = VdfBytes()
                                    .steamMadeShortcut("3", 3114973893u, "Mortal Shell 2",
                                                       "\"/home/user/Games/Mortal Shell II/MortalShell2.exe\"",
                                                       "/home/user/Games/Mortal Shell II/", "mangohud %command%", 0)
                                    .map("tags")
                                    .end()
                                    .end()
                                    .bytes();
    QByteArray withIcon = expected;
    withIcon.replace(QByteArray("\x01icon\0\0", 7), QByteArray("\x01icon\0", 6) + s.icon.toUtf8() + QByteArray(1, '\0'));
    QCOMPARE(bytesOf(shortcutAt(doc, 3)), withIcon + '\x08'); // bytesOf()'s holder ends too

    // Written and read back.
    const QByteArray bytes = BinaryVdf::write(doc.root);
    QCOMPARE(bytes.left(readAll(kFakeShortcuts).size() - 2), readAll(kFakeShortcuts).chopped(2));
    QCOMPARE(SteamShortcuts::read(BinaryVdf::parse(bytes).root).last(), s);

    // An app ID a shortcut already has.
    QVERIFY(!SteamShortcuts::add(&doc.root, s));
    QCOMPARE(BinaryVdf::write(doc.root), bytes);
}

void ShortcutsTest::addToEmptyFile()
{
    // No shortcuts.vdf yet: the file Steam would write for one shortcut.
    Node root;
    SteamShortcut s;
    s.appId = 0x80000001u;
    s.name = u"Game"_s;
    s.exe = u"/g/game.exe"_s;
    s.startDir = u"/g/"_s;
    QVERIFY(SteamShortcuts::add(&root, s));
    const QByteArray bytes = BinaryVdf::write(root);
    QVERIFY(bytes.startsWith(QByteArray("\0shortcuts\0\0" "0\0", 14)));
    QVERIFY(bytes.endsWith("\x08\x08\x08\x08"));
    QCOMPARE(SteamShortcuts::read(BinaryVdf::parse(bytes).root), QList<SteamShortcut>{s});
}

void ShortcutsTest::updateOneField()
{
    BinaryVdf::Document doc = parseFile(kFakeShortcuts);
    const BinaryVdf::Document before = doc;
    SteamShortcut s = SteamShortcuts::read(doc.root).at(0);
    s.launchOptions = u"gamemoderun %command%"_s;
    QVERIFY(SteamShortcuts::update(&doc.root, s));

    QCOMPARE(bytesOf(shortcutAt(doc, 1)), bytesOf(shortcutAt(before, 1)));
    QCOMPARE(bytesOf(shortcutAt(doc, 2)), bytesOf(shortcutAt(before, 2)));
    // Only that field of the shortcut changed, in place.
    const Node &cave = shortcutAt(doc, 0);
    const Node &old = shortcutAt(before, 0);
    QCOMPARE(cave.children.size(), old.children.size());
    for (qsizetype i = 0; i < cave.children.size(); ++i) {
        if (cave.children.at(i).key == "LaunchOptions")
            QCOMPARE(cave.children.at(i).toString(), u"gamemoderun %command%"_s);
        else
            QCOMPARE(cave.children.at(i), old.children.at(i));
    }
    QCOMPARE(SteamShortcuts::read(doc.root).at(0), s);

    // Renamed and hidden too.
    s.name = u"Cave Story+"_s;
    s.hidden = true;
    QVERIFY(SteamShortcuts::update(&doc.root, s));
    QCOMPARE(SteamShortcuts::read(doc.root).at(0), s);

    // No such shortcut.
    s.appId = 1234;
    QVERIFY(!SteamShortcuts::update(&doc.root, s));
}

void ShortcutsTest::updateKeepsQuoting()
{
    BinaryVdf::Document doc = parseFile(kFakeShortcuts);
    QList<SteamShortcut> shortcuts = SteamShortcuts::read(doc.root);

    // Steam's quoted exe stays quoted; Steam ROM Manager's unquoted one stays unquoted.
    shortcuts[0].exe = u"/home/user/Games/CaveStory/Doukutsu.exe"_s;
    shortcuts[1].exe = u"/home/user/AppImages/azahar-2.appimage"_s;
    // The arguments inside exe are kept when the program changes.
    shortcuts[2].exe = u"/home/user/AppImages/ryubing.appimage"_s;
    for (const SteamShortcut &s : std::as_const(shortcuts))
        QVERIFY(SteamShortcuts::update(&doc.root, s));
    QCOMPARE(shortcutAt(doc, 0).child("exe")->toString(), u"\"/home/user/Games/CaveStory/Doukutsu.exe\""_s);
    QCOMPARE(shortcutAt(doc, 1).child("exe")->toString(), u"/home/user/AppImages/azahar-2.appimage"_s);
    QCOMPARE(shortcutAt(doc, 2).child("exe")->toString(),
             u"\"/home/user/AppImages/ryubing.appimage\" --fullscreen \"/home/user/Games/switch/Smash.nsp\""_s);
    // An unquoted one that gains a space needs the quotes.
    shortcuts[1].exe = u"/home/user/App Images/azahar.appimage"_s;
    QVERIFY(SteamShortcuts::update(&doc.root, shortcuts[1]));
    QCOMPARE(shortcutAt(doc, 1).child("exe")->toString(), u"\"/home/user/App Images/azahar.appimage\""_s);
    QCOMPARE(SteamShortcuts::read(doc.root), shortcuts);
}

void ShortcutsTest::updateUnchanged()
{
    // The same values, or the same paths written another way, change no byte.
    BinaryVdf::Document doc = parseFile(kFakeShortcuts);
    const QByteArray bytes = readAll(kFakeShortcuts);
    for (SteamShortcut s : SteamShortcuts::read(doc.root)) {
        QVERIFY(SteamShortcuts::update(&doc.root, s));
        if (s.startDir.endsWith(u'/'))
            s.startDir.chop(1);
        else
            s.startDir += u'/';
        QVERIFY(SteamShortcuts::update(&doc.root, s));
    }
    QCOMPARE(BinaryVdf::write(doc.root), bytes);
}

void ShortcutsTest::updateInsertsMissingField()
{
    // A shortcut without LaunchOptions or IsHidden: they go where Steam puts them.
    BinaryVdf::Document doc = BinaryVdf::parse(VdfBytes()
                                                   .map("shortcuts")
                                                   .map("0")
                                                   .i32("appid", -2)
                                                   .str("appname", "Old")
                                                   .str("exe", "/old")
                                                   .str("icon", "")
                                                   .i32("LastPlayTime", 0)
                                                   .str("Unknown", "kept")
                                                   .end()
                                                   .end()
                                                   .end()
                                                   .bytes());
    QVERIFY(doc.ok);
    SteamShortcut s = SteamShortcuts::read(doc.root).at(0);
    s.launchOptions = u"-x"_s;
    s.hidden = true;
    s.startDir = u"/"_s;
    QVERIFY(SteamShortcuts::update(&doc.root, s));
    QList<QByteArray> keys;
    for (const Node &n : shortcutAt(doc, 0).children)
        keys.append(n.key);
    QCOMPARE(keys, (QList<QByteArray>{"appid", "appname", "exe", "StartDir", "icon", "LaunchOptions", "IsHidden",
                                      "LastPlayTime", "Unknown"}));
    QCOMPARE(SteamShortcuts::read(doc.root).at(0), s);
}

void ShortcutsTest::removeRenumbers()
{
    BinaryVdf::Document doc = parseFile(kFakeShortcuts);
    const BinaryVdf::Document before = doc;
    QVERIFY(SteamShortcuts::remove(&doc.root, 3100000002u));
    const Node &list = *doc.root.child("shortcuts");
    QCOMPARE(list.children.size(), 2);
    QCOMPARE(list.children.at(0), shortcutAt(before, 0));
    // The one after it is renumbered, its fields untouched.
    QCOMPARE(list.children.at(1).key, QByteArray("1"));
    QCOMPARE(list.children.at(1).children, shortcutAt(before, 2).children);
    QVERIFY(!SteamShortcuts::remove(&doc.root, 3100000002u));

    // Added again: numbered after the last.
    SteamShortcut s = SteamShortcuts::read(before.root).at(1);
    QVERIFY(SteamShortcuts::add(&doc.root, s));
    QCOMPARE(doc.root.child("shortcuts")->children.last().key, QByteArray("2"));
    QVERIFY(SteamShortcuts::remove(&doc.root, 2900000001u));
    QVERIFY(SteamShortcuts::remove(&doc.root, 3300000003u));
    QVERIFY(SteamShortcuts::remove(&doc.root, 3100000002u));
    QCOMPARE(BinaryVdf::write(doc.root), QByteArray("\0shortcuts\0\x08\x08", 13));
}

QTEST_MAIN(ShortcutsTest)
#include "shortcuts_test.moc"
