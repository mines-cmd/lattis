#include "emulators.h"
#include "releasesource.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

const QString kFile = QStringLiteral("library/emulators.toml");

bool anyContains(const QStringList &list, const QString &needle)
{
    for (const QString &s : list) {
        if (s.contains(needle))
            return true;
    }
    return false;
}

QString dump(const QStringList &warnings)
{
    return warnings.join(u'\n');
}

QList<EmulatorDef> mergeText(const QByteArray &text, QStringList *warnings)
{
    const Parsed<QList<EmulatorEntry>> user = Emulators::parseFile(text, kFile);
    *warnings = user.warnings;
    return Emulators::merge(Emulators::presets(), user.value, warnings);
}

const QByteArray kCitra = R"(
[[emulator]]
id = "citra"
name = "Citra"
platforms = ["3ds"]
path = "~/Applications/citra-qt.AppImage"
command = ["{exe}", "{args}", "{rom}"]
extensions = ["3ds", "cci", "cxi", "3dsx"]
)";

bool touch(const QString &path, bool executable = false)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.close();
    if (executable)
        file.setPermissions(file.permissions() | QFile::ExeOwner);
    return true;
}

} // namespace

class EmulatorsTest : public QObject
{
    Q_OBJECT

private slots:
    void presetsAreValid();
    void overrideMergesKeyByKey();
    void userDefinedEmulator();
    void incompleteUserEmulatorSuggestsPreset();
    void validation_data();
    void validation();
    void extensionsNormalized();
    void duplicateAndInvalidIds();
    void syntaxError();
    void referenceRoundTrips();
    void writeReferenceOnlyWhenChanged();
    void docsListEveryKey();
    void presetFolders();
    void singlePathOrList();
    void portableDetected();
    void installPathsPerType();

    void expandHome();
    void installedFlatpaks();
    void detectOrder();
    void detectAppImageNewestVersion();
    void detectAppImageInLaterFolder();
    void detectNothing();
    void flatpakPermissions();
    void flatpakCanRead_data();
    void flatpakCanRead();
};

void EmulatorsTest::presetsAreValid()
{
    const QDir dir(QStringLiteral(":/qt/qml/Launcher/resources/emulators"));
    const QStringList files = dir.entryList({QStringLiteral("*.toml")});
    QCOMPARE(files, (QStringList{"azahar.toml", "cemu.toml", "dolphin.toml", "duckstation.toml", "melonds.toml",
                                 "mgba.toml", "pcsx2.toml", "ppsspp.toml", "rmg.toml", "rpcs3.toml", "ryubing.toml",
                                 "shadps4.toml"}));
    for (const QString &name : files) {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const Parsed<EmulatorEntry> preset = Emulators::parsePreset(file.readAll(), name);
        QVERIFY2(preset.ok, qPrintable(dump(preset.warnings)));
        QVERIFY2(preset.warnings.isEmpty(), qPrintable(dump(preset.warnings)));
        QCOMPARE(preset.value.id + QStringLiteral(".toml"), name);
    }

    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    QVERIFY2(warnings.isEmpty(), qPrintable(dump(warnings)));
    QCOMPARE(defs.size(), 12);
    QCOMPARE(defs.at(0).id, QStringLiteral("azahar"));
    for (const EmulatorDef &d : defs) {
        QVERIFY(d.preset);
        QVERIFY(d.userKeys.isEmpty());
        QVERIFY2(!d.platforms.isEmpty() && !d.flatpak.isEmpty() && !d.appimage.isEmpty(), qPrintable(d.id));
        QVERIFY2(!d.release.isEmpty(), qPrintable(d.id));
        // Dolphin's only AppImage is a community build; every other preset is the project's own.
        QCOMPARE(d.releaseUnofficial.isEmpty(), d.id != QStringLiteral("dolphin"));
        QCOMPARE(d.releaseAsset.isEmpty(), d.release.isEmpty());
        QCOMPARE(d.releaseInside.isEmpty(), !d.releaseAsset.endsWith(QStringLiteral(".zip")));
        if (!d.release.isEmpty())
            QVERIFY2(ReleaseSources::parse(d.release).has_value(), qPrintable(d.id));
        QVERIFY2(d.command.contains("{exe}") && d.command.contains("{args}") && d.command.contains("{rom}"),
                 qPrintable(d.id));
        QVERIFY2(!d.extensions.isEmpty() || !d.files.isEmpty(), qPrintable(d.id));
    }
    QVERIFY(!Emulators::find(defs, "azahar")->extensions.contains(QStringLiteral("cia")));
    QCOMPARE(Emulators::find(defs, "ryubing")->flatpak, QStringLiteral("io.github.ryubing.Ryujinx"));
    QCOMPARE(Emulators::find(defs, "rpcs3")->files, QStringList{"eboot.bin"});
    QCOMPARE(Emulators::find(defs, "dolphin")->platforms, (QStringList{"gamecube", "wii"}));
}

