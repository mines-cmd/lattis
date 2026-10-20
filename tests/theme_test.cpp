#include "schemamodel.h"
#include "theme.h"
#include "themeschema.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// Exercises the settings-window path end to end: setValue() applies live, then persists
// after the debounce into the file of the setting's half (colors or style); resetValue()
// removes the key; switching colors, a style or a theme updates settings.toml. No QML engine
// involved: Theme is plain C++ here.
class ThemeTest : public QObject
{
    Q_OBJECT

private slots:
    void setValueAppliesLiveAndSaves();
    void typedHexWithAlphaSavesAsRgba();
    void changedOnlyFiltersToCustomizedSettings();
    void resetValueRemovesKey();
    void heroLogoPositionAcceptsCenterSides();
    void heroScrimSettings();
    void radiusTiersDriveMembers();
    void perElementRadiusWinsOverTier();
    void radiusTierLinksHold();
    void migrateLayoutRadiusToMedium();
    void migrateLayoutRadiusSmallToSmall();
    void migrateReferenceValue();
    void migrationIdempotent();
    void migrationSkipsExistingNewKey();
    void createColorsAndStyleSwitchTheActiveOnes();
    void newNamesAreChecked();
    void saveThemeNamesTheActivePair();
    void halfModelsListOnlyTheirOwnSettings();
    void customizeModelListsBothHalves();
    void previewReadsAnotherTheme();
    void previewCarriesThePopupFields();
    void themePopupSettingsHaveTheirDefaults();
    void styleDescriptions();
    void styleSummaryListsWhatDiffers();
    void builtInsResolveWithoutProblems();
    void builtInStylesDifferWhereDecided();
    void eachHalfLoadsItsOwnSettings();
    void wrongHalfIsReportedAndIgnored();
    void firstEditOfABuiltInWritesItsLayer();
    void userFileLayersOnBuiltInOfTheSameName();
    void builtInsExtendTheOriginalBuiltIn();
    void editsRouteByHalf();
    void switchingTheStyleStartsANewSession();
    void resetToBuiltInRemovesTheLayer();
    void openingAnUneditedBuiltInWritesTheHeaderFirst();
    void activeThemeIsEmptyWhenThePairMatchesNone();
    void missingFileFallsBackToBuiltIn();
    void newFilesReload();
    void deletingRefusesBuiltInsAndUnknownNames();
    void usedByListsThemesAndExtendingFiles();
    void deletingTheActiveColorsSwitchesToEmber();
    void deletingAThemeLeavesItsHalves();
    void renamingColorsRepointsWhatNamesThem();
    void renamingRefusesBuiltInsAndTakenNames();
    void aFailingTrashKeepsTheFile();
    void referenceFilesAreWritten();
    void gradientEndDefaultsMatchStart();
    void gradientEndFollowsStartOverride();
    void layoutDirectionPropagates();
    void unknownDirectionFallsBack();
    void gradientEndSchemaIntegrity();
    void paletteEndCascades();
    void onboardingNewKeysResolveInDefault();
    void cardBackgroundFollowsSurface();
    void downloadBarResolvesInDefault();
    void motionDefaultsAndRanges();
    void schemaModelRows();
    void schemaModelFilter();

private:
    QString writeDataDir(QTemporaryDir &dir);
    static void write(const QString &path, const QByteArray &content);
    static QString read(const QString &path);
    // Colors and a style that are empty files named after no built-in: the schema defaults.
    static void usePlainDefaults(const QString &dataDir);
};

QString ThemeTest::writeDataDir(QTemporaryDir &dir)
{
    const QString path = dir.path();
    QDir().mkpath(path + "/themes");
    return path;
}

void ThemeTest::write(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QString ThemeTest::read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

void ThemeTest::usePlainDefaults(const QString &dataDir)
{
    write(dataDir + "/settings.toml", "[appearance]\ncolors = \"Plain\"\nstyle = \"Plain\"\n");
    write(dataDir + "/themes/colors/Plain.toml", "");
    write(dataDir + "/themes/styles/Plain.toml", "");
}

void ThemeTest::heroLogoPositionAcceptsCenterSides()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));

    for (const char *position : {"center_left", "center_right"}) {
        theme.setValue("hero", "logo_position", QString::fromLatin1(position));
        QCOMPARE(theme.resolvedValue("hero", "logo_position").toString(), QString::fromLatin1(position));
    }
}

void ThemeTest::heroScrimSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));

    QCOMPARE(theme.resolvedValue("hero", "scrim_style").toString(), QStringLiteral("shadow"));
    QCOMPARE(theme.resolvedValue("hero", "scrim_side").toString(), QStringLiteral("auto"));
    QCOMPARE(theme.resolvedValue("hero", "shadow_size").toInt(), 48);
    QCOMPARE(theme.resolvedValue("hero", "shadow_offset").toInt(), 0);
    for (const char *side : {"auto", "top", "bottom", "left", "right", "full"}) {
        theme.setValue("hero", "scrim_side", QString::fromLatin1(side));
        QCOMPARE(theme.resolvedValue("hero", "scrim_side").toString(), QString::fromLatin1(side));
    }
    theme.setValue("hero", "scrim_style", QStringLiteral("gradient"));
    QCOMPARE(theme.resolvedValue("hero", "scrim_style").toString(), QStringLiteral("gradient"));
    theme.setValue("hero", "shadow_offset", -12);
    QCOMPARE(theme.resolvedValue("hero", "shadow_offset").toInt(), -12);
}

void ThemeTest::setValueAppliesLiveAndSaves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QCOMPARE(theme.activeTheme(), QStringLiteral("Ember"));

    theme.setValue("palette", "accent", QColor("#ff0000"));

    // Applied immediately, before any save.
    auto *palette = theme.property("palette").value<QObject *>();
    QVERIFY(palette);
    QCOMPARE(palette->property("accent").value<QColor>(), QColor("#ff0000"));
    QCOMPARE(theme.isCustomized("palette", "accent"), true);

    // Debounced save (400ms); wait for it rather than for a fixed sleep.
    QTest::qWait(700);

    const QString contents = read(dataDir + "/themes/colors/Ember.toml");
    QVERIFY2(contents.contains("accent = \"#ff0000\""), qPrintable(contents));
    QVERIFY(!QFile::exists(dataDir + "/themes/styles/Soft.toml"));
}

// The settings window passes typed text through as-is. "#rrggbbaa" must be saved in that
// order; QColor on its own would read 8 digits as #aarrggbb.
void ThemeTest::typedHexWithAlphaSavesAsRgba()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    theme.setValue("palette", "surface", QStringLiteral("#10101080"));
    theme.setValue("tags", "palette", QVariantList{QColor("#c0392b"), QStringLiteral("#20304080")});

    auto *palette = theme.property("palette").value<QObject *>();
    QCOMPARE(palette->property("surface").value<QColor>(), QColor(0x10, 0x10, 0x10, 0x80));

    QTest::qWait(700);

    const QString contents = read(dataDir + "/themes/colors/Ember.toml");
    QVERIFY2(contents.contains("surface = \"#10101080\""), qPrintable(contents));
    QVERIFY2(contents.contains("palette = [\"#c0392b\", \"#20304080\"]"), qPrintable(contents));
}

void ThemeTest::changedOnlyFiltersToCustomizedSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    auto *model = qobject_cast<ThemeSettingsModel *>(theme.styleSchema());
    auto *colorsModel = qobject_cast<ThemeSettingsModel *>(theme.colorsSchema());
    QVERIFY(model);
    QCOMPARE(model->changedCount(), 0);

    model->setChangedOnly(true);
    QCOMPARE(model->rowCount(), 0);

    theme.setValue("radius", "medium", 30);
    QCOMPARE(model->changedCount(), 1);
    QCOMPARE(colorsModel->changedCount(), 0); // counted per half
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->data(model->index(0), ThemeSettingsModel::KeyRole).toString(), QStringLiteral("medium"));

    // Editing a row that's already shown must not reset the model (that would close an
    // open color picker mid-drag).
    QSignalSpy resets(model, &QAbstractItemModel::modelReset);
    theme.setValue("radius", "medium", 31);
    QCOMPARE(resets.count(), 0);

    theme.resetValue("radius", "medium");
    QCOMPARE(model->changedCount(), 0);
    QCOMPARE(model->rowCount(), 0);

    model->setChangedOnly(false);
    QVERIFY(model->rowCount() > 100);
}

void ThemeTest::resetValueRemovesKey()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    theme.setValue("radius", "medium", 30);
    QTest::qWait(700);
    QVERIFY(read(dataDir + "/themes/styles/Soft.toml").contains("medium = 30"));

    theme.resetValue("radius", "medium");
    QCOMPARE(theme.isCustomized("radius", "medium"), false);
    QVERIFY(!read(dataDir + "/themes/styles/Soft.toml").contains("medium = 30"));
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 8); // Soft's own
}

// New colors or a style extend the one they're based on and become the active one.
void ThemeTest::createColorsAndStyleSwitchTheActiveOnes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QVERIFY(theme.createColors("nightly", "Mythos"));
    QCOMPARE(theme.activeColors(), QStringLiteral("nightly"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Soft"));
    QCOMPARE(theme.activeTheme(), QString());
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#04a5ff"));
    QVERIFY(read(dataDir + "/themes/colors/nightly.toml").contains("extends = \"Mythos\""));
    QVERIFY(!QFile::exists(dataDir + "/themes/styles/nightly.toml"));

    QVERIFY(theme.createStyle("boxy", "Sharp"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("boxy"));
    QCOMPARE(theme.activeColors(), QStringLiteral("nightly"));
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 0);
    const QString settings = read(dataDir + "/settings.toml");
    QVERIFY2(settings.contains("colors = \"nightly\""), qPrintable(settings));
    QVERIFY2(settings.contains("style = \"boxy\""), qPrintable(settings));

    // Empty `basedOn` means the one in use.
    QVERIFY(theme.createStyle("boxier", ""));
    QVERIFY(read(dataDir + "/themes/styles/boxier.toml").contains("extends = \"boxy\""));
}

void ThemeTest::newNamesAreChecked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Taken.toml", "");

    Theme theme(dataDir);
    for (const QString kind : {"colors", "style"}) {
        QCOMPARE(theme.nameError(kind, "Fresh name"), QString());
        QVERIFY(!theme.nameError(kind, "").isEmpty());
        QVERIFY(!theme.nameError(kind, "  ").isEmpty());
        QVERIFY(!theme.nameError(kind, "a/b").isEmpty());
        QVERIFY(!theme.nameError(kind, "reference").isEmpty());
    }
    // Built-ins and names in use are refused, each half on its own.
    QVERIFY(!theme.nameError("colors", "Ember").isEmpty());
    QVERIFY(!theme.nameError("style", "Soft").isEmpty());
    QVERIFY(!theme.nameError("colors", "Taken").isEmpty());
    QVERIFY(theme.nameError("style", "Taken").isEmpty());
    QVERIFY(theme.nameError("style", "Ember").isEmpty());

    QVERIFY(!theme.createColors("Ember", "Ember"));
    QVERIFY(!theme.createColors("reference", "Ember"));
    QVERIFY(!theme.createColors("Taken", "Ember"));
    QVERIFY(!theme.createStyle("a/b", "Soft"));
    QCOMPARE(theme.activeColors(), QStringLiteral("Ember"));
}

