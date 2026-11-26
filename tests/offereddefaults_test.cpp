#include "offereddefaults.h"
#include "tomlpatch.h"

#include <QFile>
#include <QTest>

#include <toml++/toml.hpp>

namespace {

const QString kDefaults = QStringLiteral(
    "# header\n\n"
    "offered = [\"a\", \"a/x\", \"b\", \"b/y\", \"c\", \"c/z\", \"c/w\"]\n\n"
    "[[platform]]\nid = \"a\"\nname = \"A\"\nicon = \"a\"\nemulators = [\"x\"]\n\n"
    "[[platform]]\nid = \"b\"\nname = \"B\"\nicon = \"b\"\nemulators = [\"y\"]\n\n"
    "[[platform]]\nid = \"c\"\nname = \"Platform C\"\nicon = \"cc\"\nemulators = [\"z\", \"w\"]\n");

const QStringList kBootstrap = {"a", "a/x", "b", "b/y"};

QStringList ids(const QString &text)
{
    return TomlPatch::blockValues(text, QStringLiteral("platform"), QStringLiteral("id"));
}

QStringList offeredOf(const QString &text)
{
    QStringList out;
    const toml::table root = toml::parse(text.toStdString());
    for (const toml::node &item : *root["offered"].as_array())
        out.append(QString::fromStdString(item.as_string()->get()));
    return out;
}

QStringList emulatorsOf(const QString &text, const QString &id)
{
    QStringList out;
    const toml::table root = toml::parse(text.toStdString());
    for (const toml::node &entry : *root["platform"].as_array()) {
        const toml::table &t = *entry.as_table();
        if (QString::fromStdString(t["id"].as_string()->get()) != id)
            continue;
        for (const toml::node &item : *t["emulators"].as_array())
            out.append(QString::fromStdString(item.as_string()->get()));
    }
    return out;
}

QString file(const QString &platforms, const QString &offered = {})
{
    return QStringLiteral("# mine\n\n") + offered + platforms;
}

QString block(const QString &id, const QString &emulators)
{
    return QStringLiteral("[[platform]]\nid = \"%1\"\nname = \"%1\"\nemulators = %2\n\n").arg(id, emulators);
}

QString resourceDefaults()
{
    QFile f(QStringLiteral(":/qt/qml/Launcher/resources/platforms.toml"));
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace

class OfferedDefaultsTest : public QObject
{
    Q_OBJECT

private slots:
    void freshFileIsComplete();
    void oldFileGetsSwitch2LichiiAndEden();
    void oldFileGetsOnlyLaterDefaults();
    void deletedBootstrapPlatformStaysDeleted();
    void deletedNewPlatformStaysDeleted();
    void placedAfterNearestEarlierDefault();
    void placedFirstWithoutEarlierDefault();
    void usersOrderKept();
    void oldFileGetsTheRetroPlatformsOnce();
    void retroPlatformsKeepAUsersOrder();
    void emulatorAppendedOnce();
    void emulatorPlacedWhereDefaultsHaveIt();
    void missingPlatformSkipsItsEmulators();
    void nothingChangesWhenAllOffered();
    void offeredWrittenForOldFile();
    void badOfferedIsAnError();
    void unparsableFileIsAnError();
    void unparsableDefaultsAreAnError();
    void offeredWithANumberIsAnError();
    void numbersAndBooleansAreCopiedAsWritten();
    void bootstrapMatchesTheDefaultsToday();
};

void OfferedDefaultsTest::freshFileIsComplete()
{
    const QString defaults = resourceDefaults();
    QVERIFY(!defaults.isEmpty());
    QStringList expected;
    const toml::table root = toml::parse(defaults.toStdString());
    for (const toml::node &entry : *root["platform"].as_array()) {
        const toml::table &t = *entry.as_table();
        const QString id = QString::fromStdString(t["id"].as_string()->get());
        expected.append(id);
        for (const toml::node &e : *t["emulators"].as_array())
            expected.append(id + u'/' + QString::fromStdString(e.as_string()->get()));
    }
    QCOMPARE(offeredOf(defaults), expected);

    const TomlPatch::Result r = OfferedDefaults::apply(defaults, defaults, OfferedDefaults::bootstrap());
    QVERIFY(r.ok);
    QCOMPARE(r.text, defaults);
}

void OfferedDefaultsTest::oldFileGetsSwitch2LichiiAndEden()
{
    const QString old = file(block("switch", "[\"ryubing\"]") + block("3ds", "[\"azahar\"]"));
    QStringList added;
    const TomlPatch::Result r = OfferedDefaults::apply(old, resourceDefaults(), OfferedDefaults::bootstrap(), &added);
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text).mid(0, 3), QStringList({"switch", "switch-2", "3ds"}));
    QCOMPARE(emulatorsOf(r.text, "switch"), QStringList({"eden", "ryubing", "lichii"}));
    QCOMPARE(emulatorsOf(r.text, "switch-2"), QStringList({"lichii"}));
    QVERIFY(r.text.contains("icon = \"switch2\""));