void EmulatorsTest::overrideMergesKeyByKey()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = mergeText(R"(
[[emulator]]
id = "ryubing"
path = "~/Applications/Ryujinx.AppImage"
args = ["--fullscreen"]
)",
                                              &warnings);
    QVERIFY2(warnings.isEmpty(), qPrintable(dump(warnings)));
    const EmulatorDef *ryubing = Emulators::find(defs, QStringLiteral("ryubing"));
    QVERIFY(ryubing);
    QVERIFY(ryubing->preset);
    QCOMPARE(ryubing->path, QStringLiteral("~/Applications/Ryujinx.AppImage"));
    QCOMPARE(ryubing->args, QStringList{QStringLiteral("--fullscreen")});
    QCOMPARE(ryubing->flatpak, QStringLiteral("io.github.ryubing.Ryujinx")); // from the preset
    QCOMPARE(ryubing->name, QStringLiteral("Ryubing"));
    QCOMPARE(ryubing->userKeys, (QStringList{"args", "path"}));

    const EmulatorDef *azahar = Emulators::find(defs, QStringLiteral("azahar"));
    QVERIFY(azahar);
    QVERIFY(azahar->userKeys.isEmpty());
}

void EmulatorsTest::userDefinedEmulator()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = mergeText(kCitra, &warnings);
    QVERIFY2(warnings.isEmpty(), qPrintable(dump(warnings)));
    QCOMPARE(defs.size(), 13);
    const EmulatorDef &citra = defs.last(); // after the presets
    QCOMPARE(citra.id, QStringLiteral("citra"));
    QVERIFY(!citra.preset);
    QCOMPARE(citra.name, QStringLiteral("Citra"));
    QCOMPARE(citra.platforms, QStringList{QStringLiteral("3ds")});
    QCOMPARE(citra.command, (QStringList{"{exe}", "{args}", "{rom}"}));
    QVERIFY(citra.args.isEmpty());
}

void EmulatorsTest::incompleteUserEmulatorSuggestsPreset()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = mergeText(R"(
[[emulator]]
id = "ryujing"
path = "/opt/ryujinx/Ryujinx"
)",
                                              &warnings);
    QCOMPARE(defs.size(), 12);
    QCOMPARE(warnings.size(), 1);
    const QString w = warnings.first();
    QVERIFY2(w.startsWith(kFile + QStringLiteral(":2:")), qPrintable(w));
    QVERIFY2(w.contains(QStringLiteral("isn't built in, so it needs name, command and extensions or files")), qPrintable(w));
    QVERIFY2(w.contains(QStringLiteral("did you mean \"ryubing\"")), qPrintable(w));
}

