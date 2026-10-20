#include "vdf.h"

#include <QFile>
#include <QTest>

using namespace Qt::StringLiterals;

class VdfTest : public QObject
{
    Q_OBJECT

private slots:
    void parseLibraryFolders();
    void parseManifest();
    void parseLoginUsers();
    void parseLocalConfig();
    void parseConfigKeepsBackslashes();
    void keysIgnoreCase();
    void commentsIncludesAndConditions();
    void bareWords();
    void byteRanges();
    void malformed_data();
    void malformed();
    void setExistingValue();
    void setSameValueIsUnchanged();
    void setEscapedValue();
    void addKeyToExistingBlock();
    void addKeyToEmptyBlock();
    void addMissingBlocks();
    void addKeyAtTopLevel();
    void addKeyToEmptyFile();
    void keepsCrLf();
    void setFailsOnBlockValueMismatch();
    void removeValue();
    void removeBlock();
    void removeMissingIsNoop();
    void removeKeepsTrailingLines();
    void editMalformedFails();
};

namespace {

QByteArray fixture(const QString &name)
{
    QFile f(QStringLiteral(VDF_DATA_DIR "/") + name);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qPrintable(name));
    return f.readAll();
}

const QStringList kAppPath = {u"Software"_s, u"Valve"_s, u"Steam"_s, u"apps"_s};

QStringList appPath(const QString &appId, const QString &key = {})
{
    QStringList path = kAppPath;
    path << appId;
    if (!key.isEmpty())
        path << key;
    return path;
}

// Checks that `edited` is `original` with `oldPart` replaced by `newPart` once, and every
// other byte the same.
void compareEdit(const QByteArray &original, const QByteArray &edited, const QByteArray &oldPart,
                 const QByteArray &newPart)
{
    const qsizetype at = original.indexOf(oldPart);
    QVERIFY2(at >= 0, oldPart.constData());
    QByteArray expected = original;
    expected.replace(at, oldPart.size(), newPart);
    QCOMPARE(edited, expected);
}

} // namespace

void VdfTest::parseLibraryFolders()
{
    const Vdf::Document doc = Vdf::parse(fixture(u"libraryfolders.vdf"_s));
    QVERIFY2(doc.ok, qPrintable(doc.error));
    const Vdf::Node *folders = doc.root.child(u"libraryfolders");
    QVERIFY(folders && folders->isBlock);
    QCOMPARE(folders->children.size(), 2);
    QCOMPARE(folders->children[0].valueOf(u"path"), u"/home/player/.local/share/Steam"_s);
    QCOMPARE(folders->children[1].valueOf(u"path"), u"/mnt/games/SteamLibrary"_s);
    const Vdf::Node *apps = folders->find({u"0"_s, u"apps"_s});
    QVERIFY(apps);
    QCOMPARE(apps->children.size(), 4);
    QCOMPARE(apps->valueOf(u"2231450"), u"404028818"_s);
    QVERIFY(folders->find({u"1"_s, u"apps"_s})->children.isEmpty());
    QCOMPARE(folders->children[1].valueOf(u"label"), u"Games"_s);
    QCOMPARE(folders->children[0].valueOf(u"label", u"fallback"_s), QString());
    QCOMPARE(folders->children[0].valueOf(u"missing", u"fallback"_s), u"fallback"_s);
}

void VdfTest::parseManifest()
{
    const Vdf::Document doc = Vdf::parse(fixture(u"appmanifest_2231450.acf"_s));
    QVERIFY2(doc.ok, qPrintable(doc.error));
    const Vdf::Node *app = doc.root.child(u"AppState");
    QVERIFY(app);
    QCOMPARE(app->valueOf(u"appid"), u"2231450"_s);
    QCOMPARE(app->valueOf(u"name"), u"Pizza Tower"_s);
    QCOMPARE(app->valueOf(u"installdir"), u"Pizza Tower"_s);
    QCOMPARE(app->valueOf(u"StateFlags"), u"4"_s);
    QCOMPARE(app->valueOf(u"SizeOnDisk"), u"404028818"_s);
    QCOMPARE(app->find({u"InstalledDepots"_s, u"2231451"_s})->valueOf(u"size"), u"404028818"_s);
    QCOMPARE(app->line, 1);
    QCOMPARE(app->child(u"name")->line, 5);
}