// A theme is a name for the active pair; saving over a theme of the user's needs the caller to have asked.
void ThemeTest::saveThemeNamesTheActivePair()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    theme.setStyle("Sharp");
    QCOMPARE(theme.activeTheme(), QString());

    QVERIFY(!theme.themeExists("Angular"));
    QVERIFY(theme.nameError("theme", "Angular").isEmpty());
    QVERIFY(theme.saveTheme("Angular"));
    QCOMPARE(read(dataDir + "/themes/Angular.toml"), QStringLiteral("colors = \"Ember\"\nstyle = \"Sharp\"\n"));
    QVERIFY(theme.themeExists("Angular"));
    QVERIFY(theme.availableThemes().contains("Angular"));
    QCOMPARE(theme.activeTheme(), QStringLiteral("Angular"));

    // It exists now: replacing it is refused unless asked for, and then it names the new pair.
    theme.setStyle("Round");
    QVERIFY(theme.themeExists("Angular"));
    QVERIFY(!theme.saveTheme("Angular"));
    QVERIFY(read(dataDir + "/themes/Angular.toml").contains("Sharp"));
    QVERIFY(theme.saveTheme("Angular", true));
    QVERIFY(read(dataDir + "/themes/Angular.toml").contains("Round"));

    // Built-in themes can't be replaced, and the name rules hold.
    QVERIFY(!theme.nameError("theme", "Ember").isEmpty());
    QVERIFY(!theme.saveTheme("Ember", true));
    QVERIFY(!theme.saveTheme("reference"));
    QVERIFY(!theme.saveTheme("a/b"));
    QVERIFY(!QFile::exists(dataDir + "/themes/Ember.toml"));
}

// The setup's theme cards: another theme's colors, read without switching to it.
void ThemeTest::previewReadsAnotherTheme()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/mine.toml", "colors = \"mine\"\nstyle = \"Soft\"\n");
    write(dataDir + "/themes/colors/mine.toml", "[palette]\nsurface = \"#202020\"\naccent = \"#d0bcff\"\n");

    Theme theme(dataDir);
    theme.setValue("palette", "accent", QColor("#ff0000"));
    const QStringList problems = theme.problems();

    const QVariantMap own = theme.preview("mine", "Soft");
    QCOMPARE(own.value("surface").value<QColor>(), QColor("#202020"));
    QCOMPARE(own.value("accent").value<QColor>(), QColor("#d0bcff"));
    QCOMPARE(own.value("background").value<QColor>(), QColor("#050505"));
    QVERIFY(own.value("rail").value<QColor>().isValid());
    QCOMPARE(own.value("launch").value<QColor>(), QColor("#d0bcff"));

    QCOMPARE(theme.preview("Mythos", "Soft").value("accent").value<QColor>(), QColor("#04a5ff"));

    // The pair is any colors with any style: Ember's colors with Sharp's corners.
    const QVariantMap sharp = theme.preview("Ember", "Sharp");
    QCOMPARE(sharp.value("accent").value<QColor>(), QColor("#e15f24"));
    for (const QString tier : {"large", "medium", "small", "tiny"})
        QCOMPARE(sharp.value("radius").toMap().value(tier).toInt(), 0);
    QCOMPARE(sharp.value("searchRadius").toInt(), 0);
    QCOMPARE(theme.preview("Ember", "Soft").value("radius").toMap().value("medium").toInt(), 8);

    QCOMPARE(theme.themePair("mine").value("colors").toString(), QStringLiteral("mine"));
    QCOMPARE(theme.themePair("Steam").value("style").toString(), QStringLiteral("Steam"));
    QVERIFY(theme.themePair("missing").isEmpty());

    // The active theme and its unsaved edit are untouched.
    QCOMPARE(theme.activeTheme(), QStringLiteral("Ember"));
    auto *palette = theme.property("palette").value<QObject *>();
    QCOMPARE(palette->property("accent").value<QColor>(), QColor("#ff0000"));
    const QColor surfaceAfterPreviews = theme.resolvedValue("palette", "surface").value<QColor>();
    QCOMPARE(surfaceAfterPreviews, theme.preview("Ember", "Soft").value("surface").value<QColor>());
    QCOMPARE(theme.problems(), problems);
}

// What the theme popup draws for a pair: launch states, and the style's corners, borders and rail.
void ThemeTest::previewCarriesThePopupFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    Theme theme(dataDir);

    const QVariantMap soft = theme.preview("Ember", "Soft");
    QVERIFY(soft.value("launchRunning").value<QColor>().isValid());
    QVERIFY(soft.value("launchInstall").value<QColor>().isValid());
    QCOMPARE(soft.value("launchInstall").value<QColor>(), theme.resolvedValue("launch_button", "install_background").value<QColor>());
    QCOMPARE(soft.value("railBorderWidth").toInt(), 2);
    QCOMPARE(soft.value("searchRadius").toInt(), 8);
    QCOMPARE(soft.value("launchRadius").toInt(), 8);
    QCOMPARE(soft.value("launchBorderWidth").toInt(), 0);
    QCOMPARE(soft.value("launchLabel").toString(), QStringLiteral("Launch"));
    QCOMPARE(soft.value("railContainer").toBool(), true);
    QCOMPARE(soft.value("railContainerRadius").toInt(), 12);
    QCOMPARE(soft.value("railButtonRadius").toInt(), 8);

    const QVariantMap steam = theme.preview("Steam", "Steam");
    QCOMPARE(steam.value("railBorderWidth").toInt(), 2);
    QCOMPARE(steam.value("searchBorderWidth").toInt(), 0);
    QCOMPARE(steam.value("launchLabel").toString(), QStringLiteral("▶  PLAY"));
    QCOMPARE(steam.value("launchRunning").value<QColor>(), theme.preview("Steam", "Soft").value("launchRunning").value<QColor>());

    // A missing half falls back like the loader does.
    const QVariantMap missing = theme.preview("Ember", "NoSuchStyle");
    QCOMPARE(missing.value("launchLabel").toString(), QStringLiteral("Launch"));
    QCOMPARE(missing.value("searchRadius").toInt(), 8);
    QVERIFY(missing.value("launchRunning").value<QColor>().isValid());
}

void ThemeTest::themePopupSettingsHaveTheirDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));

    const auto number = [&](const char *key) { return theme.resolvedValue("settings_window", key).toInt(); };
    QCOMPARE(number("theme_popup_width"), 720);
    QCOMPARE(number("theme_popup_list_height"), 420);
    QCOMPARE(number("theme_popup_padding"), 10);
    QCOMPARE(number("theme_popup_column_spacing"), 10);
    QCOMPARE(number("theme_popup_row_spacing"), 3);
    QCOMPARE(number("theme_popup_row_radius"), theme.resolvedValue("radius", "small").toInt());
    QCOMPARE(number("theme_popup_selected_border_width"), 1);
    QCOMPARE(number("theme_popup_swatch_size"), 16);
    QCOMPARE(number("theme_popup_swatch_spacing"), 4);
    QCOMPARE(number("theme_popup_button_height"), number("footer_button_height"));
    QCOMPARE(number("theme_popup_fade_height"), 18);
    QCOMPARE(number("theme_popup_header_font_size"), 11);
    QCOMPARE(number("theme_popup_description_font_size"), theme.resolvedValue("font", "small_size").toInt());
    QCOMPARE(number("theme_popup_scene_height"), 30);
    QCOMPARE(number("theme_popup_scene_padding"), 8);

    const auto color = [&](const char *key) { return theme.resolvedValue("settings_window", key).value<QColor>(); };
    QCOMPARE(color("theme_popup_selected_background"), theme.resolvedValue("palette", "selected_overlay").value<QColor>());
    QCOMPARE(color("theme_popup_selected_border"), theme.resolvedValue("palette", "accent").value<QColor>());
    QCOMPARE(color("theme_popup_header_color"), theme.resolvedValue("palette", "text_muted").value<QColor>());
    QCOMPARE(color("theme_popup_scene_background"), theme.resolvedValue("palette", "background").value<QColor>());
}

void ThemeTest::styleDescriptions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/styles/Sharp.toml", "[radius]\ntiny = 1\n");
    write(dataDir + "/themes/styles/long.toml", "extends = \"Soft\"\ndescription = \"" + QByteArray(60, 'x') + "\"\n");
    write(dataDir + "/themes/styles/bare.toml", "extends = \"Soft\"\n");

    Theme theme(dataDir);
    QCOMPARE(theme.styleDescription("Sharp"), QStringLiteral("Square corners, rounded tags and chips"));
    QVERIFY(!theme.styleDescription("Soft").isEmpty());
    QCOMPARE(theme.styleDescription("bare"), QString());
    QCOMPARE(theme.styleDescription("missing"), QString());

    // A hand-edited value that is too long is elided.
    const QString elided = theme.styleDescription("long");
    QCOMPARE(elided.size(), 48);
    QVERIFY(elided.endsWith(QChar(0x2026)));

    // `description` is a key of the file, not an unknown section.
    theme.setStyle("long");
    for (const QString &problem : theme.problems())
        QVERIFY2(!problem.contains("description"), qPrintable(problem));

    QVERIFY(theme.createStyle("boxy", "Sharp", "  Boxes   all\nround "));
    QCOMPARE(theme.styleDescription("boxy"), QStringLiteral("Boxes all round"));
    QVERIFY(read(dataDir + "/themes/styles/boxy.toml").contains("description = \"Boxes all round\""));
    QVERIFY(theme.createStyle("capped", "Soft", QString(60, QLatin1Char('y'))));
    QCOMPARE(theme.styleDescription("capped").size(), 48);
    QVERIFY(theme.createStyle("plain", "Soft", ""));
    QVERIFY(!read(dataDir + "/themes/styles/plain.toml").contains("description"));

    QVERIFY(theme.setStyleDescription("plain", "Changed"));
    QCOMPARE(theme.styleDescription("plain"), QStringLiteral("Changed"));
    QVERIFY(theme.setStyleDescription("plain", "Again"));
    QCOMPARE(theme.styleDescription("plain"), QStringLiteral("Again"));
    QVERIFY(theme.setStyleDescription("plain", ""));
    QVERIFY(!read(dataDir + "/themes/styles/plain.toml").contains("description"));
    QVERIFY(!theme.setStyleDescription("Soft", "Mine"));
    QVERIFY(!theme.setStyleDescription("missing", "Mine"));
}