void EmulatorsTest::validation_data()
{
    QTest::addColumn<QByteArray>("block");
    QTest::addColumn<QString>("warning");

    QTest::newRow("unknown key") << QByteArray("extension = [\"nsp\"]") << "unknown key extension — did you mean \"extensions\"";
    QTest::newRow("wrong type") << QByteArray("name = 3") << "name must be text in quotes";
    QTest::newRow("list type") << QByteArray("args = \"--fullscreen\"") << "args must be a list of text";
    QTest::newRow("no exe") << QByteArray("command = [\"/usr/bin/ryujinx\", \"{rom}\"]") << "command must include \"{exe}\"";
    QTest::newRow("args inside") << QByteArray("command = [\"{exe}\", \"--x={args}\", \"{rom}\"]")
                                 << "{args} must be an argument of its own";
    QTest::newRow("unknown placeholder") << QByteArray("command = [\"{exe}\", \"{romm}\"]")
                                         << "unknown placeholder {romm} in command";
    QTest::newRow("install type") << QByteArray("saves = { natvie = \"~/x\" }")
                                  << "unknown install type natvie in saves; use native, flatpak or portable — did you mean \"native\"";
    QTest::newRow("saves type") << QByteArray("saves = \"~/x\"") << "saves must be a table of folders";
    QTest::newRow("saves list type") << QByteArray("saves = { native = [\"~/a\", 3] }")
                                     << "saves.native must be a path in quotes, or a list of them";
    QTest::newRow("relative native") << QByteArray("saves = { native = \"a/b\" }")
                                     << "saves.native paths must be absolute, or start with ~/";
    QTest::newRow("absolute portable") << QByteArray("saves = { portable = [\"user\", \"/opt/x\"] }")
                                       << "saves.portable paths are relative to the program's folder";
    QTest::newRow("portable outside") << QByteArray("saves = { portable = \"../x\" }")
                                      << "saves.portable paths are relative to the program's folder";
    QTest::newRow("portable marker") << QByteArray("portable = \"../portable.txt\"")
                                     << "portable is a file or folder next to the program";
    QTest::newRow("save extension dot") << QByteArray("saves_beside_rom = [\"sav\", \"sav.bak\"]")
                                        << "saves_beside_rom are extensions without dots";
    QTest::newRow("relative path") << QByteArray("path = \"bin/ryujinx\"") << "path must be absolute";
    QTest::newRow("release scheme") << QByteArray("release = \"mgba-emu/mgba\"")
                                    << "release starts with github: or gitlab:";
    QTest::newRow("release github") << QByteArray("release = \"github:mgba-emu\"")
                                    << "release is \"github:<owner>/<repo>\"";
    QTest::newRow("release gitlab") << QByteArray("release = \"gitlab:git.example.org/project\"")
                                    << "release is \"gitlab:<host>/<group>/<project>\"";
    QTest::newRow("release asset folder") << QByteArray("release_asset = \"dist/app.AppImage\"")
                                          << "release_asset is a file name pattern, without folders";
    QTest::newRow("release inside folder") << QByteArray("release_inside = \"bin/*.AppImage\"")
                                           << "release_inside is a file name pattern, without folders";
    QTest::newRow("appimage folder") << QByteArray("appimage = \"~/Apps/r.AppImage\"") << "appimage is a file name pattern for ~/Applications and ~/AppImages, without folders";
}

void EmulatorsTest::validation()
{
    QFETCH(QByteArray, block);
    QFETCH(QString, warning);

    const Parsed<QList<EmulatorEntry>> parsed =
        Emulators::parseFile("[[emulator]]\nid = \"ryubing\"\n" + block + "\n", kFile);
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.value.size(), 1);
    QVERIFY(parsed.value.first().values.isEmpty() || parsed.value.first().values.contains(QStringLiteral("saves")));
    QVERIFY2(anyContains(parsed.warnings, kFile + QStringLiteral(":3: ") + warning), qPrintable(dump(parsed.warnings)));

    // The preset's own value stays.
    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), parsed.value, &warnings);
    QCOMPARE(defs.size(), 12); // the presets, nothing more
}

void EmulatorsTest::extensionsNormalized()
{
    const Parsed<QList<EmulatorEntry>> parsed = Emulators::parseFile(R"(
[[emulator]]
id = "ryubing"
extensions = [".NSP", "xci"]
saves_beside_rom = [".SAV", "ss1"]
)",
                                                                     kFile);
    QVERIFY(anyContains(parsed.warnings, QStringLiteral(":4: write extensions without the dot")));
    QCOMPARE(parsed.value.first().values.value(QStringLiteral("extensions")).toStringList(),
             (QStringList{"nsp", "xci"}));
    QCOMPARE(parsed.value.first().values.value(QStringLiteral("saves_beside_rom")).toStringList(),
             (QStringList{"sav", "ss1"}));
}

void EmulatorsTest::duplicateAndInvalidIds()
{
    const Parsed<QList<EmulatorEntry>> parsed = Emulators::parseFile(R"(
emulators = 1

[[emulator]]
id = "ryubing"

[[emulator]]
id = "ryubing"

[[emulator]]
id = "Dolphin"

[[emulator]]
name = "No id"
)",
                                                                     kFile);
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.value.size(), 1);
    QVERIFY2(anyContains(parsed.warnings, QStringLiteral(":2: unknown key emulators; emulators go in [[emulator]] "
                                                         "blocks — did you mean \"emulator\"")),
             qPrintable(dump(parsed.warnings)));
    QVERIFY(anyContains(parsed.warnings, QStringLiteral(":7: emulator \"ryubing\" is listed twice; skipped")));
    QVERIFY(anyContains(parsed.warnings, QStringLiteral(":10: emulator id \"Dolphin\" may only use")));
    QVERIFY(anyContains(parsed.warnings, QStringLiteral(":13: emulator without an id; skipped")));
}

void EmulatorsTest::syntaxError()
{
    const Parsed<QList<EmulatorEntry>> parsed = Emulators::parseFile("[[emulator]]\nid = \n", kFile);
    QVERIFY(!parsed.ok);
    QCOMPARE(parsed.warnings.size(), 1);
    QVERIFY(parsed.warnings.first().startsWith(kFile + QStringLiteral(":2:")));
}