void VdfTest::parseLoginUsers()
{
    const Vdf::Document doc = Vdf::parse(fixture(u"loginusers.vdf"_s));
    QVERIFY2(doc.ok, qPrintable(doc.error));
    const Vdf::Node *users = doc.root.child(u"users");
    QCOMPARE(users->children.size(), 2);
    QCOMPARE(users->children[0].key, u"76561198000000001"_s);
    QCOMPARE(users->children[0].valueOf(u"MostRecent"), u"1"_s);
    QCOMPARE(users->children[1].valueOf(u"PersonaName"), u"Player \"Two\""_s);
}

void VdfTest::parseLocalConfig()
{
    const Vdf::Document doc = Vdf::parse(fixture(u"localconfig.vdf"_s));
    QVERIFY2(doc.ok, qPrintable(doc.error));
    const Vdf::Node *store = doc.root.child(u"UserLocalConfigStore");
    const Vdf::Node *pizza = store->find(appPath(u"2231450"_s));
    QVERIFY(pizza);
    QCOMPARE(pizza->valueOf(u"Playtime"), u"4085"_s);
    QCOMPARE(pizza->valueOf(u"LastPlayed"), u"1788735088"_s);
    QCOMPARE(pizza->valueOf(u"LaunchOptions"), QString());
    const Vdf::Node *tomb = store->find(appPath(u"8000"_s));
    QCOMPARE(tomb->valueOf(u"LaunchOptions"), u"PROTON_LOG=1 %command% -window \"C:\\Games\\save dir\""_s);
    QCOMPARE(store->find({u"WebStorage"_s})->children[0].value,
             uR"({"inputGain":1,"outputGain":1,"selectedMic":"default"})"_s);
}

void VdfTest::parseConfigKeepsBackslashes()
{
    const Vdf::Document doc = Vdf::parse(fixture(u"config.vdf"_s));
    QVERIFY2(doc.ok, qPrintable(doc.error));
    const Vdf::Node *foz = doc.root.find({u"InstallConfigStore"_s, u"Software"_s, u"Valve"_s,
                                         u"Steam"_s, u"ShaderCacheManager"_s, u"FozFiles"_s});
    QVERIFY(foz);
    QVERIFY(foz->valueOf(u"8000").contains(u"fozpipelinesv6\\steamapprun"_s));
}

void VdfTest::keysIgnoreCase()
{
    const Vdf::Document doc = Vdf::parse(fixture(u"localconfig.vdf"_s));
    const Vdf::Node *store = doc.root.child(u"userlocalconfigstore");
    QVERIFY(store);
    QVERIFY(store->find({u"SOFTWARE"_s, u"valve"_s, u"steam"_s, u"Apps"_s, u"8000"_s}));
    QCOMPARE(store->find(appPath(u"8000"_s))->valueOf(u"playtime"), u"70"_s);
}

void VdfTest::commentsIncludesAndConditions()
{
    const QByteArray text = "#base \"base.vdf\"\n"
                            "#include \"other.vdf\"\n"
                            "// a comment \"with\" { braces }\n"
                            "\"root\" // after the key\n"
                            "{\n"
                            "\t\"a\"\t\t\"1\" [$WIN32]\n"
                            "\t\"url\"\t\t\"http://example.com\" // trailing\n"
                            "\t\"b\"\t\t\"2\"\n"
                            "}\n";
    const Vdf::Document doc = Vdf::parse(text);
    QVERIFY2(doc.ok, qPrintable(doc.error));
    QCOMPARE(doc.root.children.size(), 1);
    const Vdf::Node *root = doc.root.child(u"root");
    QCOMPARE(root->children.size(), 3);
    QCOMPARE(root->valueOf(u"a"), u"1"_s);
    QCOMPARE(root->valueOf(u"url"), u"http://example.com"_s);
    QCOMPARE(root->valueOf(u"b"), u"2"_s);

    // Editing leaves the #include lines and comments where they are.
    const Vdf::EditResult edited = Vdf::setValue(text, {u"root"_s, u"b"_s}, u"3"_s);
    QVERIFY2(edited.ok, qPrintable(edited.error));
    compareEdit(text, edited.text, "\"2\"", "\"3\"");
}

