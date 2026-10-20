#include "actions.h"
#include "actionschema.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// Loading [shortcuts] from settings.toml, validation, conflicts, and saving edits back.
// No QML engine involved; Actions is plain C++ here.
class ActionsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWithoutShortcutsSection();
    void settingsOverrideDefaults();
    void badEntriesWarnAndKeepDefaults();
    void conflictsOnlyWithinAWindow();
    void renamedActionsKeepTheirKeys();
    void setKeysSavesAndResetRemoves();
    void setKeysIntoCommentedTemplate();
    void sequenceForNormalizes();
    void namesAndTrigger();
    void everyActionHasAGroup();
    void listModel();
    void listModelFilter();

private:
    static QString writeSettings(const QTemporaryDir &dir, const QByteArray &content);
    static QString readSettings(const QTemporaryDir &dir);
};

QString ActionsTest::writeSettings(const QTemporaryDir &dir, const QByteArray &content)
{
    QFile file(dir.path() + "/settings.toml");
    if (file.open(QIODevice::WriteOnly))
        file.write(content);
    return dir.path();
}

QString ActionsTest::readSettings(const QTemporaryDir &dir)
{
    QFile file(dir.path() + "/settings.toml");
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

void ActionsTest::defaultsWithoutShortcutsSection()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, "[appearance]\ntheme = \"custom\"\n"));

    QCOMPARE(actions.keysOf("toggle_favorite"), QStringList{"Ctrl+D"});
    QCOMPARE(actions.keysOf("focus_search"), (QStringList{"Ctrl+F", "/"}));
    QCOMPARE(actions.keysOf("toggle_favorites_filter"), QStringList{"Ctrl+Shift+D"});
    QCOMPARE(actions.keysOf("cycle_sort"), QStringList{"Ctrl+Shift+S"});
    QCOMPARE(actions.keysOf("clear_filters"), QStringList{"Ctrl+Shift+Backspace"});
    QCOMPARE(actions.keysOf("look_up_missing"), QStringList{"Ctrl+Shift+L"});
    QCOMPARE(actions.isCustom("toggle_favorite"), false);
    QVERIFY2(actions.problems().isEmpty(), qPrintable(actions.problems().join('\n')));

    auto *keys = qobject_cast<QQmlPropertyMap *>(actions.keys());
    QCOMPARE(keys->value("launch").toStringList(), (QStringList{"Return", "Enter"}));
}

void ActionsTest::settingsOverrideDefaults()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, "[shortcuts]\n"
                                       "toggle_favorite = \"ctrl+shift+k\"\n"
                                       "open_folder = [\"Ctrl+L\", \"F4\"]\n"
                                       "quit = []\n"));

    QCOMPARE(actions.keysOf("toggle_favorite"), QStringList{"Ctrl+Shift+K"}); // normalized
    QCOMPARE(actions.keysOf("open_folder"), (QStringList{"Ctrl+L", "F4"}));
    QCOMPARE(actions.keysOf("quit"), QStringList{}); // [] unbinds
    QCOMPARE(actions.isCustom("quit"), true);
    QCOMPARE(actions.isCustom("launch"), false);
}

void ActionsTest::badEntriesWarnAndKeepDefaults()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, "[shortcuts]\n"
                                       "lanch = \"Ctrl+L\"\n"
                                       "quit = \"Ctrl+Nope\"\n"
                                       "add_game = 5\n"));

    const QString problems = actions.problems().join('\n');
    QVERIFY2(problems.contains("unknown action \"lanch\" — did you mean \"launch\"?"), qPrintable(problems));
    QVERIFY2(problems.contains("\"Ctrl+Nope\" isn't a key"), qPrintable(problems));
    QVERIFY2(problems.contains("add_game must be keys in quotes"), qPrintable(problems));
    QCOMPARE(actions.keysOf("quit"), QStringList{"Ctrl+Q"});
    QCOMPARE(actions.keysOf("add_game"), QStringList{"Ctrl+N"});
}