void EmulatorsTest::referenceRoundTrips()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = mergeText(kCitra + R"(
[[emulator]]
id = "ryubing"
args = ["--fullscreen"]
)",
                                              &warnings);
    QVERIFY2(warnings.isEmpty(), qPrintable(dump(warnings)));
    const QString text = Emulators::referenceText(defs);

    for (const EmulatorFields::Field &f : EmulatorFields::all())
        QVERIFY2(text.contains(QStringLiteral("#   %1 ").arg(QLatin1String(f.key)))
                     || text.contains(QStringLiteral("#   %1\n#                ").arg(QLatin1String(f.key))),
                 f.key);
    QVERIFY(text.contains(QStringLiteral("# Built in.\n[[emulator]]\nid = \"azahar\"")));
    QVERIFY(text.contains(QStringLiteral("# Built in, with changes from emulators.toml (marked).\n[[emulator]]\n"
                                         "id = \"ryubing\"")));
    QVERIFY(text.contains(QStringLiteral("args = [\"--fullscreen\"]   # emulators.toml\n")));
    QVERIFY(text.contains(QStringLiteral("# Defined in emulators.toml.\n[[emulator]]\nid = \"citra\"")));

    // Read back as a user file, the reference defines the same emulators.
    const Parsed<QList<EmulatorEntry>> reread = Emulators::parseFile(text.toUtf8(), QStringLiteral("reference"));
    QVERIFY2(reread.ok && reread.warnings.isEmpty(), qPrintable(dump(reread.warnings)));
    QStringList mergeWarnings;
    const QList<EmulatorDef> again = Emulators::merge({}, reread.value, &mergeWarnings);
    QVERIFY2(mergeWarnings.isEmpty(), qPrintable(dump(mergeWarnings)));
    QCOMPARE(again.size(), defs.size());
    for (qsizetype i = 0; i < defs.size(); ++i) {
        EmulatorDef expected = defs.at(i);
        expected.preset = false;
        expected.userKeys = again.at(i).userKeys;
        QCOMPARE(again.at(i), expected);
    }
}

void EmulatorsTest::writeReferenceOnlyWhenChanged()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("library")));
    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    const QString path = dir.filePath(QStringLiteral("library/emulators.reference.toml"));

    QVERIFY(Emulators::writeReference(dir.path(), defs));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readAll()), Emulators::referenceText(defs));
    file.close();

    QVERIFY(file.open(QIODevice::Append));
    QVERIFY(file.setFileTime(QDateTime(QDate(2020, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime));
    file.close();
    QVERIFY(Emulators::writeReference(dir.path(), defs));
    QCOMPARE(QFileInfo(path).lastModified().date(), QDate(2020, 1, 1));
}

void EmulatorsTest::docsListEveryKey()
{
    QFile file(QStringLiteral(DOCS_DIR "/dev/library.md"));
    if (!file.open(QIODevice::ReadOnly))
        QSKIP("the library format docs aren't in the public repo");
    // Rows of the emulators.toml table: | `key` | type | description |
    QStringList keys;
    bool inSection = false;
    for (const QString &line : QString::fromUtf8(file.readAll()).split(u'\n')) {
        if (line.startsWith(QLatin1String("## ")) || line.startsWith(QLatin1String("### ")))
            inSection = line == QLatin1String("### `emulators.toml`");
        if (!inSection || !line.startsWith(QStringLiteral("| `")))
            continue;
        const QStringList cells = line.split(u'|');
        QVERIFY(cells.size() >= 4);
        const QString key = cells.at(1).trimmed().remove(u'`');
        const EmulatorFields::Field *field = EmulatorFields::find(key);
        QVERIFY2(field, qPrintable(QStringLiteral("library.md lists unknown key %1").arg(key)));
        QCOMPARE(cells.at(2).trimmed(), EmulatorFields::typeName(field->type));
        keys.append(key);
    }
    QCOMPARE(keys, EmulatorFields::keys());
}

void EmulatorsTest::presetFolders()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    // Those that write saves next to the ROM have no save folders; only they have saves_beside_rom.
    const QStringList besideRom = {QStringLiteral("melonds"), QStringLiteral("mgba")};
    // Those whose portable folder depends on the working folder have no portable entry.
    const QStringList notPortable = {QStringLiteral("azahar"), QStringLiteral("ppsspp"), QStringLiteral("rmg"),
                                     QStringLiteral("shadps4")};
    for (const EmulatorDef &d : defs) {
        QCOMPARE(d.savesBesideRom.isEmpty(), !besideRom.contains(d.id));
        if (besideRom.contains(d.id)) {
            QVERIFY2(d.saves.isEmpty() && d.keys.isEmpty(), qPrintable(d.id));
            QVERIFY(d.savesBesideRom.contains(QStringLiteral("sav")));
            continue;
        }
        const QStringList types = notPortable.contains(d.id) ? QStringList{"flatpak", "native"}
                                                             : QStringList{"flatpak", "native", "portable"};
        QCOMPARE(d.saves.keys(), types);
        QCOMPARE(d.portable.isEmpty(), notPortable.contains(d.id));
        if (!d.keys.isEmpty())
            QCOMPARE(d.keys.keys(), types);
        for (const auto *table : {&d.saves, &d.keys}) {
            for (auto it = table->begin(); it != table->end(); ++it) {
                QVERIFY2(!it.value().isEmpty(), qPrintable(d.id));
                // Each type lists the same things, in the same order.
                QCOMPARE(it.value().size(), table->first().size());
                for (const QString &path : it.value()) {
                    const QString what = d.id + u' ' + it.key() + u' ' + path;
                    if (it.key() == QLatin1String("native"))
                        QVERIFY2(path.startsWith(QLatin1String("~/.")), qPrintable(what));
                    else if (it.key() == QLatin1String("flatpak"))
                        QVERIFY2(path.startsWith(QStringLiteral("~/.var/app/%1/").arg(d.flatpak)), qPrintable(what));
                    else
                        QVERIFY2(!path.startsWith(u'/') && !path.startsWith(u'~'), qPrintable(what));
                    // The same place under each root.
                    QVERIFY2(path.endsWith(table->value(QStringLiteral("native")).at(it.value().indexOf(path))
                                               .section(u'/', -1)),
                             qPrintable(what));
                }
            }
        }
        // They all expand to absolute paths.
        EmulatorInstall install;
        install.kind = EmulatorInstall::Kind::Path;
        install.program = QStringLiteral("/opt/emu/emu");
        for (const QString &path : Emulators::installPaths(d.saves, install, QStringLiteral("/home/u")))
            QVERIFY2(path.startsWith(QLatin1String("/home/u/.")), qPrintable(path));
    }
}