// The line a style without a description gets in the theme popup.
void ThemeTest::styleSummaryListsWhatDiffers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/styles/plain.toml", "extends = \"Soft\"\n");
    write(dataDir + "/themes/styles/bare.toml",
          "extends = \"Soft\"\n[layout]\nborder_width = 0\n[rail]\ncontainer = false\n"
          "[launch_button]\nlabel = \"Play\"\n[motion]\nfast = 90\nnormal = 150\nslow = 400\n");
    write(dataDir + "/themes/styles/child.toml", "extends = \"bare\"\n[radius]\nlarge = 1\n");
    write(dataDir + "/themes/styles/one.toml", "extends = \"Soft\"\n[motion]\nfast = 90\n");

    Theme theme(dataDir);
    QCOMPARE(theme.styleSummary("plain"), QStringLiteral("Radius 12 · 8 · 6 · 4"));
    QCOMPARE(theme.styleSummary("bare"),
             QStringLiteral("Radius 12 · 8 · 6 · 4 · no borders · no rail panel · Play · +2 other changes"));
    QCOMPARE(theme.styleSummary("child"),
             QStringLiteral("Radius 1 · 8 · 6 · 4 · no borders · no rail panel · Play · +2 other changes"));
    QCOMPARE(theme.styleSummary("one"), QStringLiteral("Radius 12 · 8 · 6 · 4 · +1 other change"));
    QVERIFY(theme.styleSummary("Steam").contains("▶  PLAY"));
    QVERIFY(theme.styleSummary("missing").startsWith("Radius 12"));

    // Reading a style changes nothing of the active theme.
    QCOMPARE(theme.resolvedValue("radius", "large").toInt(), 12);
    QCOMPARE(theme.activeStyle(), QStringLiteral("Soft"));
}

// Every built-in pair loads, and nothing needs a font or a layout the other half doesn't give.
void ThemeTest::builtInsResolveWithoutProblems()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QCOMPARE(theme.activeColors(), QStringLiteral("Ember"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Soft"));
    QCOMPARE(theme.activeTheme(), QStringLiteral("Ember"));
    QCOMPARE(theme.availableThemes(), QStringList({"Ember", "Moss", "Mythos", "Steam", "Catppuccin", "Gruvbox", "Rose Pine", "Nord"}));
    QCOMPARE(theme.availableColors(), QStringList({"Ember", "Moss", "Mythos", "Steam", "Catppuccin", "Gruvbox", "Rose Pine", "Nord"}));
    QCOMPARE(theme.availableStyles(), QStringList({"Sharp", "Soft", "Round", "Steam"}));
    QCOMPARE(theme.builtInThemes(), theme.availableThemes());
    QVERIFY(theme.problems().isEmpty());

    for (const QString &name : theme.builtInThemes()) {
        theme.setTheme(name);
        QCOMPARE(theme.activeTheme(), name);
        QVERIFY2(theme.problems().isEmpty(), qPrintable(name + ": " + theme.problems().join('\n')));
    }
    for (const QString &style : theme.builtInStyles()) {
        theme.setStyle(style);
        QCOMPARE(theme.activeStyle(), style);
        QVERIFY2(theme.problems().isEmpty(), qPrintable(style + ": " + theme.problems().join('\n')));
    }
    for (const QString &colors : theme.builtInColors()) {
        theme.setColors(colors);
        QVERIFY2(theme.problems().isEmpty(), qPrintable(colors + ": " + theme.problems().join('\n')));
    }
}

void ThemeTest::builtInStylesDifferWhereDecided()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));
    const auto radii = [&] {
        return QList<int>{theme.resolvedValue("radius", "large").toInt(), theme.resolvedValue("radius", "medium").toInt(),
                          theme.resolvedValue("radius", "small").toInt(), theme.resolvedValue("radius", "tiny").toInt()};
    };

    QCOMPARE(radii(), (QList<int>{12, 8, 6, 4})); // Soft
    QCOMPARE(theme.resolvedValue("layout", "border_width").toInt(), 2);
    QCOMPARE(theme.resolvedValue("launch_button", "hover_brightness").toDouble(), 1.1);
    QCOMPARE(theme.resolvedValue("launch_button", "gradient_direction").toString(), QStringLiteral("down"));
    QCOMPARE(theme.resolvedValue("settings_window", "window_border_radius").toInt(), 12);
    QCOMPARE(theme.resolvedValue("font", "family").toString(), QStringLiteral("Inter"));
    QCOMPARE(theme.resolvedValue("list", "width").toInt(), 420);

    theme.setStyle("Round");
    QCOMPARE(radii(), (QList<int>{16, 12, 8, 4}));
    QCOMPARE(theme.resolvedValue("list", "width").toInt(), 420);

    theme.setStyle("Sharp");
    QCOMPARE(radii(), (QList<int>{0, 0, 0, 0}));
    QCOMPARE(theme.resolvedValue("hero", "radius").toInt(), 0);
    QCOMPARE(theme.resolvedValue("settings_window", "checkbox_radius").toInt(), 0);
    // The kept corners.
    QCOMPARE(theme.resolvedValue("tags", "radius").toInt(), 6);
    QCOMPARE(theme.resolvedValue("settings_window", "source_tag_radius").toInt(), 6);
    QCOMPARE(theme.resolvedValue("settings_window", "key_chip_radius").toInt(), 6);
    QCOMPARE(theme.resolvedValue("filters", "chip_radius").toInt(), 12);
    QCOMPARE(theme.resolvedValue("list", "icon_radius").toInt(), 4);
    QCOMPARE(theme.resolvedValue("list", "width").toInt(), 420);

    theme.setStyle("Steam");
    QCOMPARE(radii(), (QList<int>{4, 2, 0, 0}));
    QCOMPARE(theme.resolvedValue("launch_button", "label").toString(), QString::fromUtf8("▶  PLAY"));
    QCOMPARE(theme.resolvedValue("settings_window", "window_border_radius").toInt(), 4);
    QCOMPARE(theme.resolvedValue("settings_window", "window_border_width").toInt(), 0);
    QCOMPARE(theme.resolvedValue("rail", "border_width").toInt(), 2);
}

void ThemeTest::eachHalfLoadsItsOwnSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/settings.toml", "[appearance]\ncolors = \"Dusk\"\nstyle = \"Boxy\"\n");
    write(dataDir + "/themes/colors/Dusk.toml", "[palette]\naccent = \"#112233\"\nmoss = \"#223311\"\n");
    write(dataDir + "/themes/styles/Boxy.toml", "[radius]\nlarge = 3\n[layout]\ngradient_direction = \"right\"\n");

    Theme theme(dataDir);
    QVERIFY2(theme.problems().isEmpty(), qPrintable(theme.problems().join('\n')));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#112233"));
    auto *palette = theme.property("palette").value<QObject *>();
    QCOMPARE(palette->property("moss").value<QColor>(), QColor("#223311")); // a custom color
    QCOMPARE(theme.resolvedValue("radius", "large").toInt(), 3);
    QCOMPARE(theme.resolvedValue("layout", "gradient_direction").toString(), QStringLiteral("right"));
    QCOMPARE(theme.sourceLabel("palette", "accent"), QStringLiteral("Dusk.toml (colors)"));
    QCOMPARE(theme.sourceLabel("radius", "large"), QStringLiteral("Boxy.toml (style)"));
    QCOMPARE(theme.sourceLabel("radius", "medium"), QStringLiteral("default"));
    QCOMPARE(theme.activeFile("palette", "accent"), QStringLiteral("Dusk.toml"));
    QCOMPARE(theme.activeFile("radius", "large"), QStringLiteral("Boxy.toml"));
}

// A radius in a colors file (or a color in a style file) is reported, with the folder it
// belongs in, and not applied.
void ThemeTest::wrongHalfIsReportedAndIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/settings.toml", "[appearance]\ncolors = \"Dusk\"\nstyle = \"Boxy\"\n");
    write(dataDir + "/themes/colors/Dusk.toml", "[palette]\naccent = \"#112233\"\n[radius]\nlarge = 3\n");
    write(dataDir + "/themes/styles/Boxy.toml", "[palette]\nsurface = \"#202020\"\nmoss = \"#223311\"\n[radius]\nmedium = 5\n");

    Theme theme(dataDir);
    const QString problems = theme.problems().join('\n');
    QVERIFY2(problems.contains("radius.large belongs to a theme's style, not its colors; put it in themes/styles/"),
             qPrintable(problems));
    QVERIFY2(problems.contains("palette.surface belongs to a theme's colors, not its style; put it in themes/colors/"),
             qPrintable(problems));
    QVERIFY2(problems.contains("palette.moss belongs to a theme's colors"), qPrintable(problems));

    QCOMPARE(theme.resolvedValue("radius", "large").toInt(), 12); // the schema default
    QCOMPARE(theme.resolvedValue("palette", "surface").value<QColor>(), QColor("#101010"));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#112233"));
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 5);
    auto *palette = theme.property("palette").value<QObject *>();
    QVERIFY(!palette->property("moss").isValid());
}

