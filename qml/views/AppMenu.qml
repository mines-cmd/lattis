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
    SettingsMenuItem {
        readonly property var updates: Installer.updates
        readonly property var waiting: updates.filter(u => u.state === "idle" || u.state === "failed")
        iconPath: Icons.download
        visible: updates.length > 0
        enabled: waiting.length > 0
        text: waiting.length === 0
              ? (updates.length === 1 ? qsTr("Updating %1").arg(updates[0].name) : qsTr("Updating emulators"))
            : waiting.length === 1 ? qsTr("Update %1 to %2").arg(waiting[0].name).arg(waiting[0].version)
            : qsTr("Update %1 emulators").arg(waiting.length)
        onTriggered: Qt.callLater(() => root.chosen("update_emulators"))
    }
    SettingsMenuSeparator { visible: AppUpdate.notify || Installer.updates.length > 0 }
    Entry {
        actionName: "add_game"
        iconPath: Icons.add
        text: qsTr("Add game")
    }
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
    SettingsMenuItem {
        iconPath: Icons.download
        visible: Object.values(Installer.emulators).some(e => e.installedByApp && e.route === "appimage")
        text: qsTr("Check for emulator updates")
        onTriggered: Qt.callLater(() => root.chosen("check_emulator_updates"))
    }
    Entry {
        actionName: "sync_steam"
        iconPath: Icons.steamLine
        visible: SteamLibrary.configured
        enabled: !SteamLibrary.syncing
        text: SteamLibrary.syncing ? qsTr("Syncing Steam library") : qsTr("Sync Steam library")
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "overview"
        iconPath: Icons.book
        text: qsTr("Library overview")
    }
    Entry {
        actionName: "play_statistics"
        iconPath: Icons.barChartFill
        text: qsTr("Play statistics")
    }
    Entry {
        actionName: "gallery"
        iconPath: Icons.image
        text: qsTr("Gallery")
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
        actionName: "toggle_view"
        iconPath: General.view === "grid" ? Icons.viewList : Icons.layoutGrid
        text: General.view === "grid" ? qsTr("List view") : qsTr("Grid view")
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
        actionName: "open_settings"
        iconPath: Icons.gear
        text: qsTr("Settings")
    }
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