void EmulatorsTest::singlePathOrList()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = mergeText(R"(
[[emulator]]
id = "mgba"
saves = { native = "~/gba saves", flatpak = ["~/a", "~/b"] }
)",
                                              &warnings);
    QVERIFY2(warnings.isEmpty(), qPrintable(dump(warnings)));
    const EmulatorDef *mgba = Emulators::find(defs, QStringLiteral("mgba"));
    QCOMPARE(mgba->saves.value(QStringLiteral("native")), QStringList{"~/gba saves"});
    QCOMPARE(mgba->saves.value(QStringLiteral("flatpak")), (QStringList{"~/a", "~/b"}));
    // One path is written as one, several as a list.
    const QString text = Emulators::referenceText(defs);
    QVERIFY2(text.contains(QStringLiteral("saves = { native = \"~/gba saves\", flatpak = [\"~/a\", \"~/b\"] }")),
             qPrintable(text));
}

void EmulatorsTest::portableDetected()
{
    QTemporaryDir dir;
    InstallLocations where;
    where.home = dir.path();
    where.applicationsDirs = {dir.filePath(QStringLiteral("Applications"))};
    QVERIFY(touch(dir.filePath(QStringLiteral("dolphin/dolphin-emu")), true));
    QVERIFY(touch(dir.filePath(QStringLiteral("Applications/DuckStation-x64.AppImage")), true));
    QVERIFY(touch(dir.filePath(QStringLiteral("Applications/rpcs3-v0.0.38_linux64.AppImage")), true));

    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    EmulatorDef dolphin = *Emulators::find(defs, QStringLiteral("dolphin"));
    dolphin.path = QStringLiteral("~/dolphin/dolphin-emu");
    const EmulatorDef duckstation = *Emulators::find(defs, QStringLiteral("duckstation"));
    const EmulatorDef rpcs3 = *Emulators::find(defs, QStringLiteral("rpcs3"));

    // Without their markers: native.
    QCOMPARE(Emulators::detect(dolphin, where, {}).installType(), QStringLiteral("native"));
    QCOMPARE(Emulators::detect(duckstation, where, {}).installType(), QStringLiteral("native"));
    QCOMPARE(Emulators::detect(rpcs3, where, {}).installType(), QStringLiteral("native"));

    // A file next to a program, a file and a folder next to AppImages.
    QVERIFY(touch(dir.filePath(QStringLiteral("dolphin/portable.txt"))));
    QVERIFY(touch(dir.filePath(QStringLiteral("Applications/portable.txt"))));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Applications/portable")));
    const EmulatorInstall d = Emulators::detect(dolphin, where, {});
    QCOMPARE(d.installType(), QStringLiteral("portable"));
    QCOMPARE(d.describe(where.home), QStringLiteral("~/dolphin/dolphin-emu (portable)"));
    QCOMPARE(Emulators::detect(duckstation, where, {}).installType(), QStringLiteral("portable"));
    QCOMPARE(Emulators::detect(rpcs3, where, {}).installType(), QStringLiteral("portable"));

    // A Flatpak is never portable, and an emulator without `portable` isn't either.
    EmulatorDef flatpakDolphin = *Emulators::find(defs, QStringLiteral("dolphin"));
    QCOMPARE(Emulators::detect(flatpakDolphin, where, {flatpakDolphin.flatpak}).installType(), QStringLiteral("flatpak"));
    dolphin.portable.clear();
    QCOMPARE(Emulators::detect(dolphin, where, {}).installType(), QStringLiteral("native"));
}