// Built-ins aren't copied: the first edit writes the file that layers on the built-in of the same name.
void ThemeTest::firstEditOfABuiltInWritesItsLayer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    theme.setValue("palette", "accent", QColor("#00ff00"));
    QTest::qWait(700);

    QVERIFY(QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    QVERIFY(!QFile::exists(dataDir + "/themes/styles/Soft.toml"));
    QVERIFY(!QFile::exists(dataDir + "/themes/Ember.toml"));
    QCOMPARE(theme.sourceLabel("palette", "accent"), QStringLiteral("Ember.toml (colors)"));
    // The built-in's other values still apply under it.
    QCOMPARE(theme.resolvedValue("palette", "background").value<QColor>(), QColor("#0d1117"));
}

void ThemeTest::userFileLayersOnBuiltInOfTheSameName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Ember.toml", "[palette]\naccent = \"#abcdef\"\n");

    Theme theme(dataDir);
    QVERIFY2(theme.problems().isEmpty(), qPrintable(theme.problems().join('\n')));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#abcdef"));
    QCOMPARE(theme.resolvedValue("palette", "background").value<QColor>(), QColor("#0d1117"));
    QVERIFY(theme.isCustomized("palette", "accent"));
    QVERIFY(!theme.isCustomized("palette", "background"));

    // A file whose extends names itself layers the same way.
    write(dataDir + "/themes/colors/Ember.toml", "extends = \"Ember\"\n[palette]\ntext = \"#fefefe\"\n");
    QTRY_COMPARE(theme.resolvedValue("palette", "text").value<QColor>(), QColor("#fefefe"));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#e15f24"));
    QCOMPARE(theme.resolvedValue("palette", "background").value<QColor>(), QColor("#0d1117"));

    // Deleting the file goes back to the built-in.
    QVERIFY(QFile::remove(dataDir + "/themes/colors/Ember.toml"));
    QTRY_COMPARE(theme.resolvedValue("palette", "text").value<QColor>(), QColor("#e6edf3"));
    QVERIFY(!theme.isCustomized("palette", "text"));
    QVERIFY(theme.problems().isEmpty());
}

// Editing Soft must not change Sharp, which extends it; your own styles based on Soft follow the edit.
void ThemeTest::builtInsExtendTheOriginalBuiltIn()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/styles/Soft.toml", "[search]\nradius = 12\n");
    write(dataDir + "/themes/styles/Mine.toml", "extends = \"Soft\"\n");
    write(dataDir + "/settings.toml", "[appearance]\ncolors = \"Ember\"\nstyle = \"Sharp\"\n");

    Theme theme(dataDir);
    QVERIFY2(theme.problems().isEmpty(), qPrintable(theme.problems().join(QLatin1Char(10))));
    QCOMPARE(theme.resolvedValue("search", "radius").toInt(), 0);
    QCOMPARE(theme.preview("Ember", "Mine").value("searchRadius").toInt(), 12);
    QCOMPARE(theme.preview("Ember", "Soft").value("searchRadius").toInt(), 12);
    QCOMPARE(theme.preview("Ember", "Sharp").value("searchRadius").toInt(), 0);
}

void ThemeTest::activeThemeIsEmptyWhenThePairMatchesNone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/settings.toml", "[appearance]\ncolors = \"Mythos\"\nstyle = \"Sharp\"\n");

    Theme theme(dataDir);
    QCOMPARE(theme.activeColors(), QStringLiteral("Mythos"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Sharp"));
    QCOMPARE(theme.activeTheme(), QString());

    QSignalSpy changed(&theme, &Theme::activeThemeChanged);
    theme.setStyle("Soft");
    QCOMPARE(theme.activeTheme(), QStringLiteral("Mythos"));
    QCOMPARE(changed.count(), 1);

    // A theme of your own is found by its pair, not its name.
    write(dataDir + "/themes/Mine.toml", "colors = \"Mythos\"\nstyle = \"Round\"\n");
    theme.setStyle("Round");
    QCOMPARE(theme.activeTheme(), QStringLiteral("Mine"));
    QCOMPARE(theme.availableThemes().last(), QStringLiteral("Mine"));
}

void ThemeTest::missingFileFallsBackToBuiltIn()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    const QByteArray settings = "[appearance]\ncolors = \"Gone\"\nstyle = \"Nope\"\n";
    write(dataDir + "/settings.toml", settings);

    Theme theme(dataDir);
    const QString problems = theme.problems().join('\n');
    QVERIFY2(problems.contains("colors \"Gone\" not found"), qPrintable(problems));
    QVERIFY2(problems.contains("style \"Nope\" not found"), qPrintable(problems));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#e15f24")); // Ember
    QCOMPARE(theme.resolvedValue("radius", "large").toInt(), 12);                         // Soft
    QCOMPARE(theme.activeColors(), QStringLiteral("Gone"));
    QCOMPARE(theme.activeTheme(), QString());
    QCOMPARE(read(dataDir + "/settings.toml").toUtf8(), settings); // not rewritten

    // Putting the file back restores it.
    write(dataDir + "/themes/colors/Gone.toml", "[palette]\naccent = \"#123456\"\n");
    QTRY_COMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#123456"));
    QVERIFY(!theme.problems().join('\n').contains("colors \"Gone\""));
}

// A trash that moves the file into a folder of the test's, or fails with the given result.
static Uninstall::Trash fakeTrash(const QString &folder, bool works, QStringList *trashed = nullptr)
{
    return [folder, works, trashed](const QString &path, QString *error) {
        if (!works) {
            *error = QStringLiteral("no trash");
            return false;
        }
        QDir().mkpath(folder);
        if (trashed)
            trashed->append(QFileInfo(path).fileName());
        return QFile::rename(path, folder + u'/' + QFileInfo(path).fileName());
    };
}

void ThemeTest::deletingRefusesBuiltInsAndUnknownNames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Ember.toml", "[palette]\naccent = \"#123456\"\n");

    QStringList trashed;
    Theme theme(dataDir, fakeTrash(dir.path() + "/trash", true, &trashed));
    for (const QString &name : {QStringLiteral("Ember"), QStringLiteral("Nope"), QStringLiteral("reference"),
                                QStringLiteral("../x"), QString()}) {
        QVERIFY2(!theme.deleteColors(name), qPrintable(name));
    }
    QVERIFY(!theme.deleteStyle("Soft"));
    QVERIFY(!theme.deleteStyle("Nope"));
    QVERIFY(!theme.deleteTheme("Ember"));
    QVERIFY(!theme.deleteTheme("Nope"));
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    QVERIFY(trashed.isEmpty());
}

void ThemeTest::usedByListsThemesAndExtendingFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Night.toml", "extends = \"Ember\"\n");
    write(dataDir + "/themes/colors/Dusk.toml", "extends = \"Night\"\n");
    write(dataDir + "/themes/styles/Flat.toml", "extends = \"Soft\"\n");
    write(dataDir + "/themes/Late.toml", "colors = \"Night\"\nstyle = \"Flat\"\n");
    write(dataDir + "/themes/Early.toml", "colors = \"Night\"\nstyle = \"Soft\"\n");

    Theme theme(dataDir);
    QVariantMap night = theme.usedBy("colors", "Night");
    QCOMPARE(night["themes"].toStringList(), (QStringList{"Early", "Late"}));
    QCOMPARE(night["files"].toStringList(), QStringList{"Dusk"});
    QCOMPARE(theme.usedBy("style", "Flat")["themes"].toStringList(), QStringList{"Late"});
    QVERIFY(theme.usedBy("style", "Flat")["files"].toStringList().isEmpty());
    QVERIFY(theme.usedBy("theme", "Late").isEmpty());
    QVERIFY(theme.usedBy("colors", "Dusk")["themes"].toStringList().isEmpty());
    // A built-in is used by its pair and by the files layered on it.
    QVERIFY(theme.usedBy("colors", "Ember")["themes"].toStringList().contains("Ember"));
    QVERIFY(theme.usedBy("colors", "Ember")["files"].toStringList().contains("Night"));
}

