import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide

    // Only platforms with an emulator: a ROM folder is only useful once one can launch it.
    readonly property var withEmulator: Library.platformSettings.filter(p => p.emulators.length > 0)
    readonly property var ready: withEmulator.filter(p => root.emulatorFound(p))
    readonly property var toInstall: withEmulator.filter(p => !root.emulatorFound(p))
    // Kept here because the rows are rebuilt when the lists change.
    property string expandedId: ""

    spacing: 0

    Component.onCompleted: Installer.refresh()

    function toggle(platformId) {
        root.expandedId = root.expandedId === platformId ? "" : platformId
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
        root.toggle(platformId)
    }

    component Heading: Label {
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.weight: Theme.font.strongWeight
    }

    component PlatformCard: RowCard {
        id: card

        property var platforms: []

        panel: true

        Repeater {
            model: card.platforms

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
                    expanded: root.expandedId === entry.modelData.id
                    firstInCard: entry.index === 0
                    lastInCard: entry.index === card.platforms.length - 1
                    onToggleRequested: root.toggle(entry.modelData.id)
                }
            }
        }
    }

    RowLayout {
        visible: root.ready.length > 0
        Layout.fillWidth: true
        Layout.bottomMargin: 10
        spacing: 10

        Heading {
            Layout.fillWidth: true
            text: qsTr("Ready")
        }
        ListAction {
            text: qsTr("Find from one folder")
            tip: qsTr("Match a folder's subfolders to platforms by name")
            iconPath: Icons.folder
            onClicked: findDialog.active = true
        }
    }

    PlatformCard {
        visible: root.ready.length > 0
        platforms: root.ready
    }

    Heading {
        visible: root.toInstall.length > 0
        Layout.topMargin: root.ready.length > 0 ? Theme.settingsWindow.sectionSpacing : 0
        Layout.bottomMargin: 4
        text: qsTr("Install an emulator")
    }
    Label {
        visible: root.toInstall.length > 0
        Layout.fillWidth: true
        Layout.bottomMargin: 10
        text: qsTr("Lattis downloads these for you, as an AppImage or from Flathub. Installs carry on while you go to the next step.")
        color: Theme.onboarding.descriptionColor
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }

    PlatformCard {
        visible: root.toInstall.length > 0
        platforms: root.toInstall
    }

    Item { Layout.fillHeight: true }
}