void EmulatorsTest::installPathsPerType()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    const EmulatorDef *dolphin = Emulators::find(defs, QStringLiteral("dolphin"));
    const QString home = QStringLiteral("/home/u");

    EmulatorInstall install;
    install.kind = EmulatorInstall::Kind::Executable;
    install.program = QStringLiteral("/usr/bin/dolphin-emu");
    QCOMPARE(Emulators::installPaths(dolphin->saves, install, home),
             (QStringList{"/home/u/.local/share/dolphin-emu/GC", "/home/u/.local/share/dolphin-emu/Wii",
                          "/home/u/.local/share/dolphin-emu/StateSaves"}));
    // It has no keys.
    QVERIFY(Emulators::installPaths(dolphin->keys, install, home).isEmpty());

    install.portable = true;
    install.program = QStringLiteral("/opt/dolphin/dolphin-emu");
    QCOMPARE(Emulators::installPaths(dolphin->saves, install, home),
             (QStringList{"/opt/dolphin/User/GC", "/opt/dolphin/User/Wii", "/opt/dolphin/User/StateSaves"}));

    install = {};
    install.kind = EmulatorInstall::Kind::Flatpak;
    install.program = dolphin->flatpak;
    QCOMPARE(Emulators::installPaths(dolphin->saves, install, home).first(),
             QStringLiteral("/home/u/.var/app/org.DolphinEmu.dolphin-emu/data/dolphin-emu/GC"));

    // Not installed: nothing.
    QVERIFY(Emulators::installPaths(dolphin->saves, EmulatorInstall(), home).isEmpty());
}

void EmulatorsTest::expandHome()
{
    QCOMPARE(Emulators::expandHome(QStringLiteral("~/Applications/a"), QStringLiteral("/home/u")),
             QStringLiteral("/home/u/Applications/a"));
    QCOMPARE(Emulators::expandHome(QStringLiteral("~"), QStringLiteral("/home/u")), QStringLiteral("/home/u"));
    QCOMPARE(Emulators::expandHome(QStringLiteral("/opt/~x"), QStringLiteral("/home/u")), QStringLiteral("/opt/~x"));
    QCOMPARE(Emulators::expandHome(QStringLiteral("~other/x"), QStringLiteral("/home/u")), QStringLiteral("~other/x"));
}

void EmulatorsTest::installedFlatpaks()
{
    QTemporaryDir dir;
    const QString user = dir.filePath(QStringLiteral("user"));
    const QString system = dir.filePath(QStringLiteral("system"));
    QVERIFY(QDir().mkpath(user + QStringLiteral("/io.github.ryubing.Ryujinx/current")));
    QVERIFY(QDir().mkpath(system + QStringLiteral("/org.azahar_emu.Azahar/current")));
    QVERIFY(QDir().mkpath(system + QStringLiteral("/org.half.Removed/x86_64"))); // no `current`

    const QSet<QString> apps = Emulators::installedFlatpaks({user, system, dir.filePath(QStringLiteral("none"))});
    QCOMPARE(apps, (QSet<QString>{"io.github.ryubing.Ryujinx", "org.azahar_emu.Azahar"}));
}