    const TomlPatch::Result again = OfferedDefaults::apply(r.text, resourceDefaults(), OfferedDefaults::bootstrap());
    QVERIFY(again.ok);
    QCOMPARE(again.text, r.text);
}

void OfferedDefaultsTest::oldFileGetsOnlyLaterDefaults()
{
    const QString old = file(block("a", "[\"x\"]") + block("b", "[\"y\"]"));
    QStringList added;
    const TomlPatch::Result r = OfferedDefaults::apply(old, kDefaults, kBootstrap, &added);
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"a", "b", "c"}));
    QCOMPARE(added, QStringList({"Platform C"}));
    QCOMPARE(emulatorsOf(r.text, "c"), QStringList({"z", "w"}));
    QVERIFY(r.text.contains("name = \"Platform C\""));
    QVERIFY(r.text.contains("icon = \"cc\""));
    QCOMPARE(offeredOf(r.text), QStringList({"a", "a/x", "b", "b/y", "c", "c/z", "c/w"}));
    QVERIFY(r.text.startsWith("# mine\n"));
}

void OfferedDefaultsTest::deletedBootstrapPlatformStaysDeleted()
{
    const QString old = file(block("a", "[\"x\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, kDefaults, kBootstrap);
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"a", "c"}));
}

void OfferedDefaultsTest::deletedNewPlatformStaysDeleted()
{
    const QString old = file(block("a", "[\"x\"]") + block("b", "[\"y\"]"));
    const TomlPatch::Result first = OfferedDefaults::apply(old, kDefaults, kBootstrap);
    QVERIFY(first.ok);
    const TomlPatch::Result removed
        = TomlPatch::removeBlock(first.text, {QStringLiteral("platform"), QStringLiteral("c")});
    QCOMPARE(ids(removed.text), QStringList({"a", "b"}));
    const TomlPatch::Result second = OfferedDefaults::apply(removed.text, kDefaults, kBootstrap);
    QVERIFY(second.ok);
    QCOMPARE(second.text, removed.text);
}

void OfferedDefaultsTest::placedAfterNearestEarlierDefault()
{
    const QString defaults = QStringLiteral(
        "[[platform]]\nid = \"a\"\nname = \"A\"\n\n[[platform]]\nid = \"m\"\nname = \"M\"\n\n"
        "[[platform]]\nid = \"b\"\nname = \"B\"\n");
    const QString old = file(block("a", "[\"x\"]") + block("own", "[]") + block("b", "[\"y\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, defaults, {"a", "b"});
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"a", "m", "own", "b"}));
}

void OfferedDefaultsTest::placedFirstWithoutEarlierDefault()
{
    const QString defaults
        = QStringLiteral("[[platform]]\nid = \"n\"\nname = \"N\"\n\n[[platform]]\nid = \"a\"\nname = \"A\"\n");
    const QString old = file(block("own", "[]") + block("a", "[]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, defaults, {"a"});
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"n", "own", "a"}));
}

void OfferedDefaultsTest::usersOrderKept()
{
    const QString old = file(block("b", "[\"y\"]") + block("a", "[\"x\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, kDefaults, kBootstrap);
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"b", "c", "a"}));
}

