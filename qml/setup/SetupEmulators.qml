import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide
    // With nothing installed the short list would be empty, and every platform's Install is what's useful.
    property bool showAll: installed.length === 0

    // Only platforms with an emulator: a ROM folder is only useful once one can launch it.
    readonly property var withEmulator: Library.platformSettings.filter(p => p.emulators.length > 0)
    readonly property var installed: withEmulator.filter(p => root.emulatorFound(p))
    readonly property var notInstalled: withEmulator.filter(p => !root.emulatorFound(p))
    // Platforms whose emulator was installed from this step keep their place when it's found, instead of jumping up.
    property var startedHere: ({})
    readonly property var shown: root.showAll
        ? withEmulator.filter(p => root.emulatorFound(p) && !root.startedHere[p.id])
              .concat(withEmulator.filter(p => !root.emulatorFound(p) || root.startedHere[p.id]))
        : installed
    readonly property bool canInstallAny: notInstalled.some(p => root.installId(p) !== "")

    spacing: Theme.onboarding.spacing

    Component.onCompleted: Installer.refresh()

    readonly property Component headerAction: SettingsButton {
        text: qsTr("Find from one folder")
        tip: qsTr("Match a folder's subfolders to platforms by name")
        iconPath: Icons.folder
        onClicked: findDialog.active = true
    }

    // The emulator to install for a platform: one with a job already, else the first that can be installed.
    function installId(p) {
        return p.emulators.find(id => (Installer.emulators[id]?.state ?? "idle") !== "idle")
            ?? p.emulators.find(id => (Installer.emulators[id]?.routes.length ?? 0) > 0)
            ?? ""
    }
    // Platform ids whose install card is open; kept here because the rows are rebuilt when the list changes.
    property var expandedIds: ({})
    function toggle(platformId) {
        const next = Object.assign({}, root.expandedIds)
        next[platformId] = !next[platformId]
        root.expandedIds = next
    }
    function markStarted(platformId) {
        if (root.startedHere[platformId])
            return
        const next = Object.assign({}, root.startedHere)
        next[platformId] = true
        root.startedHere = next
        root.showAll = true
    }
    function emulatorFound(p) {
        return p.emulators.some(id => { const e = Library.emulators.find(e => e.id === id); return e && e.found })
    }

    Loader {
        id: findDialog
        property string pendingFolder: ""
        active: false
        sourceComponent: FindRomFoldersDialog {
            initialFolder: findDialog.pendingFolder
            onClosed: findDialog.active = false
        }
    }

    function openFindDialogForScreenshot(path) {
        findDialog.pendingFolder = path
        findDialog.active = true
    }
    function expandForScreenshot(platformId) {
        root.showAll = true
        root.toggle(platformId)
    }

    SettingsCard {
        panel: true
        visible: root.shown.length > 0
        spacing: 0

        Repeater {
            model: root.shown

            ColumnLayout {
                id: entry
                required property var modelData
                required property int index

                Layout.fillWidth: true
                spacing: 0

                Rectangle {
                    visible: Theme.settingsWindow.rowDividers && entry.index > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.settingsWindow.rowPadding
                    Layout.rightMargin: Theme.settingsWindow.rowPadding
                    implicitHeight: 1
                    color: Theme.settingsWindow.rowDivider
                }

                SetupPlatformRow {
                    Layout.fillWidth: true
                    platform: entry.modelData
                    expanded: root.expandedIds[entry.modelData.id] ?? false
                    firstInCard: entry.index === 0
                    lastInCard: entry.index === root.shown.length - 1
                    onToggleRequested: root.toggle(entry.modelData.id)
                    onInstallingChanged: if (installing) root.markStarted(entry.modelData.id)
                }
            }
        }
    }

    RowLayout {
        visible: root.notInstalled.length > 0
        Layout.fillWidth: true
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: root.showAll
                  ? qsTr("Platforms without an installed emulator are shown too.")
                  : (root.notInstalled.length === 1
                     ? qsTr("1 more platform has no emulator installed here (%1).").arg(root.notInstalled[0].name)
                     : qsTr("%1 more platforms have no emulator installed here (%2).")
                           .arg(root.notInstalled.length).arg(root.notInstalled.map(p => p.name).join(", ")))
                    + (root.canInstallAny ? " " + qsTr("Show them to install an emulator from here.") : "")
            color: Theme.onboarding.descriptionColor
            wrapMode: Text.WordWrap
        }
        SettingsButton {
            text: root.showAll ? qsTr("Show installed only") : qsTr("Show all platforms")
            onClicked: root.showAll = !root.showAll
        }
    }

    Item { Layout.fillHeight: true }
}