void EmulatorsTest::detectOrder()
{
    using Kind = EmulatorInstall::Kind;
    QTemporaryDir dir;
    InstallLocations where;
    where.home = dir.path();
    where.pathDirs = {dir.filePath(QStringLiteral("bin")), dir.filePath(QStringLiteral("usr/bin"))};
    where.applicationsDirs = {dir.filePath(QStringLiteral("Applications")), dir.filePath(QStringLiteral("AppImages"))};
    QVERIFY(touch(dir.filePath(QStringLiteral("bin/Ryujinx")))); // not executable: skipped
    QVERIFY(touch(dir.filePath(QStringLiteral("usr/bin/ryujinx")), true));
    QVERIFY(touch(dir.filePath(QStringLiteral("Applications/Ryujinx-1.3.3-x64.AppImage"))));
    QVERIFY(touch(dir.filePath(QStringLiteral("opt/Ryujinx.AppImage")), true));

    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    EmulatorDef ryubing = *Emulators::find(defs, QStringLiteral("ryubing"));
    const QSet<QString> withFlatpak = {ryubing.flatpak};

    ryubing.path = QStringLiteral("~/opt/Ryujinx.AppImage");
    EmulatorInstall install = Emulators::detect(ryubing, where, withFlatpak);
    QCOMPARE(install.kind, Kind::Path);
    QCOMPARE(install.program, dir.filePath(QStringLiteral("opt/Ryujinx.AppImage")));
    QCOMPARE(install.describe(where.home), QStringLiteral("~/opt/Ryujinx.AppImage"));
    QCOMPARE(install.installType(), QStringLiteral("native"));

    ryubing.path = QStringLiteral("~/opt/Gone.AppImage");
    install = Emulators::detect(ryubing, where, withFlatpak);
    QCOMPARE(install.kind, Kind::Flatpak);
    QVERIFY(install.pathMissing);
    QCOMPARE(install.describe(where.home), QStringLiteral("Flatpak io.github.ryubing.Ryujinx"));
    QCOMPARE(install.installType(), QStringLiteral("flatpak"));

    ryubing.path.clear();
    install = Emulators::detect(ryubing, where, {});
    QCOMPARE(install.kind, Kind::Executable);
    QCOMPARE(install.program, dir.filePath(QStringLiteral("usr/bin/ryujinx")));
    QVERIFY(!install.pathMissing);

    where.pathDirs.clear();
    install = Emulators::detect(ryubing, where, {});
    QCOMPARE(install.kind, Kind::AppImage);
    QCOMPARE(install.describe(where.home), QStringLiteral("~/Applications/Ryujinx-1.3.3-x64.AppImage"));
}

void EmulatorsTest::detectAppImageNewestVersion()
{
    QTemporaryDir dir;
    InstallLocations where;
    where.home = dir.path();
    where.applicationsDirs = {dir.path()};
    QVERIFY(touch(dir.filePath(QStringLiteral("ryujinx-1.9.0-x64.AppImage"))));
    QVERIFY(touch(dir.filePath(QStringLiteral("Ryujinx-1.10.0-x64.AppImage"))));
    QVERIFY(touch(dir.filePath(QStringLiteral("ryujinx-notes.txt"))));

    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    const QHash<QString, EmulatorInstall> installs = Emulators::detectAll(defs, where);
    QCOMPARE(installs.value(QStringLiteral("ryubing")).program, dir.filePath(QStringLiteral("Ryujinx-1.10.0-x64.AppImage")));
    QVERIFY(!installs.value(QStringLiteral("azahar")).found());
}

void EmulatorsTest::detectAppImageInLaterFolder()
{
    // Gear Lever's layout: lowercase names in ~/AppImages.
    QTemporaryDir dir;
    InstallLocations where;
    where.home = dir.path();
    where.applicationsDirs = {dir.filePath(QStringLiteral("Applications")), dir.filePath(QStringLiteral("AppImages"))};
    QVERIFY(touch(dir.filePath(QStringLiteral("AppImages/ryujinx.appimage")), true));
    QVERIFY(touch(dir.filePath(QStringLiteral("AppImages/azahar.appimage")), true));
    QVERIFY(touch(dir.filePath(QStringLiteral("Applications/azahar-2125.AppImage")), true));

    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    const QHash<QString, EmulatorInstall> installs = Emulators::detectAll(defs, where);
    QCOMPARE(installs.value(QStringLiteral("ryubing")).describe(where.home), QStringLiteral("~/AppImages/ryujinx.appimage"));
    // The first folder with a match wins.
    QCOMPARE(installs.value(QStringLiteral("azahar")).describe(where.home),
             QStringLiteral("~/Applications/azahar-2125.AppImage"));
}