void ThemeTest::deletingTheActiveColorsSwitchesToEmber()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Night.toml", "extends = \"Ember\"\n[palette]\naccent = \"#123456\"\n");
    write(dataDir + "/themes/Late.toml", "colors = \"Night\"\nstyle = \"Sharp\"\n");

    QStringList trashed;
    Theme theme(dataDir, fakeTrash(dir.path() + "/trash", true, &trashed));
    theme.setColors("Night");
    theme.setStyle("Sharp");
    QCOMPARE(theme.activeTheme(), QStringLiteral("Late"));
    theme.beginEditSession();
    theme.setValue("palette", "accent", QColor("#0000ff"));
    theme.setValue("radius", "large", 3);

    QVERIFY(theme.deleteColors("Night"));
    QCOMPARE(trashed, QStringList{"Night.toml"});
    QVERIFY(QFile::exists(dir.path() + "/trash/Night.toml"));
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Night.toml"));
    QCOMPARE(theme.activeColors(), QStringLiteral("Ember"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Sharp"));
    QVERIFY(!theme.availableColors().contains("Night"));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#e15f24"));
    QVERIFY(read(dataDir + "/settings.toml").contains("colors = \"Ember\""));
    QVERIFY(read(dataDir + "/settings.toml").contains("style = \"Sharp\""));
    // The unsaved color edit went with the file; switching colors starts a new session, and the style edit is saved.
    QTest::qWait(700);
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    QCOMPARE(theme.sessionChanges().size(), 0);
    QVERIFY(read(dataDir + "/themes/styles/Sharp.toml").contains("large = 3"));
    // The theme that named it stays, falling back with a problem.
    QVERIFY(theme.availableThemes().contains("Late"));

    // A style that isn't active goes without touching the settings.
    write(dataDir + "/themes/styles/Flat.toml", "extends = \"Soft\"\n");
    QTRY_VERIFY(theme.availableStyles().contains("Flat"));
    QVERIFY(theme.deleteStyle("Flat"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Sharp"));
    QVERIFY(!theme.availableStyles().contains("Flat"));

    // Deleting the active style falls back to Soft.
    write(dataDir + "/themes/styles/Boxy.toml", "extends = \"Sharp\"\n");
    theme.setStyle("Boxy");
    QVERIFY(theme.deleteStyle("Boxy"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Soft"));
}

void ThemeTest::renamingColorsRepointsWhatNamesThem()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Night.toml", "extends = \"Ember\"\n[palette]\naccent = \"#123456\"\n");
    write(dataDir + "/themes/colors/Dusk.toml", "extends = \"Night\"\n");
    write(dataDir + "/themes/Late.toml", "colors = \"Night\"\nstyle = \"Sharp\"\n");

    Theme theme(dataDir, fakeTrash(dir.path() + "/trash", true));
    theme.setColors("Night");
    theme.setStyle("Sharp");
    QCOMPARE(theme.activeTheme(), QStringLiteral("Late"));

    QVERIFY(theme.renameColors("Night", "Fern"));
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Night.toml"));
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Fern.toml"));
    QCOMPARE(theme.activeColors(), QStringLiteral("Fern"));
    QCOMPARE(theme.activeTheme(), QStringLiteral("Late"));
    QVERIFY(read(dataDir + "/settings.toml").contains("colors = \"Fern\""));
    QVERIFY(read(dataDir + "/themes/Late.toml").contains("colors = \"Fern\""));
    QVERIFY(read(dataDir + "/themes/colors/Dusk.toml").contains("extends = \"Fern\""));
    QVERIFY(theme.availableColors().contains("Fern"));
    QVERIFY(!theme.availableColors().contains("Night"));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#123456"));

    QVERIFY(theme.renameTheme("Late", "Early"));
    QVERIFY(QFile::exists(dataDir + "/themes/Early.toml"));
    QVERIFY(!QFile::exists(dataDir + "/themes/Late.toml"));
    QCOMPARE(theme.activeTheme(), QStringLiteral("Early"));
}

void ThemeTest::renamingRefusesBuiltInsAndTakenNames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Night.toml", "extends = \"Ember\"\n");
    write(dataDir + "/themes/colors/Dusk.toml", "extends = \"Ember\"\n");
    write(dataDir + "/themes/colors/Ember.toml", "[palette]\naccent = \"#123456\"\n");

    Theme theme(dataDir, fakeTrash(dir.path() + "/trash", true));
    QVERIFY(!theme.renameColors("Ember", "Fire"));
    QVERIFY(!theme.renameColors("Night", "Dusk"));
    QVERIFY(!theme.renameColors("Night", "Mythos"));
    QVERIFY(!theme.renameColors("Night", "reference"));
    QVERIFY(!theme.renameColors("Night", "a/b"));
    QVERIFY(!theme.renameColors("Night", QString()));
    QVERIFY(!theme.renameColors("Nope", "Other"));
    QVERIFY(!theme.renameStyle("Soft", "Mushy"));
    QVERIFY(!theme.renameTheme("Ember", "Fire"));
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Night.toml"));
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Ember.toml"));
}

void ThemeTest::deletingAThemeLeavesItsHalves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Night.toml", "extends = \"Ember\"\n");
    write(dataDir + "/themes/styles/Flat.toml", "extends = \"Soft\"\n");
    write(dataDir + "/themes/Late.toml", "colors = \"Night\"\nstyle = \"Flat\"\n");

    Theme theme(dataDir, fakeTrash(dir.path() + "/trash", true));
    theme.setTheme("Late");
    QCOMPARE(theme.activeTheme(), QStringLiteral("Late"));
    QVERIFY(theme.deleteTheme("Late"));
    QVERIFY(QFile::exists(dir.path() + "/trash/Late.toml"));
    QVERIFY(!theme.availableThemes().contains("Late"));
    QCOMPARE(theme.activeTheme(), QString());
    QCOMPARE(theme.activeColors(), QStringLiteral("Night"));
    QCOMPARE(theme.activeStyle(), QStringLiteral("Flat"));
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Night.toml"));
    QVERIFY(QFile::exists(dataDir + "/themes/styles/Flat.toml"));
}

void ThemeTest::aFailingTrashKeepsTheFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/colors/Night.toml", "extends = \"Ember\"\n");

    Theme theme(dataDir, fakeTrash({}, false));
    theme.setColors("Night");
    QVERIFY(!theme.deleteColors("Night"));
    QVERIFY(theme.needsPermanent());
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Night.toml"));
    QCOMPARE(theme.activeColors(), QStringLiteral("Night"));
    QVERIFY(read(dataDir + "/settings.toml").contains("colors = \"Night\""));

    // Asked again, it deletes for good.
    QVERIFY(theme.deleteColors("Night", true));
    QVERIFY(!theme.needsPermanent());
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Night.toml"));
    QCOMPARE(theme.activeColors(), QStringLiteral("Ember"));
}

void ThemeTest::newFilesReload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QVERIFY(!theme.availableColors().contains("Neon"));
    write(dataDir + "/themes/colors/Neon.toml", "[palette]\naccent = \"#00ffaa\"\n");
    write(dataDir + "/themes/styles/Flat.toml", "[radius]\nlarge = 1\n");
    QTRY_VERIFY(theme.availableColors().contains("Neon"));
    QTRY_VERIFY(theme.availableStyles().contains("Flat"));
    QCOMPARE(theme.availableColors().last(), QStringLiteral("Neon"));

    // Editing the active file by hand applies at once.
    theme.setColors("Neon");
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#00ffaa"));
    write(dataDir + "/themes/colors/Neon.toml", "[palette]\naccent = \"#ffaa00\"\n");
    QTRY_COMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#ffaa00"));
}

void ThemeTest::referenceFilesAreWritten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    const QString colors = read(dataDir + "/themes/colors/reference.toml");
    const QString style = read(dataDir + "/themes/styles/reference.toml");
    QVERIFY(colors.contains("[palette]"));
    QVERIFY(colors.contains("accent = "));
    QVERIFY(!colors.contains("[radius]"));
    QVERIFY(style.contains("[radius]"));
    QVERIFY(style.contains("gradient_direction = "));
    QVERIFY(!style.contains("accent = "));
    QVERIFY(!QFile::exists(dataDir + "/themes/reference.toml"));
    QVERIFY(theme.availableColors().filter("reference").isEmpty());
    QVERIFY(theme.availableStyles().filter("reference").isEmpty());
}

// Every _end color in the defaults resolves to the same value as its start.
void ThemeTest::gradientEndDefaultsMatchStart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);

    for (const ThemeSchema::Setting &s : ThemeSchema::settings()) {
        if (s.type != ThemeSchema::Type::Color)
            continue;
        const QString key = QString::fromUtf8(s.key.data(), qsizetype(s.key.size()));
        if (!key.endsWith(QStringLiteral("_end")))
            continue;
        const QString section = QString::fromUtf8(s.section.data(), qsizetype(s.section.size()));
        const QString startKey = key.chopped(4);
        const QColor end = theme.resolvedValue(section, key).value<QColor>();
        const QColor start = theme.resolvedValue(section, startKey).value<QColor>();
        if (end != start)
            QFAIL(qPrintable(QStringLiteral("%1.%2 = %3 != %1.%4 = %5")
                                 .arg(section, key, end.name(), startKey, start.name())));
    }
}

// Changing a start color makes its _end follow automatically.
void ThemeTest::gradientEndFollowsStartOverride()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    theme.setValue("rail", "background", QColor("#ff0000"));

    QCOMPARE(theme.resolvedValue("rail", "background").value<QColor>(), QColor("#ff0000"));
    QCOMPARE(theme.resolvedValue("rail", "background_end").value<QColor>(), QColor("#ff0000"));
}

// layout.gradient_direction = "right" propagates to every section; a section can override.
void ThemeTest::layoutDirectionPropagates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);
    theme.setValue("layout", "gradient_direction", QStringLiteral("right"));

    for (const ThemeSchema::Setting &s : ThemeSchema::settings()) {
        if (s.type != ThemeSchema::Type::Enum)
            continue;
        const QString key = QString::fromUtf8(s.key.data(), qsizetype(s.key.size()));
        if (key != QStringLiteral("gradient_direction"))
            continue;
        const QString section = QString::fromUtf8(s.section.data(), qsizetype(s.section.size()));
        if (section == QStringLiteral("layout"))
            continue;
        QCOMPARE(theme.resolvedValue(section, QStringLiteral("gradient_direction")).toString(),
                 QStringLiteral("right"));
    }

    // A section can still override its own direction.
    theme.setValue("rail", "gradient_direction", QStringLiteral("left"));
    QCOMPARE(theme.resolvedValue("rail", "gradient_direction").toString(), QStringLiteral("left"));
    QCOMPARE(theme.resolvedValue("search", "gradient_direction").toString(), QStringLiteral("right"));
}

// An unknown gradient_direction value produces a warning and falls back to "down".
void ThemeTest::unknownDirectionFallsBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);
    write(dataDir + "/themes/styles/Plain.toml", "[rail]\ngradient_direction = \"diagonal\"\n");

    Theme theme(dataDir);
    QVERIFY(!theme.problems().isEmpty());
    QCOMPARE(theme.resolvedValue("rail", "gradient_direction").toString(), QStringLiteral("down"));
}

// Every _end key's default is either @section.start (self-reference) or a reference to
// another existing _end key (palette cascade). The start key must exist in the schema.
void ThemeTest::gradientEndSchemaIntegrity()
{
    QSet<QString> knownKeys;
    for (const ThemeSchema::Setting &s : ThemeSchema::settings()) {
        const QString section = QString::fromUtf8(s.section.data(), qsizetype(s.section.size()));
        const QString key = QString::fromUtf8(s.key.data(), qsizetype(s.key.size()));
        knownKeys.insert(section + u'.' + key);
    }

    for (const ThemeSchema::Setting &s : ThemeSchema::settings()) {
        if (s.type != ThemeSchema::Type::Color)
            continue;
        const QString key = QString::fromUtf8(s.key.data(), qsizetype(s.key.size()));
        if (!key.endsWith(QStringLiteral("_end")))
            continue;
        const QString section = QString::fromUtf8(s.section.data(), qsizetype(s.section.size()));
        const QString startKey = key.chopped(4);
        const QString selfDefault = u'@' + section + u'.' + startKey;
        const QString actualDefault = QString::fromUtf8(s.def.data(), qsizetype(s.def.size()));

        if (actualDefault != selfDefault) {
            // Also valid: a reference to an existing _end key (cascade via palette or another section).
            const QString refKey = actualDefault.startsWith(u'@') ? actualDefault.mid(1) : QString();
            if (refKey.isEmpty() || !refKey.endsWith(QStringLiteral("_end")) || !knownKeys.contains(refKey))
                QFAIL(qPrintable(QStringLiteral("%1.%2: expected \"%3\" or a reference to an existing _end key, got \"%4\"")
                                     .arg(section, key, selfDefault, actualDefault)));
        }
        if (!knownKeys.contains(section + u'.' + startKey))
            QFAIL(qPrintable(QStringLiteral("%1.%2: start %1.%3 not found in schema")
                                 .arg(section, key, startKey)));
    }
}