void OfferedDefaultsTest::emulatorPlacedWhereDefaultsHaveIt()
{
    const QString defaults
        = QStringLiteral("[[platform]]\nid = \"a\"\nname = \"A\"\nemulators = [\"n\", \"x\", \"m\", \"y\"]\n");
    const QString old = file(block("a", "[\"mine\", \"y\", \"x\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, defaults, {"a", "a/x", "a/y"});
    QVERIFY(r.ok);
    QCOMPARE(emulatorsOf(r.text, "a"), QStringList({"mine", "n", "m", "y", "x"}));
}

void OfferedDefaultsTest::oldFileGetsTheRetroPlatformsOnce()
{
    const QStringList retro = {"snes", "nes", "gbc", "gb", "dreamcast", "genesis", "sms", "gamegear", "arcade"};
    const QStringList emulators = {"snes9x", "punes", "mgba", "mgba", "flycast", "blastem", "ares", "ares", "mame"};
    QString text = file(block("switch", "[\"eden\"]") + block("n64", "[\"rmg\"]") + block("nds", "[\"melonds\"]")
                        + block("gba", "[\"mgba\"]") + block("ps4", "[\"shadps4\"]") + block("psp", "[\"ppsspp\"]"));
    QStringList added;
    const TomlPatch::Result r = OfferedDefaults::apply(text, resourceDefaults(), OfferedDefaults::bootstrap(), &added);
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"switch", "switch-2", "n64", "snes", "nes", "nds", "gba", "gbc", "gb", "dreamcast",
                                       "genesis", "sms", "gamegear", "ps4", "psp", "arcade"}));
    for (int i = 0; i < retro.size(); ++i) {
        QCOMPARE(emulatorsOf(r.text, retro.at(i)), QStringList({emulators.at(i)}));
        QVERIFY(offeredOf(r.text).contains(retro.at(i)));
        QVERIFY(offeredOf(r.text).contains(retro.at(i) + u'/' + emulators.at(i)));
    }
    QCOMPARE(added.size(), 11); // Lichii on Switch, Switch 2 and the nine

    const TomlPatch::Result again = OfferedDefaults::apply(r.text, resourceDefaults(), OfferedDefaults::bootstrap());
    QVERIFY(again.ok);
    QCOMPARE(again.text, r.text);

    const TomlPatch::Result removed = TomlPatch::removeBlock(r.text, {QStringLiteral("platform"), QStringLiteral("gb")});
    const TomlPatch::Result second = OfferedDefaults::apply(removed.text, resourceDefaults(), OfferedDefaults::bootstrap());
    QVERIFY(second.ok);
    QVERIFY(!ids(second.text).contains("gb"));
    QVERIFY(ids(second.text).contains("gbc"));
}

void OfferedDefaultsTest::retroPlatformsKeepAUsersOrder()
{
    const QString old = file(block("psp", "[]") + block("gba", "[\"mgba\"]") + block("n64", "[\"rmg\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, resourceDefaults(), OfferedDefaults::bootstrap());
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"switch-2", "psp", "arcade", "gba", "gbc", "gb", "dreamcast", "genesis", "sms", "gamegear",
                                       "n64", "snes", "nes"}));
}

