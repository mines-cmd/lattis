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

    function openSubmenu() {
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item?.subMenu === themes) {
                currentIndex = i
                themes.popup(item, item.width, 0)
            }
        }
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
    SettingsMenuItem {
        iconPath: Icons.download
        visible: AppUpdate.notify
        text: AppUpdate.state === "ready" ? qsTr("Restart to update to %1").arg(AppUpdate.latestVersion)
            : qsTr("Update to %1").arg(AppUpdate.latestVersion)
        onTriggered: Qt.callLater(() => root.chosen("update_app"))
    }
    SettingsMenuSeparator { visible: AppUpdate.notify }
    Instantiator {
        model: General.railButtonOrder
        delegate: Entry {
            required property string modelData
            actionName: modelData
            iconPath: modelData === "add_game" ? Icons.add : modelData === "play_statistics" ? Icons.barChartFill
                    : modelData === "gallery" ? Icons.image : Icons.gear
            text: modelData === "add_game" ? qsTr("Add game")
                : modelData === "play_statistics" ? qsTr("Play statistics")
                : modelData === "gallery" ? qsTr("Gallery") : qsTr("Settings")
        }
        onObjectAdded: (index, object) => root.insertItem(index + 4, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "look_up_missing"
        iconPath: Icons.searchEyeLine
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
    Entry {
        actionName: "sync_steam"
        iconPath: Icons.steamLine
        visible: SteamLibrary.configured
        enabled: !SteamLibrary.syncing
        text: SteamLibrary.syncing ? qsTr("Syncing Steam library") : qsTr("Sync Steam library")
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
            delegate: SettingsMenuItem {
                required property string modelData
                text: modelData
                checked: modelData === Theme.activeTheme
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
    Entry {
        actionName: "toggle_fullscreen"
        iconPath: Icons.fullscreenLine
        text: qsTr("Fullscreen")
    }
    Entry {
        actionName: "toggle_toolbar"
        iconPath: General.toolbar ? Icons.eyeCloseLine : Icons.eyeLine
        text: General.toolbar ? qsTr("Hide toolbar") : qsTr("Show toolbar")
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
        iconPath: Icons.save2Line
        text: qsTr("Back up saves")
    }
    Entry {
        actionName: "import_backup"
        iconPath: Icons.inboxUnarchiveLine
        text: qsTr("Import")
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "show_shortcuts"
        iconPath: Icons.commandLine
        text: qsTr("Keyboard shortcuts")
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