// Setting a palette _end cascades to all surfaces that reference that palette color.
void ThemeTest::paletteEndCascades()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);

    theme.setValue("palette", "surface_end", QColor("#aabbcc"));
    QCOMPARE(theme.resolvedValue("filters", "background_end").value<QColor>(), QColor("#aabbcc"));
    QCOMPARE(theme.resolvedValue("info_panel", "background_end").value<QColor>(), QColor("#aabbcc"));
    QCOMPARE(theme.resolvedValue("toast", "background_end").value<QColor>(), QColor("#aabbcc"));
    QCOMPARE(theme.resolvedValue("settings_window", "card_background_end").value<QColor>(), QColor("#aabbcc"));
    QCOMPARE(theme.resolvedValue("game_editor", "card_background_end").value<QColor>(), QColor("#aabbcc"));

    theme.setValue("palette", "accent_end", QColor("#ff4400"));
    QCOMPARE(theme.resolvedValue("launch_button", "background_end").value<QColor>(), QColor("#ff4400"));
    QCOMPARE(theme.resolvedValue("rail", "selected_background_end").value<QColor>(), QColor("#ff4400"));
    QCOMPARE(theme.resolvedValue("filters", "chip_background_end").value<QColor>(), QColor("#ff4400"));
    QCOMPARE(theme.resolvedValue("settings_window", "button_primary_background_end").value<QColor>(), QColor("#ff4400"));
    QCOMPARE(theme.resolvedValue("settings_window", "sidebar_selected_background_end").value<QColor>(), QColor("#ff4400"));

    theme.setValue("palette", "surface_raised_end", QColor("#334455"));
    QCOMPARE(theme.resolvedValue("launch_button", "uninstalled_background_end").value<QColor>(), QColor("#334455"));
    QCOMPARE(theme.resolvedValue("settings_window", "button_background_end").value<QColor>(), QColor("#334455"));

    theme.setValue("palette", "surface_hover_end", QColor("#223344"));
    QCOMPARE(theme.resolvedValue("icon_button", "hover_background_end").value<QColor>(), QColor("#223344"));
    QCOMPARE(theme.resolvedValue("settings_window", "button_hover_background_end").value<QColor>(), QColor("#223344"));
}

void ThemeTest::onboardingNewKeysResolveInDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);

    QCOMPARE(theme.resolvedValue("onboarding", "background").value<QColor>(), QColor("#0a0605"));
    QCOMPARE(theme.resolvedValue("onboarding", "card_background").value<QColor>(),
             theme.resolvedValue("palette", "surface").value<QColor>());
    QCOMPARE(theme.resolvedValue("onboarding", "card_shadow_color").value<QColor>(), QColor(0x00, 0x00, 0x00, 0xa6));
    QCOMPARE(theme.resolvedValue("onboarding", "panel_background").value<QColor>(), QColor(0xff, 0xff, 0xff, 0x08));
    QCOMPARE(theme.resolvedValue("onboarding", "tile_background").value<QColor>(), QColor(0xff, 0xff, 0xff, 0x08));
}

void ThemeTest::cardBackgroundFollowsSurface()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);

    theme.setValue("palette", "surface", QColor("#202020"));
    QCOMPARE(theme.resolvedValue("onboarding", "card_background").value<QColor>(), QColor("#202020"));
    QCOMPARE(theme.resolvedValue("onboarding", "card_background_end").value<QColor>(), QColor("#202020"));
}

void ThemeTest::motionDefaultsAndRanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    {
        Theme theme(dataDir);
        QVERIFY(theme.problems().isEmpty());
        QCOMPARE(theme.resolvedValue("motion", "duration_scale").toDouble(), 1.0);
        QCOMPARE(theme.resolvedValue("motion", "fast").toInt(), 120);
        QCOMPARE(theme.resolvedValue("motion", "normal").toInt(), 220);
        QCOMPARE(theme.resolvedValue("motion", "slow").toInt(), 400);
        QCOMPARE(theme.resolvedValue("motion", "easing").toString(), QStringLiteral("snappy"));
        QCOMPARE(theme.resolvedValue("motion", "overshoot").toDouble(), 1.0);
        QCOMPARE(theme.resolvedValue("motion", "spring").toDouble(), 6.0);
        QCOMPARE(theme.resolvedValue("motion", "damping").toDouble(), 0.5);
        QCOMPARE(theme.resolvedValue("motion", "popup_scale").toDouble(), 0.95);
        QCOMPARE(theme.resolvedValue("motion", "press_scale").toDouble(), 0.96);
        QCOMPARE(theme.resolvedValue("motion", "chip_scale").toDouble(), 0.8);
    }

    // Motion is a style setting, so it can't be in a colors file.
    for (const ThemeSchema::Setting &s : ThemeSchema::settings()) {
        if (s.section == "motion")
            QVERIFY2(!ThemeSchema::isColorSetting(s), std::string(s.key).c_str());
    }

    // Out of range or unknown values report a problem and keep the default.
    write(dataDir + "/themes/styles/Plain.toml",
          "[motion]\nduration_scale = 9\ndamping = 0\neasing = \"bouncy\"\nfast = 80\n");
    Theme theme(dataDir);
    QCOMPARE(theme.problems().size(), 3);
    QCOMPARE(theme.resolvedValue("motion", "duration_scale").toDouble(), 1.0);
    QCOMPARE(theme.resolvedValue("motion", "damping").toDouble(), 0.5);
    QCOMPARE(theme.resolvedValue("motion", "easing").toString(), QStringLiteral("snappy"));
    QCOMPARE(theme.resolvedValue("motion", "fast").toInt(), 80);

    // The per-element durations that follow a speed change with it; one that is set stays.
    QCOMPARE(theme.resolvedValue("backdrop", "fade_duration").toInt(), 400);
    QCOMPARE(theme.resolvedValue("hero", "fade_duration").toInt(), 400);
    QCOMPARE(theme.resolvedValue("download_bar", "collapse_duration").toInt(), 400);
    theme.setValue("motion", "slow", 600);
    theme.setValue("motion", "normal", 300);
    theme.setValue("download_bar", "collapse_duration", 50);
    QCOMPARE(theme.resolvedValue("backdrop", "fade_duration").toInt(), 600);
    QCOMPARE(theme.resolvedValue("hero", "fade_duration").toInt(), 600);
    QCOMPARE(theme.resolvedValue("download_bar", "collapse_duration").toInt(), 50);
}

void ThemeTest::downloadBarResolvesInDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);
    QVERIFY(theme.resolvedValue("download_bar", "show").toBool());
    QCOMPARE(theme.resolvedValue("download_bar", "height").toInt(), 56);
    QCOMPARE(theme.resolvedValue("download_bar", "fill_duration").toInt(), 400);
    QCOMPARE(theme.resolvedValue("download_bar", "chevron_size").toInt(), 16);
    QCOMPARE(theme.resolvedValue("download_bar", "collapse_duration").toInt(), 400);
    QCOMPARE(theme.resolvedValue("download_bar", "pill_height").toInt(), 32);
    QCOMPARE(theme.resolvedValue("download_bar", "pill_icon_size").toInt(), 16);
    QCOMPARE(theme.resolvedValue("download_bar", "pill_radius").toInt(), theme.resolvedValue("download_bar", "radius").toInt());
    QCOMPARE(theme.resolvedValue("download_bar", "chevron_hover").value<QColor>(),
             theme.resolvedValue("download_bar", "title").value<QColor>());
    QCOMPARE(theme.resolvedValue("download_bar", "radius").toInt(), theme.resolvedValue("radius", "medium").toInt());
    QCOMPARE(theme.resolvedValue("download_bar", "track_radius").toInt(), theme.resolvedValue("radius", "tiny").toInt());
    QCOMPARE(theme.resolvedValue("download_bar", "background_end").value<QColor>(),
             theme.resolvedValue("download_bar", "background").value<QColor>());

    theme.setValue("launch_button", "install_background", QColor("#ff0000"));
    QCOMPARE(theme.resolvedValue("download_bar", "fill").value<QColor>(), QColor("#ff0000"));
}

void ThemeTest::radiusTiersDriveMembers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);
    write(dataDir + "/themes/styles/Plain.toml", "[radius]\nlarge = 20\n");

    Theme theme(dataDir);
    QCOMPARE(theme.resolvedValue("hero", "radius").toInt(), 20);
    QCOMPARE(theme.resolvedValue("info_panel", "radius").toInt(), 20);
    QCOMPARE(theme.resolvedValue("filters", "radius").toInt(), 20);
    QCOMPARE(theme.resolvedValue("rail", "container_radius").toInt(), 20);
    QCOMPARE(theme.resolvedValue("settings_window", "card_radius").toInt(), 20);
    QCOMPARE(theme.resolvedValue("onboarding", "card_radius").toInt(), 20);
    QVERIFY(theme.problems().isEmpty());
}

void ThemeTest::perElementRadiusWinsOverTier()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);
    write(dataDir + "/themes/styles/Plain.toml", "[radius]\nlarge = 20\n[hero]\nradius = 5\n");

    Theme theme(dataDir);
    QCOMPARE(theme.resolvedValue("radius", "large").toInt(), 20);
    QCOMPARE(theme.resolvedValue("hero", "radius").toInt(), 5);
    QCOMPARE(theme.resolvedValue("info_panel", "radius").toInt(), 20);
}

void ThemeTest::radiusTierLinksHold()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    usePlainDefaults(dataDir);

    Theme theme(dataDir);
    // game_editor.card_radius → settings_window.card_radius → radius.large (12)
    QCOMPARE(theme.resolvedValue("settings_window", "card_radius").toInt(), 12);
    QCOMPARE(theme.resolvedValue("game_editor", "card_radius").toInt(), 12);

    // sidebar_container_radius → rail.container_radius → radius.large
    QCOMPARE(theme.resolvedValue("settings_window", "sidebar_container_radius").toInt(), 12);
    QVERIFY(theme.problems().isEmpty());
}