void ActionsTest::conflictsOnlyWithinAWindow()
{
    QTemporaryDir dir;
    // new_colors (settings window) sharing Ctrl+D with toggle_favorite (main window) is fine;
    // open_settings works everywhere, so sharing Ctrl+E with open_colors_file is not.
    Actions actions(writeSettings(dir, "[shortcuts]\n"
                                       "new_colors = \"Ctrl+D\"\n"
                                       "open_settings = \"Ctrl+E\"\n"));

    QCOMPARE(actions.hasConflict("toggle_favorite"), false);
    QCOMPARE(actions.hasConflict("new_colors"), false);
    QCOMPARE(actions.hasConflict("open_settings"), true);
    QCOMPARE(actions.hasConflict("open_colors_file"), true);
    QCOMPARE(actions.problems().size(), 1);
}

// Bindings set for actions that were renamed move to the new names, with the file's other content kept.
void ActionsTest::renamedActionsKeepTheirKeys()
{
    QTemporaryDir dir;
    {
        Actions actions(writeSettings(dir, "# mine\n[shortcuts]\nnew_theme = \"Ctrl+Alt+T\"\n"
                                            "open_theme_file = [\"F4\", \"Ctrl+Alt+E\"]\nquit = \"Ctrl+Alt+Q\"\n"));
        QVERIFY2(actions.problems().isEmpty(), qPrintable(actions.problems().join('\n')));
        QCOMPARE(actions.keysOf("new_colors"), QStringList{"Ctrl+Alt+T"});
        QCOMPARE(actions.keysOf("open_colors_file"), (QStringList{"F4", "Ctrl+Alt+E"}));
        QCOMPARE(actions.isCustom("new_colors"), true);
        QCOMPARE(actions.keysOf("quit"), QStringList{"Ctrl+Alt+Q"});
        // The style's actions have their own keys.
        QCOMPARE(actions.keysOf("new_style"), QStringList{"Ctrl+Alt+N"});
    }
    const QString text = readSettings(dir);
    QVERIFY2(!text.contains("new_theme") && !text.contains("open_theme_file"), qPrintable(text));
    QVERIFY2(text.contains("# mine") && text.contains("quit = \"Ctrl+Alt+Q\""), qPrintable(text));

    // A binding already on the new name wins; the old one is dropped.
    QTemporaryDir other;
    Actions actions(writeSettings(other, "[shortcuts]\nnew_theme = \"Ctrl+Alt+T\"\nnew_colors = \"Ctrl+Alt+C\"\n"));
    QCOMPARE(actions.keysOf("new_colors"), QStringList{"Ctrl+Alt+C"});
    QVERIFY(!readSettings(other).contains("new_theme"));
}

void ActionsTest::setKeysSavesAndResetRemoves()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, "# My settings\n[appearance]\ntheme = \"custom\" # the dark one\n"));

    actions.setKeys("toggle_favorite", {"Ctrl+B"});
    QCOMPARE(actions.keysOf("toggle_favorite"), QStringList{"Ctrl+B"});
    QString text = readSettings(dir);
    QVERIFY2(text.contains("[shortcuts]\ntoggle_favorite = \"Ctrl+B\""), qPrintable(text));
    QVERIFY2(text.startsWith("# My settings\n[appearance]\ntheme = \"custom\" # the dark one\n"), qPrintable(text));

    actions.setKeys("focus_search", {"Ctrl+K", "/"});
    QVERIFY2(readSettings(dir).contains("focus_search = [\"Ctrl+K\", \"/\"]"), qPrintable(readSettings(dir)));

    actions.setKeys("quit", {});
    QCOMPARE(actions.keysOf("quit"), QStringList{});
    QVERIFY2(readSettings(dir).contains("quit = []"), qPrintable(readSettings(dir)));

    actions.resetKeys("toggle_favorite");
    QCOMPARE(actions.keysOf("toggle_favorite"), QStringList{"Ctrl+D"});
    QCOMPARE(actions.isCustom("toggle_favorite"), false);
    QVERIFY(!readSettings(dir).contains("toggle_favorite"));
}