void EmulatorsTest::detectNothing()
{
    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    const EmulatorInstall install = Emulators::detect(defs.first(), InstallLocations(), {});
    QVERIFY(!install.found());
    QCOMPARE(install.describe(QString()), QStringLiteral("Not found"));
    QCOMPARE(install.installType(), QString());
}

namespace {

bool writeText(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(text) == text.size();
}

} // namespace

void EmulatorsTest::flatpakPermissions()
{
    // The layout of ~/.local/share/flatpak and /var/lib/flatpak: app/<id>/current/active/
    // metadata, and overrides/ next to app/.
    QTemporaryDir dir;
    const QString user = dir.filePath(QStringLiteral("user/app"));
    const QString system = dir.filePath(QStringLiteral("system/app"));
    const QString app = QStringLiteral("org.azahar_emu.Azahar");
    QVERIFY(writeText(system + "/" + app + "/current/active/metadata",
                      "[Application]\nname=org.azahar_emu.Azahar\n\n[Context]\nshared=network;ipc;\n"
                      "filesystems=xdg-download:ro;home;/media;\n\n[Session Bus Policy]\nfilesystems=nope\n"));
    QVERIFY(writeText(dir.filePath("system/overrides/global"), "[Context]\nfilesystems=/mnt/games:ro;\n"));
    QVERIFY(writeText(dir.filePath("user/overrides/" + app), "[Context]\nfilesystems=!home;~/Games/3ds:create;\n"));
    QVERIFY(writeText(dir.filePath("user/overrides/global"), "[Context]\nfilesystems=!/media;\n"));

    // Metadata, then the system's global and app overrides, then the user's.
    QCOMPARE(Emulators::flatpakFilesystems(app, {user, system}),
             QStringList({"xdg-download", "/mnt/games", "~/Games/3ds"}));
    QCOMPARE(Emulators::flatpakFilesystems(QStringLiteral("org.not.Installed"), {user, system}),
             QStringList({"/mnt/games"}));

    // Detection reports them for a Flatpak install.
    QStringList warnings;
    const QList<EmulatorDef> defs = Emulators::merge(Emulators::presets(), {}, &warnings);
    InstallLocations where;
    where.home = dir.filePath(QStringLiteral("home"));
    where.flatpakDirs = {user, system};
    const EmulatorInstall install = Emulators::detect(*Emulators::find(defs, "azahar"), where, {app});
    QCOMPARE(install.filesystems, QStringList({"xdg-download", "/mnt/games", "~/Games/3ds"}));
}

void EmulatorsTest::flatpakCanRead_data()
{
    QTest::addColumn<QStringList>("filesystems");
    QTest::addColumn<QString>("folder");
    QTest::addColumn<bool>("readable");

    QTest::newRow("nothing") << QStringList() << "/home/me/Games" << false;
    QTest::newRow("host") << QStringList{"host"} << "/mnt/x" << true;
    QTest::newRow("home") << QStringList{"home"} << "/home/me/Games/switch" << true;
    QTest::newRow("home is not /mnt") << QStringList{"home"} << "/mnt/games" << false;
    QTest::newRow("tilde folder") << QStringList{"~/Games"} << "/home/me/Games/switch" << true;
    QTest::newRow("tilde folder itself") << QStringList{"~/Games"} << "/home/me/Games" << true;
    QTest::newRow("tilde prefix is not a parent") << QStringList{"~/Games"} << "/home/me/GamesOld" << false;
    QTest::newRow("absolute") << QStringList{"/mnt/games/"} << "/mnt/games/3ds" << true;
    QTest::newRow("xdg") << QStringList{"xdg-download"} << "/home/me/Downloads/roms" << true;
    QTest::newRow("xdg subfolder") << QStringList{"xdg-download/roms"} << "/home/me/Downloads/roms/3ds" << true;
    QTest::newRow("xdg other subfolder") << QStringList{"xdg-download/roms"} << "/home/me/Downloads/iso" << false;
    QTest::newRow("unknown xdg") << QStringList{"xdg-nope"} << "/home/me/Downloads" << false;
    QTest::newRow("host-os") << QStringList{"host-os"} << "/usr/share/roms" << false;
}

void EmulatorsTest::flatpakCanRead()
{
    QFETCH(QStringList, filesystems);
    QFETCH(QString, folder);
    QFETCH(bool, readable);
    InstallLocations where;
    where.home = QStringLiteral("/home/me");
    where.xdgDirs = {{QStringLiteral("download"), QStringLiteral("/home/me/Downloads")}};
    QCOMPARE(Emulators::flatpakCanRead(filesystems, folder, where), readable);
}

QTEST_APPLESS_MAIN(EmulatorsTest)
#include "emulators_test.moc"