// The old layout's files (themes/<name>.toml with settings in them) are still rewritten in
// place for the new radius keys; the split into colors and a style reads the result.
void ThemeTest::migrateLayoutRadiusToMedium()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/Ember.toml", "extends = \"ember\"\n[layout]\nradius = 15\n");

    Theme theme(dataDir);

    const QString text = read(dataDir + "/themes/Ember.toml");
    QVERIFY2(text.contains("medium = 15"), qPrintable(text));
    QVERIFY2(!text.contains("radius = 15"), qPrintable(text));
}

void ThemeTest::migrateLayoutRadiusSmallToSmall()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/Ember.toml", "extends = \"ember\"\n[layout]\nradius_small = 3\n");

    Theme theme(dataDir);

    const QString text = read(dataDir + "/themes/Ember.toml");
    QVERIFY2(text.contains("small = 3"), qPrintable(text));
    QVERIFY2(!text.contains("radius_small"), qPrintable(text));
}

void ThemeTest::migrateReferenceValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    // A user-created custom setting that happened to reference the old keys.
    write(dataDir + "/themes/Ember.toml",
          "extends = \"ember\"\n[hero]\nradius = \"@layout.radius\"\n[notice]\nradius = \"@layout.radius_small\"\n");

    Theme theme(dataDir);

    const QString text = read(dataDir + "/themes/Ember.toml");
    QVERIFY2(text.contains("\"@radius.medium\""), qPrintable(text));
    QVERIFY2(text.contains("\"@radius.small\""), qPrintable(text));
    QVERIFY2(!text.contains("@layout."), qPrintable(text));
}

void ThemeTest::migrationIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/Ember.toml", "extends = \"ember\"\n[layout]\nradius = 10\n");

    Theme first(dataDir);
    Q_UNUSED(first)
    const QString text1 = read(dataDir + "/themes/Ember.toml");

    // Second construction runs the migration again; file must be unchanged.
    Theme second(dataDir);
    Q_UNUSED(second)
    QCOMPARE(read(dataDir + "/themes/Ember.toml"), text1);
}

void ThemeTest::migrationSkipsExistingNewKey()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);
    write(dataDir + "/themes/Ember.toml", "extends = \"ember\"\n[radius]\nmedium = 14\n[layout]\nradius = 20\n");

    Theme theme(dataDir);

    const QString text = read(dataDir + "/themes/Ember.toml");
    QVERIFY2(text.contains("medium = 14"), qPrintable(text));
    QVERIFY2(!text.contains("radius = 20"), qPrintable(text));
}

void ThemeTest::schemaModelRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));
    auto *model = qobject_cast<ThemeSettingsModel *>(theme.styleSchema());
    const auto rowOf = [&](const QString &section, const QString &key) {
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex i = model->index(row);
            if (i.data(ThemeSettingsModel::SectionRole) == section && i.data(ThemeSettingsModel::KeyRole) == key)
                return i;
        }
        return QModelIndex();
    };

    const QModelIndex hue = rowOf("hero", "placeholder_hue_shift");
    QVERIFY(hue.isValid());
    QCOMPARE(hue.data(ThemeSettingsModel::SectionTitleRole).toString(), QStringLiteral("Hero"));
    QVERIFY(hue.data(ThemeSettingsModel::SectionDocRole).toString().startsWith(QStringLiteral("The large banner")));
    QCOMPARE(hue.data(ThemeSettingsModel::TitleRole).toString(), QStringLiteral("Placeholder hue shift"));
    QCOMPARE(hue.data(ThemeSettingsModel::CamelKeyRole).toString(), QStringLiteral("placeholderHueShift"));
    QCOMPARE(hue.data(ThemeSettingsModel::TypeRole).toString(), QStringLiteral("Real"));
    QCOMPARE(hue.data(ThemeSettingsModel::MinRole).toDouble(), 0.0);
    QCOMPARE(hue.data(ThemeSettingsModel::MaxRole).toDouble(), 1.0);
    QCOMPARE(hue.data(ThemeSettingsModel::DefaultRole).toString(), QStringLiteral("0.08"));
    QCOMPARE(hue.data(ThemeSettingsModel::ValueRole).toDouble(), 0.08);
    QCOMPARE(hue.data(ThemeSettingsModel::SourceLabelRole).toString(), QStringLiteral("default"));
    QVERIFY(!hue.data(ThemeSettingsModel::IsCustomRole).toBool());

    const QModelIndex direction = rowOf("layout", "gradient_direction");
    QCOMPARE(direction.data(ThemeSettingsModel::ChoicesRole).toStringList(),
             (QStringList{"down", "up", "right", "left"}));
    QCOMPARE(direction.data(ThemeSettingsModel::DefaultRole).toString(), QStringLiteral("\"down\""));
    QCOMPARE(rowOf("details", "stats").data(ThemeSettingsModel::DefaultRole).toString(),
             QStringLiteral("[\"play_time\", \"last_played\"]"));
    QCOMPARE(rowOf("hero", "radius").data(ThemeSettingsModel::DefaultRole).toString(), QStringLiteral("@radius.large"));

    // Set in the active style, then inherited by a theme that extends it.
    theme.setValue("hero", "placeholder_hue_shift", 0.2);
    QCOMPARE(hue.data(ThemeSettingsModel::SourceLabelRole).toString(), QStringLiteral("Soft.toml (style)"));
    QVERIFY(hue.data(ThemeSettingsModel::IsCustomRole).toBool());
    QCOMPARE(hue.data(ThemeSettingsModel::RawRole).toDouble(), 0.2);
    QTest::qWait(700); // the debounced save, which the next theme reads
    QVERIFY(theme.createColors("Child", "Ember"));
    QVERIFY(theme.createStyle("Child", "Soft"));
    const QModelIndex inherited = rowOf("hero", "placeholder_hue_shift");
    QCOMPARE(inherited.data(ThemeSettingsModel::SourceLabelRole).toString(), QStringLiteral("inherited from Soft"));
    QVERIFY(!inherited.data(ThemeSettingsModel::IsCustomRole).toBool());
}

void ThemeTest::schemaModelFilter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));
    auto *model = qobject_cast<ThemeSettingsModel *>(theme.styleSchema());
    auto *colorsModel = qobject_cast<ThemeSettingsModel *>(theme.colorsSchema());
    QSignalSpy sectionsChanged(model, &ThemeSettingsModel::sectionsChanged);
    const auto keys = [](ThemeSettingsModel *m) {
        QStringList out;
        for (int row = 0; row < m->rowCount(); ++row) {
            const QModelIndex i = m->index(row);
            out.append(i.data(ThemeSettingsModel::SectionRole).toString() + u'.'
                       + i.data(ThemeSettingsModel::KeyRole).toString());
        }
        return out;
    };

    // By key, any case.
    model->setFilterText("PLACEHOLDER_HUE");
    QCOMPARE(keys(model), QStringList{"hero.placeholder_hue_shift"});
    QCOMPARE(sectionsChanged.size(), 1);
    model->setFilterText("placeholder hue");
    QCOMPARE(keys(model), QStringList{"hero.placeholder_hue_shift"});
    model->setFilterText("PLACEHOLDER_HUE");
    QCOMPARE(model->sections().size(), 1);
    QCOMPARE(model->firstRow("hero"), 0);
    QCOMPARE(model->firstRow("list"), -1);
    QVERIFY(model->index(0).data(ThemeSettingsModel::FirstInSectionRole).toBool());
    QVERIFY(model->index(0).data(ThemeSettingsModel::LastInSectionRole).toBool());
    // The other half's model has its own filter, and doesn't list that setting at all.
    QCOMPARE(colorsModel->filterText(), QString());
    colorsModel->setFilterText("PLACEHOLDER_HUE");
    QCOMPARE(colorsModel->rowCount(), 0);
    colorsModel->setFilterText("");

    // By the description, and a color by its reference or the color in use.
    colorsModel->setFilterText("the letter on the placeholder");
    QCOMPARE(keys(colorsModel), QStringList{"list.icon_placeholder_text"});
    theme.setValue("hero", "resize_frame", QStringLiteral("@palette.accent_end"));
    colorsModel->setFilterText("@palette.accent_end");
    QCOMPARE(keys(colorsModel), QStringList{"hero.resize_frame"});
    theme.setValue("palette", "accent", QColor("#123456"));
    colorsModel->setFilterText("#123456");
    QVERIFY(keys(colorsModel).contains("palette.accent"));
    theme.setValue("palette", "surface", QStringLiteral("#10101080"));
    colorsModel->setFilterText("#10101080");
    QVERIFY(keys(colorsModel).contains("palette.surface"));

    colorsModel->setFilterText("no such setting");
    QCOMPARE(colorsModel->rowCount(), 0);
    QVERIFY(colorsModel->sections().isEmpty());
    QCOMPARE(colorsModel->data(colorsModel->index(0), ThemeSettingsModel::KeyRole), QVariant());
}

// Each half's list has only its own settings and sections, and nothing is on both.
void ThemeTest::halfModelsListOnlyTheirOwnSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));
    auto *colors = qobject_cast<ThemeSettingsModel *>(theme.colorsSchema());
    auto *style = qobject_cast<ThemeSettingsModel *>(theme.styleSchema());

    // The tag colors have their own editor.
    QCOMPARE(colors->rowCount() + style->rowCount(), int(ThemeSchema::settings().size()) - 1);

    QSet<QString> colorKeys;
    for (int row = 0; row < colors->rowCount(); ++row) {
        const QModelIndex i = colors->index(row);
        const QString full = i.data(ThemeSettingsModel::SectionRole).toString() + u'.' + i.data(ThemeSettingsModel::KeyRole).toString();
        const auto type = i.data(ThemeSettingsModel::TypeRole).toString();
        QVERIFY2(type == "Color" || type == "ColorList", qPrintable(full + " is a " + type));
        colorKeys.insert(full);
    }
    for (int row = 0; row < style->rowCount(); ++row) {
        const QModelIndex i = style->index(row);
        const QString full = i.data(ThemeSettingsModel::SectionRole).toString() + u'.' + i.data(ThemeSettingsModel::KeyRole).toString();
        const auto type = i.data(ThemeSettingsModel::TypeRole).toString();
        QVERIFY2(type != "Color" && type != "ColorList", qPrintable(full + " is a " + type));
        QVERIFY(!colorKeys.contains(full));
    }

    const auto sectionNames = [](ThemeSettingsModel *m) {
        QStringList out;
        for (const QVariant &s : m->sections())
            out.append(s.toMap().value("name").toString());
        return out;
    };
    QVERIFY(sectionNames(colors).contains("palette"));
    QVERIFY(sectionNames(style).contains("radius"));
    QVERIFY(!sectionNames(colors).contains("radius"));
    // Sections with settings of both kinds are in both lists, each with its own rows.
    QVERIFY(sectionNames(colors).contains("hero") && sectionNames(style).contains("hero"));

    // Link targets: a section jumps to its first row in that half's list.
    QCOMPARE(colors->firstRow("radius"), -1);
    QVERIFY(style->firstRow("radius") >= 0);
}