void VdfTest::bareWords()
{
    const Vdf::Document doc = Vdf::parse("root { key value other \"quoted\" }");
    QVERIFY2(doc.ok, qPrintable(doc.error));
    const Vdf::Node *root = doc.root.child(u"root");
    QCOMPARE(root->valueOf(u"key"), u"value"_s);
    QCOMPARE(root->valueOf(u"other"), u"quoted"_s);
}

void VdfTest::byteRanges()
{
    const QByteArray text = fixture(u"appmanifest_2231450.acf"_s);
    const Vdf::Document doc = Vdf::parse(text);
    const Vdf::Node *name = doc.root.find({u"AppState"_s, u"name"_s});
    QCOMPARE(text.mid(name->begin, name->end - name->begin), "\"name\"\t\t\"Pizza Tower\"");
    QCOMPARE(text.mid(name->valueBegin, name->end - name->valueBegin), "\"Pizza Tower\"");
    const Vdf::Node *config = doc.root.find({u"AppState"_s, u"UserConfig"_s});
    QCOMPARE(text.at(config->valueBegin), '{');
    QCOMPARE(text.at(config->end - 1), '}');
}

void VdfTest::malformed_data()
{
    QTest::addColumn<QByteArray>("text");
    QTest::addColumn<QString>("error");
    QTest::newRow("unclosed string") << QByteArray("\"a\"\n{\n\"b\" \"c\n}\n")
                                     << u"line 3: a string isn't closed"_s;
    QTest::newRow("unclosed block") << QByteArray("\"a\"\n{\n\t\"b\" \"c\"\n")
                                    << u"line 4: the block opened on line 2 isn't closed"_s;
    QTest::newRow("stray brace") << QByteArray("\"a\" \"b\"\n}\n") << u"line 2: a '}' without its '{'"_s;
    QTest::newRow("brace without key") << QByteArray("{\n}\n") << u"line 1: a '{' without a key"_s;
    QTest::newRow("key without value") << QByteArray("\"a\"\n{\n\t\"b\"\n}\n")
                                       << u"line 3: \"b\" has no value"_s;
    QTest::newRow("key at the end") << QByteArray("\"a\"") << u"line 1: \"a\" has no value"_s;
    QTest::newRow("too deep") << QByteArray("\"a\" {").repeated(200) << QString();
}

void VdfTest::malformed()
{
    QFETCH(QByteArray, text);
    QFETCH(QString, error);
    const Vdf::Document doc = Vdf::parse(text);
    QVERIFY(!doc.ok);
    QVERIFY(doc.root.children.isEmpty());
    if (!error.isEmpty())
        QCOMPARE(doc.error, error);
    else
        QVERIFY(!doc.error.isEmpty());
}

void VdfTest::setExistingValue()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const Vdf::EditResult result = Vdf::setValue(text, QStringList{u"UserLocalConfigStore"_s} + appPath(u"8000"_s, u"LaunchOptions"_s),
                                                 u"mangohud %command%"_s);
    QVERIFY2(result.ok, qPrintable(result.error));
    compareEdit(text, result.text, "\"PROTON_LOG=1 %command% -window \\\"C:\\\\Games\\\\save dir\\\"\"",
                "\"mangohud %command%\"");
}

void VdfTest::setSameValueIsUnchanged()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const QStringList path = QStringList{u"UserLocalConfigStore"_s} + appPath(u"8000"_s, u"Playtime"_s);
    const Vdf::EditResult result = Vdf::setValue(text, path, u"70"_s);
    QVERIFY(result.ok);
    QCOMPARE(result.text, text);
    // Case-insensitive match on the key; the key's spelling stays.
    const QStringList lower = QStringList{u"userlocalconfigstore"_s} + appPath(u"8000"_s, u"playtime"_s);
    const Vdf::EditResult changed = Vdf::setValue(text, lower, u"71"_s);
    compareEdit(text, changed.text, "\"Playtime\"\t\t\"70\"", "\"Playtime\"\t\t\"71\"");
}

void VdfTest::setEscapedValue()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const QString value = u"DXVK_HUD=1 %command% \"C:\\path with space\"\ttab"_s;
    const QStringList path = QStringList{u"UserLocalConfigStore"_s} + appPath(u"8000"_s, u"LaunchOptions"_s);
    const Vdf::EditResult result = Vdf::setValue(text, path, value);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(result.text.contains("\"DXVK_HUD=1 %command% \\\"C:\\\\path with space\\\"\\ttab\""));
    QCOMPARE(Vdf::parse(result.text).root.find(path)->value, value);
}