// New data dirs get a [shortcuts] section holding only a commented-out example.
void ActionsTest::setKeysIntoCommentedTemplate()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, "[appearance]\ntheme = \"custom\"\n\n"
                                       "# Keyboard shortcuts\n[shortcuts]\n# toggle_favorite = \"Ctrl+D\"\n"));

    actions.setKeys("launch", {"Ctrl+L"});
    QVERIFY2(actions.problems().isEmpty(), qPrintable(actions.problems().join('\n')));
    QCOMPARE(actions.keysOf("launch"), QStringList{"Ctrl+L"});
    const QString text = readSettings(dir);
    QVERIFY2(text.contains("# toggle_favorite = \"Ctrl+D\""), qPrintable(text));
    QCOMPARE(text.count("[shortcuts]"), 1);
}

void ActionsTest::sequenceForNormalizes()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, ""));

    QCOMPARE(actions.sequenceFor(Qt::Key_D, Qt::ControlModifier), QStringLiteral("Ctrl+D"));
    QCOMPARE(actions.sequenceFor(Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier),
             QStringLiteral("Ctrl+Shift+Tab"));
    QCOMPARE(actions.sequenceFor(Qt::Key_Enter, Qt::KeypadModifier), QStringLiteral("Enter"));
    QCOMPARE(actions.sequenceFor(Qt::Key_Control, Qt::ControlModifier), QString());
}

void ActionsTest::namesAndTrigger()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, ""));

    const QStringList main = actions.names("main");
    QVERIFY(main.contains("launch"));
    QVERIFY(main.contains("quit")); // works everywhere
    QVERIFY(!main.contains("settings_search"));
    QVERIFY(actions.names("settings").contains("quit"));
    // The app menu's actions: all unbound by default, the data folder everywhere.
    for (const char *name : {"scan_roms", "apply_steam_changes", "about"}) {
        QVERIFY2(main.contains(name), name);
        QVERIFY2(actions.keysOf(name).isEmpty(), name);
    }
    QCOMPARE(actions.keysOf("app_menu"), QStringList{"F10"});
    QVERIFY(!actions.names("settings").contains("app_menu"));
    QVERIFY(actions.names("settings").contains("open_data_folder"));
    QVERIFY(!actions.names("settings").contains("about"));

    for (const ActionSchema::Action &a : ActionSchema::actions()) {
        const QString name = QString::fromUtf8(a.name.data(), int(a.name.size()));
        QVERIFY2(qobject_cast<QQmlPropertyMap *>(actions.keys())->contains(name), qPrintable(name));
    }

    QCOMPARE(actions.tip("Add to favorites", {"Ctrl+D"}), QStringLiteral("Add to favorites (Ctrl+D)"));
    QCOMPARE(actions.tip("Filters", {}), QStringLiteral("Filters"));

    QSignalSpy spy(&actions, &Actions::triggered);
    actions.trigger("launch");
    actions.trigger("no_such_action");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("launch"));
}

void ActionsTest::everyActionHasAGroup()
{
    for (const ActionSchema::Action &a : ActionSchema::actions()) {
        const ActionSchema::Group *g = ActionSchema::groupOf(a);
        QVERIFY2(g, a.name.data());
        QCOMPARE(g->name, a.group);
    }
    QCOMPARE(ActionSchema::groupOf({"nowhere", "nothing", "", ""}), nullptr);
}

