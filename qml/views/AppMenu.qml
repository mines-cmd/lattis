import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    pops: true

    signal chosen(string name)
    signal editTheme()

    readonly property int steamWaiting: Library.steamWaiting.length
    readonly property bool hasRomFolders: Library.platformSettings.some(p => (p.romFolders ?? []).length > 0)

    BulkLookupModel { id: missing }
    onAboutToShow: missing.reload()

    function openSubmenu(which) {
        const submenu = which === "colors" ? colors : which === "style" ? styles : themes
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item?.subMenu === submenu) {
                currentIndex = i
                submenu.popup(item, item.width, 0)
            }
        }
    }

    component PickItem: SettingsMenuItem {
        required property string modelData
        property string current
        text: modelData
        checked: modelData === current
    }

    component Entry: SettingsMenuItem {
        property string actionName
        keys: Actions.label(Actions.keys[actionName] ?? [])
        onTriggered: Qt.callLater(() => root.chosen(actionName))
    }

    Entry {
        actionName: "apply_steam_changes"
        iconPath: Icons.steamLine
        visible: root.steamWaiting > 0
        enabled: !GameRunner.closingSteam
        text: GameRunner.closingSteam ? qsTr("Closing Steam")
            : root.steamWaiting === 1 ? qsTr("Apply 1 Steam change")
            : qsTr("Apply %1 Steam changes").arg(root.steamWaiting)
    }
    SettingsMenuSeparator { visible: root.steamWaiting > 0 }
    Instantiator {
        model: General.railButtonOrder
        delegate: Entry {
            required property string modelData
            actionName: modelData
            iconPath: modelData === "add_game" ? Icons.add : modelData === "play_statistics" ? Icons.barChartLine : Icons.gear
            text: modelData === "add_game" ? qsTr("Add game")
                : modelData === "play_statistics" ? qsTr("Play statistics") : qsTr("Settings")
        }
        onObjectAdded: (index, object) => root.insertItem(index + 2, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "look_up_missing"
        iconPath: Icons.searchEyeLine
        enabled: missing.missingCount > 0
        text: missing.missingCount > 0 ? qsTr("Look up missing info (%1)").arg(missing.missingCount)
                                       : qsTr("Look up missing info")
    }
    Entry {
        actionName: "scan_roms"
        iconPath: Icons.loopRightLine
        visible: root.hasRomFolders
        enabled: !Library.scanning
        text: Library.scanning ? qsTr("Scanning") : qsTr("Scan ROM folders now")
    }
    SettingsMenuSeparator {}
    SettingsMenu {
        id: themes
        pops: true
        title: qsTr("Theme")
        iconPath: Icons.brush
        checkColumn: true

        Instantiator {
            model: Theme.availableThemes
            delegate: PickItem {
                current: Theme.activeTheme
                onTriggered: Theme.setTheme(modelData)
            }
            onObjectAdded: (index, object) => themes.insertItem(index, object)
            onObjectRemoved: (index, object) => themes.removeItem(object)
        }
        SettingsMenuSeparator {}
        SettingsMenuItem {
            text: qsTr("Edit theme")
            onTriggered: Qt.callLater(root.editTheme)
        }
    }
    SettingsMenu {
        id: colors
        pops: true
        title: qsTr("Colors")
        iconPath: Icons.paletteLine
        checkColumn: true

        Instantiator {
            model: Theme.availableColors
            delegate: PickItem {
                current: Theme.activeColors
                onTriggered: Theme.setColors(modelData)
            }
            onObjectAdded: (index, object) => colors.insertItem(index, object)
            onObjectRemoved: (index, object) => colors.removeItem(object)
        }
    }
    SettingsMenu {
        id: styles
        pops: true
        title: qsTr("Style")
        iconPath: Icons.layoutMasonryLine
        checkColumn: true

        Instantiator {
            model: Theme.availableStyles
            delegate: PickItem {
                current: Theme.activeStyle
                onTriggered: Theme.setStyle(modelData)
            }
            onObjectAdded: (index, object) => styles.insertItem(index, object)
            onObjectRemoved: (index, object) => styles.removeItem(object)
        }
    }
    Entry {
        actionName: "toggle_fullscreen"
        iconPath: Icons.fullscreenLine
        text: qsTr("Fullscreen")
    }
    Entry {
        actionName: "toggle_rail"
        iconPath: General.railHidden ? Icons.eyeLine : Icons.eyeCloseLine
        text: General.railHidden ? qsTr("Show rail") : qsTr("Hide rail")
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "back_up_library"
        iconPath: Icons.archiveLine
        text: qsTr("Back up library")
    }
    Entry {
        actionName: "back_up_saves"
        iconPath: Icons.saveLine
        text: qsTr("Back up saves")
    }
    Entry {
        actionName: "import_backup"
        iconPath: Icons.inboxUnarchiveLine
        text: qsTr("Import")
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "setup_guide"
        iconPath: Icons.signpostLine
        text: qsTr("Setup guide")
    }
    Entry {
        actionName: "show_shortcuts"
        iconPath: Icons.commandLine
        text: qsTr("Keyboard shortcuts")
    }
    Entry {
        actionName: "open_data_folder"
        iconPath: Icons.folderOpenLine
        text: qsTr("Open data folder")
    }
    Entry {
        actionName: "about"
        iconPath: Icons.informationLine
        text: qsTr("About Lattis")
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "quit"
        iconPath: Icons.shutDownLine
        text: qsTr("Quit")
    }
}