void VdfTest::addKeyToExistingBlock()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const Vdf::EditResult result = Vdf::setValue(
        text, QStringList{u"UserLocalConfigStore"_s} + appPath(u"2231450"_s, u"LaunchOptions"_s),
        u"gamemoderun %command%"_s);
    QVERIFY2(result.ok, qPrintable(result.error));
    compareEdit(text, result.text, "\"BadgeData\"\t\t\"0a000000082d1206080310011800\"\n",
                "\"BadgeData\"\t\t\"0a000000082d1206080310011800\"\n"
                "\t\t\t\t\t\t\"LaunchOptions\"\t\t\"gamemoderun %command%\"\n");
}

void VdfTest::addKeyToEmptyBlock()
{
    const QByteArray text = fixture(u"libraryfolders.vdf"_s);
    const Vdf::EditResult result = Vdf::setValue(
        text, {u"libraryfolders"_s, u"1"_s, u"apps"_s, u"8000"_s}, u"42"_s);
    QVERIFY2(result.ok, qPrintable(result.error));
    compareEdit(text, result.text, "\t\t\"apps\"\n\t\t{\n\t\t}\n\t}\n}",
                "\t\t\"apps\"\n\t\t{\n\t\t\t\"8000\"\t\t\"42\"\n\t\t}\n\t}\n}");
}

void VdfTest::addMissingBlocks()
{
    // An app Steam has no block for yet: the block is added after the last app, indented
    // like its neighbours.
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const Vdf::EditResult result = Vdf::setValue(
        text, QStringList{u"UserLocalConfigStore"_s} + appPath(u"1145360"_s, u"LaunchOptions"_s),
        u"%command% -novid"_s);
    QVERIFY2(result.ok, qPrintable(result.error));
    compareEdit(text, result.text, "\t\t\t\t\t\t\"8000_eula_0\"\t\t\"4\"\n\t\t\t\t\t}\n",
                "\t\t\t\t\t\t\"8000_eula_0\"\t\t\"4\"\n\t\t\t\t\t}\n"
                "\t\t\t\t\t\"1145360\"\n\t\t\t\t\t{\n"
                "\t\t\t\t\t\t\"LaunchOptions\"\t\t\"%command% -novid\"\n"
                "\t\t\t\t\t}\n");

    // CompatToolMapping doesn't exist in config.vdf at all.
    const QByteArray config = fixture(u"config.vdf"_s);
    const QStringList steam = {u"InstallConfigStore"_s, u"Software"_s, u"Valve"_s, u"Steam"_s};
    const Vdf::EditResult mapped = Vdf::setValue(
        config, steam + QStringList{u"CompatToolMapping"_s, u"8000"_s, u"name"_s}, u"proton_experimental"_s);
    QVERIFY2(mapped.ok, qPrintable(mapped.error));
    compareEdit(config, mapped.text, "\t\t\t\t\"RecentDownloadRate\"\t\t\"1499413\"\n",
                "\t\t\t\t\"RecentDownloadRate\"\t\t\"1499413\"\n"
                "\t\t\t\t\"CompatToolMapping\"\n\t\t\t\t{\n"
                "\t\t\t\t\t\"8000\"\n\t\t\t\t\t{\n"
                "\t\t\t\t\t\t\"name\"\t\t\"proton_experimental\"\n"
                "\t\t\t\t\t}\n"
                "\t\t\t\t}\n");
}

void VdfTest::addKeyAtTopLevel()
{
    const QByteArray text = "\"a\"\t\t\"1\"\n";
    const Vdf::EditResult result = Vdf::setValue(text, {u"b"_s}, u"2"_s);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, "\"a\"\t\t\"1\"\n\"b\"\t\t\"2\"\n");
}

void VdfTest::addKeyToEmptyFile()
{
    const Vdf::EditResult empty = Vdf::setValue({}, {u"a"_s, u"b"_s}, u"1"_s);
    QVERIFY2(empty.ok, qPrintable(empty.error));
    QCOMPARE(empty.text, "\"a\"\n{\n\t\"b\"\t\t\"1\"\n}");

    const Vdf::EditResult comment = Vdf::setValue("// only a comment\n", {u"a"_s}, u"1"_s);
    QVERIFY2(comment.ok, qPrintable(comment.error));
    QCOMPARE(comment.text, "// only a comment\n\"a\"\t\t\"1\"\n");
}