void ActionsTest::listModel()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, ""));
    auto *model = qobject_cast<ActionListModel *>(actions.model());
    QVERIFY(model);
    const auto value = [&](int row, ActionListModel::Role role) { return model->data(model->index(row), role); };

    QCOMPARE(model->rowCount(), int(ActionSchema::actions().size()));
    QCOMPARE(value(0, ActionListModel::NameRole).toString(), QStringLiteral("previous_game"));
    QCOMPARE(value(0, ActionListModel::GroupTitleRole).toString(), QStringLiteral("Game list"));
    QCOMPARE(value(0, ActionListModel::KeysRole).toStringList(), QStringList{"Up"});
    QVERIFY(value(0, ActionListModel::FirstInGroupRole).toBool());
    QVERIFY(!value(0, ActionListModel::LastInGroupRole).toBool());

    // One section per group, in order; each group's first row starts it.
    const QVariantList sections = model->sections();
    QCOMPARE(sections.size(), qsizetype(ActionSchema::groups().size()));
    for (const QVariant &s : sections) {
        const QString group = s.toMap().value("name").toString();
        const int row = model->firstRow(group);
        QVERIFY2(row >= 0, qPrintable(group));
        QVERIFY(value(row, ActionListModel::FirstInGroupRole).toBool());
        QCOMPARE(value(row, ActionListModel::GroupRole).toString(), group);
        QVERIFY(row == 0 || value(row - 1, ActionListModel::LastInGroupRole).toBool());
    }

    // A changed key shows on the same rows.
    const int launch = model->firstRow("game");
    QCOMPARE(value(launch, ActionListModel::NameRole).toString(), QStringLiteral("launch"));
    QVERIFY(!value(launch, ActionListModel::IsCustomRole).toBool());
    QSignalSpy changed(model, &QAbstractItemModel::dataChanged);
    actions.setKeys("launch", {"Ctrl+L"});
    QVERIFY(changed.size() >= 1);
    QCOMPARE(value(launch, ActionListModel::KeysRole).toStringList(), QStringList{"Ctrl+L"});
    QVERIFY(value(launch, ActionListModel::IsCustomRole).toBool());
}

void ActionsTest::listModelFilter()
{
    QTemporaryDir dir;
    Actions actions(writeSettings(dir, ""));
    auto *model = qobject_cast<ActionListModel *>(actions.model());
    QSignalSpy sectionsChanged(model, &ActionListModel::sectionsChanged);
    const auto names = [&] {
        QStringList out;
        for (int row = 0; row < model->rowCount(); ++row)
            out.append(model->data(model->index(row), ActionListModel::NameRole).toString());
        return out;
    };

    // By title, any case.
    model->setFilterText("FAVORITES");
    QCOMPARE(names(), (QStringList{"toggle_favorites_filter", "toggle_favorite"}));
    QCOMPARE(sectionsChanged.size(), 1);
    QCOMPARE(model->sections().size(), 2);
    QCOMPARE(model->firstRow("list"), -1);
    // A filtered row is first and last of its group when its neighbors are filtered out.
    QVERIFY(model->data(model->index(1), ActionListModel::FirstInGroupRole).toBool());
    QVERIFY(model->data(model->index(1), ActionListModel::LastInGroupRole).toBool());

    // By group title and by key.
    model->setFilterText("Selected game");
    QVERIFY(names().contains("launch"));
    QVERIFY(names().contains("hide_game")); // no title or key that matches
    model->setFilterText("Ctrl+Shift+S");
    QCOMPARE(names(), QStringList{"cycle_sort"});

    // The same text again changes nothing.
    model->setFilterText("Ctrl+Shift+S");
    QCOMPARE(sectionsChanged.size(), 3);

    // A key change that brings a row into the filter resets the rows.
    actions.setKeys("launch", {"Ctrl+Shift+S"});
    QCOMPARE(names(), (QStringList{"cycle_sort", "launch"}));
    QCOMPARE(sectionsChanged.size(), 4);

    model->setFilterText("no such action");
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(model->sections().isEmpty());
    QCOMPARE(model->data(model->index(0), ActionListModel::NameRole), QVariant());
}

QTEST_MAIN(ActionsTest)
#include "actions_test.moc"