void ThemeTest::customizeModelListsBothHalves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Theme theme(writeDataDir(dir));
    auto *both = qobject_cast<ThemeSettingsModel *>(theme.customizeSchema());
    auto *colors = qobject_cast<ThemeSettingsModel *>(theme.colorsSchema());
    auto *style = qobject_cast<ThemeSettingsModel *>(theme.styleSchema());
    const auto full = [](ThemeSettingsModel *m, int row) {
        const QModelIndex i = m->index(row);
        return i.data(ThemeSettingsModel::SectionRole).toString() + u'.' + i.data(ThemeSettingsModel::KeyRole).toString();
    };

    QCOMPARE(both->rowCount(), colors->rowCount() + style->rowCount());
    QSet<QString> seen;
    for (int row = 0; row < both->rowCount(); ++row) {
        QVERIFY2(!seen.contains(full(both, row)), qPrintable(full(both, row)));
        seen.insert(full(both, row));
    }
    for (int row = 0; row < colors->rowCount(); ++row)
        QVERIFY(seen.contains(full(colors, row)));
    for (int row = 0; row < style->rowCount(); ++row)
        QVERIFY(seen.contains(full(style, row)));

    // Each section is one run, colors before style.
    QSet<QString> closed;
    QString section;
    bool sawStyle = false;
    for (int row = 0; row < both->rowCount(); ++row) {
        const QModelIndex i = both->index(row);
        const QString s = i.data(ThemeSettingsModel::SectionRole).toString();
        const QString half = i.data(ThemeSettingsModel::HalfRole).toString();
        if (s != section) {
            if (!section.isEmpty())
                closed.insert(section);
            QVERIFY2(!closed.contains(s), qPrintable(s));
            section = s;
            sawStyle = false;
            QVERIFY(i.data(ThemeSettingsModel::FirstInSectionRole).toBool());
        }
        if (half == "style")
            sawStyle = true;
        else
            QVERIFY2(!sawStyle, qPrintable(full(both, row)));
    }

    // The show filter trims rows and sections; the half models ignore it.
    both->setShow("colors");
    QCOMPARE(both->rowCount(), colors->rowCount());
    QVERIFY(both->firstRow("hero") >= 0);
    QCOMPARE(both->firstRow("radius"), -1);
    for (const QVariant &s : both->sections())
        QVERIFY(colors->firstRow(s.toMap().value("name").toString()) >= 0);
    both->setShow("style");
    QCOMPARE(both->rowCount(), style->rowCount());
    QCOMPARE(both->sections().size(), style->sections().size());
    const int styleRows = style->rowCount();
    style->setShow("colors");
    QCOMPARE(style->rowCount(), styleRows);
    both->setShow("all");
    QCOMPARE(both->rowCount(), colors->rowCount() + style->rowCount());

    // changedCount counts both halves; changedOnly and show combine.
    QCOMPARE(both->changedCount(), 0);
    theme.setValue("palette", "accent", QColor("#123456"));
    theme.setValue("radius", "medium", 21);
    QCOMPARE(both->changedCount(), colors->changedCount() + style->changedCount());
    QCOMPARE(both->changedCount(), 2);
    both->setChangedOnly(true);
    QCOMPARE(both->rowCount(), 2);
    QCOMPARE(full(both, 0), QStringLiteral("palette.accent"));
    both->setShow("style");
    QCOMPARE(both->rowCount(), 1);
    QCOMPARE(both->changedCount(), 2);
}

// A color edit and a radius edit land in different files; undoing all of them puts both back.
void ThemeTest::editsRouteByHalf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    theme.beginEditSession();
    theme.setValue("palette", "accent", QColor("#00ff00"));
    QTest::qWait(700);
    QVERIFY(QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    QVERIFY(!QFile::exists(dataDir + "/themes/styles/Soft.toml"));
    QVERIFY(!read(dataDir + "/themes/colors/Ember.toml").contains("[radius]"));

    theme.setValue("radius", "medium", 20);
    QTest::qWait(700);
    QVERIFY(read(dataDir + "/themes/styles/Soft.toml").contains("medium = 20"));
    QVERIFY(!read(dataDir + "/themes/styles/Soft.toml").contains("accent"));
    QVERIFY(!read(dataDir + "/themes/colors/Ember.toml").contains("medium"));

    QCOMPARE(theme.activeFile("palette", "accent"), QStringLiteral("Ember.toml"));
    QCOMPARE(theme.activeFile("radius", "medium"), QStringLiteral("Soft.toml"));
    QCOMPARE(theme.sourceLabel("palette", "accent"), QStringLiteral("Ember.toml (colors)"));
    QCOMPARE(theme.sourceLabel("radius", "medium"), QStringLiteral("Soft.toml (style)"));
    QCOMPARE(theme.sessionChanges().size(), 2);

    theme.revertValue("radius", "medium");
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 8);
    QCOMPARE(theme.sessionChanges().size(), 1);

    theme.setValue("radius", "medium", 20);
    theme.revertAll();
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 8);
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#e15f24"));
    QVERIFY(theme.sessionChanges().isEmpty());
    QTest::qWait(700);
    QVERIFY(!read(dataDir + "/themes/styles/Soft.toml").contains("medium"));
    QVERIFY(!read(dataDir + "/themes/colors/Ember.toml").contains("accent"));
}

void ThemeTest::switchingTheStyleStartsANewSession()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    theme.beginEditSession();
    theme.setValue("palette", "accent", QColor("#00ff00"));
    QCOMPARE(theme.sessionChanges().size(), 1);

    theme.setStyle("Round");
    QVERIFY(theme.sessionChanges().isEmpty());
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 12);
    // The colors edit was saved, not lost, and the colors didn't change.
    QVERIFY(read(dataDir + "/themes/colors/Ember.toml").contains("accent = \"#00ff00\""));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#00ff00"));
}

void ThemeTest::resetToBuiltInRemovesTheLayer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QVERIFY(!theme.canResetToBuiltIn("colors"));
    QVERIFY(!theme.resetToBuiltIn("colors"));

    theme.beginEditSession();
    theme.setValue("palette", "accent", QColor("#00ff00"));
    theme.setValue("radius", "medium", 20);
    QTest::qWait(700);
    QVERIFY(theme.canResetToBuiltIn("colors"));
    QVERIFY(theme.canResetToBuiltIn("style"));
    QVERIFY(!theme.canResetToBuiltIn("theme"));

    QVERIFY(theme.resetToBuiltIn("colors"));
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    QVERIFY(QFile::exists(dataDir + "/themes/styles/Soft.toml"));
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#e15f24"));
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 20);
    QVERIFY(!theme.canResetToBuiltIn("colors"));
    // Only the style's change is left to undo.
    QCOMPARE(theme.sessionChanges().size(), 1);

    // Unsaved edits of that half go with it.
    theme.setValue("palette", "accent", QColor("#0000ff"));
    QVERIFY(theme.resetToBuiltIn("style"));
    QTest::qWait(700);
    QVERIFY(!QFile::exists(dataDir + "/themes/styles/Soft.toml"));
    QCOMPARE(theme.resolvedValue("radius", "medium").toInt(), 8);
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#0000ff"));

    // A user's own file isn't a layer on a built-in.
    QVERIFY(theme.createColors("mine", "Ember"));
    QVERIFY(theme.createStyle("mine", "Soft"));
    QVERIFY(!theme.canResetToBuiltIn("colors"));
    QVERIFY(QFile::exists(dataDir + "/themes/colors/mine.toml"));
}

void ThemeTest::openingAnUneditedBuiltInWritesTheHeaderFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = writeDataDir(dir);

    Theme theme(dataDir);
    QVERIFY(!QFile::exists(dataDir + "/themes/colors/Ember.toml"));
    QCOMPARE(theme.colorsFilePath(), dataDir + "/themes/colors/Ember.toml");
    const QString header = read(dataDir + "/themes/colors/Ember.toml");
    QVERIFY2(header.startsWith("# Your changes to the built-in Ember colors"), qPrintable(header));
    QVERIFY(header.contains("themes/colors/reference.toml"));

    QCOMPARE(theme.styleFilePath(), dataDir + "/themes/styles/Soft.toml");
    QVERIFY(read(dataDir + "/themes/styles/Soft.toml").contains("built-in Soft style"));
    QVERIFY(read(dataDir + "/themes/styles/Soft.toml").contains("themes/styles/reference.toml"));

    // The file with only comments changes nothing, and a later edit keeps the header.
    QTRY_VERIFY(theme.problems().isEmpty());
    QCOMPARE(theme.resolvedValue("palette", "accent").value<QColor>(), QColor("#e15f24"));
    theme.setValue("palette", "accent", QColor("#00ff00"));
    QTest::qWait(700);
    QVERIFY(read(dataDir + "/themes/colors/Ember.toml").startsWith("# Your changes"));
    QVERIFY(read(dataDir + "/themes/colors/Ember.toml").contains("accent = \"#00ff00\""));

    // A file of the user's own starts empty.
    QVERIFY(theme.createColors("mine", "Ember"));
    QVERIFY(theme.createStyle("mine", "Soft"));
    QFile::remove(dataDir + "/themes/colors/mine.toml");
    QCOMPARE(theme.colorsFilePath(), dataDir + "/themes/colors/mine.toml");
    QCOMPARE(read(dataDir + "/themes/colors/mine.toml"), QString());
}

QTEST_MAIN(ThemeTest)
#include "theme_test.moc"