void OfferedDefaultsTest::emulatorAppendedOnce()
{
    const QString defaults
        = QStringLiteral("[[platform]]\nid = \"a\"\nname = \"A\"\nemulators = [\"x\", \"n\"]\n");
    const QString old = file(block("a", "[\"mine\", \"x\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, defaults, {"a", "a/x"});
    QVERIFY(r.ok);
    QCOMPARE(emulatorsOf(r.text, "a"), QStringList({"mine", "x", "n"}));
    QCOMPARE(offeredOf(r.text), QStringList({"a", "a/x", "a/n"}));

    const TomlPatch::Result again = OfferedDefaults::apply(r.text, defaults, {"a", "a/x"});
    QVERIFY(again.ok);
    QCOMPARE(again.text, r.text);

    const QString onList = file(block("a", "[\"n\"]"));
    const TomlPatch::Result present = OfferedDefaults::apply(onList, defaults, {"a", "a/x"});
    QCOMPARE(emulatorsOf(present.text, "a"), QStringList({"n"}));
    QVERIFY(offeredOf(present.text).contains("a/n"));
}

void OfferedDefaultsTest::missingPlatformSkipsItsEmulators()
{
    const QString defaults
        = QStringLiteral("[[platform]]\nid = \"a\"\nname = \"A\"\nemulators = [\"x\", \"n\"]\n");
    const QString old = file(block("other", "[]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, defaults, {"a"});
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"other"}));
    QCOMPARE(emulatorsOf(r.text, "other"), QStringList());
}

void OfferedDefaultsTest::nothingChangesWhenAllOffered()
{
    const QString old = file(block("a", "[\"x\"]"),
                             "offered = [\"a\", \"a/x\", \"b\", \"b/y\", \"c\", \"c/z\", \"c/w\"]\n\n");
    const TomlPatch::Result r = OfferedDefaults::apply(old, kDefaults, kBootstrap);
    QVERIFY(r.ok);
    QCOMPARE(r.text, old);
}

void OfferedDefaultsTest::offeredWrittenForOldFile()
{
    const QString defaults = QStringLiteral("[[platform]]\nid = \"a\"\nname = \"A\"\nemulators = [\"x\"]\n");
    const QString old = file(block("a", "[\"x\"]"));
    const TomlPatch::Result r = OfferedDefaults::apply(old, defaults, {"a", "a/x"});
    QVERIFY(r.ok);
    QCOMPARE(ids(r.text), QStringList({"a"}));
    QCOMPARE(offeredOf(r.text), QStringList({"a", "a/x"}));
    QVERIFY(r.text.contains("# ids Lattis has offered"));
    QVERIFY(r.text.indexOf("offered = ") < r.text.indexOf("[[platform]]"));
}

void OfferedDefaultsTest::badOfferedIsAnError()
{
    const TomlPatch::Result r
        = OfferedDefaults::apply(file(block("a", "[]"), "offered = \"a\"\n\n"), kDefaults, kBootstrap);
    QVERIFY(!r.ok);
}

void OfferedDefaultsTest::unparsableFileIsAnError()
{
    const TomlPatch::Result r = OfferedDefaults::apply(QStringLiteral("[[platform\nid = "), kDefaults, kBootstrap);
    QVERIFY(!r.ok);
}

void OfferedDefaultsTest::bootstrapMatchesTheDefaultsToday()
{
    const QStringList list = OfferedDefaults::bootstrap();
    QCOMPARE(list.size(), 26);
    QVERIFY(list.contains("switch/ryubing"));
    QVERIFY(!list.contains("linux"));
}

void OfferedDefaultsTest::unparsableDefaultsAreAnError()
{
    const QString mine = file(block("a", "[\"x\"]"));
    QStringList added = {"untouched"};
    const TomlPatch::Result r = OfferedDefaults::apply(mine, QStringLiteral("[[platform]\nid = \"a\""), kBootstrap, &added);
    QVERIFY(!r.ok);
    QVERIFY2(r.error.startsWith(QStringLiteral("the default platforms: ")), qPrintable(r.error));
    QVERIFY(r.text.isEmpty());
    QCOMPARE(added, QStringList{"untouched"});
}

void OfferedDefaultsTest::offeredWithANumberIsAnError()
{
    const TomlPatch::Result r
        = OfferedDefaults::apply(file(block("a", "[]"), "offered = [\"a\", 3]\n\n"), kDefaults, kBootstrap);
    QVERIFY(!r.ok);
    QVERIFY2(r.error.startsWith(QStringLiteral("offered must be a list of text")), qPrintable(r.error));
}

void OfferedDefaultsTest::numbersAndBooleansAreCopiedAsWritten()
{
    const QString defaults = QStringLiteral(
        "[[platform]]\nid = \"a\"\nname = \"A\"\n\n"
        "[[platform]]\nicon = \"n\"\nplayers = 4\nportable = true\ndocked = false\nratio = 1.5\n"
        "extensions = [\"nds\", \"zip\"]\nname = \"New\"\nid = \"n\"\n");
    const TomlPatch::Result r = OfferedDefaults::apply(file(block("a", "[]")), defaults, {"a"});
    QVERIFY2(r.ok, qPrintable(r.error));
    const toml::table root = toml::parse(r.text.toStdString());
    const toml::table *added = nullptr;
    for (const toml::node &entry : *root["platform"].as_array()) {
        if (entry.as_table()->get("id")->value_or(std::string()) == "n")
            added = entry.as_table();
    }
    QVERIFY(added);
    QCOMPARE(added->get("players")->value_or(int64_t(0)), int64_t(4));
    QCOMPARE(added->get("portable")->value_or(false), true);
    QCOMPARE(added->get("docked")->value_or(true), false);
    QCOMPARE(added->get("ratio")->value_or(0.0), 1.5);
    QCOMPARE(added->get("extensions")->as_array()->size(), size_t(2));
    // id, then name, then the rest as the defaults list them.
    const qsizetype id = r.text.indexOf(QStringLiteral("id = \"n\""));
    QVERIFY(id >= 0);
    QVERIFY(id < r.text.indexOf(QStringLiteral("name = \"New\"")));
    QVERIFY(r.text.indexOf(QStringLiteral("name = \"New\"")) < r.text.indexOf(QStringLiteral("icon = \"n\"")));
    QVERIFY(r.text.indexOf(QStringLiteral("icon = \"n\"")) < r.text.indexOf(QStringLiteral("players = 4")));
}

QTEST_MAIN(OfferedDefaultsTest)
#include "offereddefaults_test.moc"