void VdfTest::keepsCrLf()
{
    const QByteArray text = "\"a\"\r\n{\r\n\t\"b\"\t\t\"1\"\r\n}\r\n";
    const Vdf::EditResult result = Vdf::setValue(text, {u"a"_s, u"c"_s}, u"2"_s);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, "\"a\"\r\n{\r\n\t\"b\"\t\t\"1\"\r\n\t\"c\"\t\t\"2\"\r\n}\r\n");
    const Vdf::EditResult removed = Vdf::remove(result.text, {u"a"_s, u"b"_s});
    QCOMPARE(removed.text, "\"a\"\r\n{\r\n\t\"c\"\t\t\"2\"\r\n}\r\n");
}

void VdfTest::setFailsOnBlockValueMismatch()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const QStringList app = QStringList{u"UserLocalConfigStore"_s} + appPath(u"8000"_s);
    QVERIFY(!Vdf::setValue(text, app, u"x"_s).ok);
    QVERIFY(!Vdf::setValue(text, app + QStringList{u"Playtime"_s, u"x"_s}, u"x"_s).ok);
    QVERIFY(!Vdf::setValue(text, {}, u"x"_s).ok);
}

void VdfTest::removeValue()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const Vdf::EditResult result = Vdf::remove(
        text, QStringList{u"UserLocalConfigStore"_s} + appPath(u"8000"_s, u"LaunchOptions"_s));
    QVERIFY2(result.ok, qPrintable(result.error));
    compareEdit(text, result.text,
                "\t\t\t\t\t\t\"LaunchOptions\"\t\t\"PROTON_LOG=1 %command% -window \\\"C:\\\\Games\\\\save dir\\\"\"\n",
                "");
}

void VdfTest::removeBlock()
{
    const QByteArray text = fixture(u"appmanifest_2231450.acf"_s);
    const Vdf::EditResult result = Vdf::remove(text, {u"AppState"_s, u"UserConfig"_s});
    QVERIFY2(result.ok, qPrintable(result.error));
    compareEdit(text, result.text, "\t\"UserConfig\"\n\t{\n\t\t\"language\"\t\t\"english\"\n\t}\n", "");
}

void VdfTest::removeMissingIsNoop()
{
    const QByteArray text = fixture(u"localconfig.vdf"_s);
    const Vdf::EditResult result = Vdf::remove(
        text, QStringList{u"UserLocalConfigStore"_s} + appPath(u"2231450"_s, u"LaunchOptions"_s));
    QVERIFY(result.ok);
    QCOMPARE(result.text, text);
}

void VdfTest::removeKeepsTrailingLines()
{
    const QByteArray text = "\"a\"\n{\n\t\"b\"\t\t\"1\" // note\n\t\"c\" \"2\" [$LINUX]\n\t\"d\" \"3\" \"e\" \"4\"\n}\n";
    const Vdf::EditResult b = Vdf::remove(text, {u"a"_s, u"b"_s});
    QCOMPARE(b.text, "\"a\"\n{\n\t\"c\" \"2\" [$LINUX]\n\t\"d\" \"3\" \"e\" \"4\"\n}\n");
    const Vdf::EditResult c = Vdf::remove(text, {u"a"_s, u"c"_s});
    QCOMPARE(c.text, "\"a\"\n{\n\t\"b\"\t\t\"1\" // note\n\t\"d\" \"3\" \"e\" \"4\"\n}\n");
    // Not alone on its line: only the pair goes.
    const Vdf::EditResult e = Vdf::remove(text, {u"a"_s, u"e"_s});
    QCOMPARE(e.text, "\"a\"\n{\n\t\"b\"\t\t\"1\" // note\n\t\"c\" \"2\" [$LINUX]\n\t\"d\" \"3\"\n}\n");
}

void VdfTest::editMalformedFails()
{
    const QByteArray text = "\"a\"\n{\n\t\"b\" \"1\"\n";
    const Vdf::EditResult set = Vdf::setValue(text, {u"a"_s, u"b"_s}, u"2"_s);
    QVERIFY(!set.ok);
    QVERIFY(set.error.startsWith(u"line "_s));
    QVERIFY(!Vdf::remove(text, {u"a"_s, u"b"_s}).ok);
}

QTEST_MAIN(VdfTest)
#include "vdf_test.moc"
