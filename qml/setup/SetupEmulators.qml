import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide
    property bool showAll: false

    // Only platforms with an emulator: a ROM folder is only useful once one can launch it.
    readonly property var withEmulator: Library.platformSettings.filter(p => p.emulators.length > 0)
    readonly property var installed: withEmulator.filter(p => root.emulatorFound(p))
    readonly property var notInstalled: withEmulator.filter(p => !root.emulatorFound(p))
    readonly property var shown: root.showAll ? installed.concat(notInstalled) : installed

    spacing: Theme.onboarding.spacing

    readonly property Component headerAction: SettingsButton {
        text: qsTr("Find from one folder")
        tip: qsTr("Match a folder's subfolders to platforms by name")
        iconPath: Icons.folder
        onClicked: findDialog.active = true
    }

    function emulatorOf(p) {
        return Library.emulators.find(e => e.id === p.emulators[0])
    }
    function emulatorFound(p) {
        return p.emulators.some(id => { const e = Library.emulators.find(e => e.id === id); return e && e.found })
    }
    function installLabel(p) {
        const e = root.emulatorOf(p)
        if (!e)
            return ""
        switch (e.installKind) {
        case "flatpak": return qsTr("%1 · Flatpak").arg(e.name)
        case "appimage": return qsTr("%1 · AppImage").arg(e.name)
        case "executable": return qsTr("%1 · installed").arg(e.name)
        case "path": return qsTr("%1 · your path").arg(e.name)
        }
        return e.detecting ? qsTr("Looking for %1…").arg(e.name) : qsTr("%1 not found").arg(e.name)
    }
    function save(id, folders) {
        const error = Library.setPlatformValue(id, "rom_folders", folders)
        if (error !== "")
            guide.message(error)
    }
    function removed(list, i) {
        const out = list.slice()
        out.splice(i, 1)
        return out
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
                readonly property bool platformInstalled: root.emulatorFound(modelData)

                Layout.fillWidth: true
                spacing: Theme.onboarding.controlSpacing

                Rectangle {
                    visible: entry.index > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    Layout.bottomMargin: 4
                    implicitHeight: 1
                    color: Theme.settingsWindow.rowDivider
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Icon {
                        path: Icons.forPlatform(entry.modelData.icon)
                        size: Theme.settingsWindow.iconSize + 4
                        color: Theme.settingsWindow.labelColor
                    }
                    Label {
                        text: entry.modelData.name
                        color: Theme.settingsWindow.labelColor
                        font.pixelSize: Theme.settingsWindow.labelFontSize
                        font.bold: true
                    }
                    Rectangle {
                        readonly property color tint: entry.platformInstalled ? Theme.settingsWindow.installFound
                                                                              : Theme.settingsWindow.installMissing

                        visible: entry.platformInstalled
                        implicitWidth: kindText.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                        implicitHeight: kindText.implicitHeight + 2
                        radius: Theme.settingsWindow.sourceTagRadius
                        color: Qt.alpha(tint, Theme.settingsWindow.sourceTagFillOpacity)
                        border.color: tint
                        border.width: Theme.settingsWindow.sourceTagBorderWidth

                        Label {
                            id: kindText
                            anchors.centerIn: parent
                            text: root.installLabel(entry.modelData)
                            color: parent.tint
                            font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        visible: !entry.platformInstalled
                        text: qsTr("Needs %1").arg(root.emulatorOf(entry.modelData)?.name ?? "")
                        color: Theme.settingsWindow.installMissing
                        font.pixelSize: Theme.settingsWindow.descriptionFontSize
                    }
                    SettingsButton {
                        visible: entry.platformInstalled
                        text: qsTr("Add folder")
                        iconPath: Icons.add
                        onClicked: {
                            folderDialog.active = true
                            folderDialog.platformId = entry.modelData.id
                            folderDialog.folders = entry.modelData.romFolders
                        }
                    }
                }

                ColumnLayout {
                    visible: entry.platformInstalled
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.settingsWindow.iconSize + 14
                    spacing: 6

                    Repeater {
                        model: entry.modelData.romFolders

                        RowLayout {
                            id: folderRow
                            required property string modelData
                            required property int index

                            Layout.fillWidth: true
                            spacing: Theme.onboarding.controlSpacing

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: pathLabel.implicitHeight + 12
                                radius: Theme.settingsWindow.inputRadius
                                color: Theme.settingsWindow.inputBackground

                                Label {
                                    id: pathLabel
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 10
                                    text: folderRow.modelData
                                    color: Theme.settingsWindow.inputText
                                    font.family: Theme.font.monoFamily
                                    font.pixelSize: Theme.settingsWindow.pathFontSize
                                    elide: Text.ElideMiddle
                                }
                            }
                            SettingsButton {
                                implicitHeight: pathLabel.implicitHeight + 12
                                leftPadding: (implicitHeight - Theme.settingsWindow.iconSize) / 2
                                rightPadding: leftPadding
                                iconPath: Icons.close
                                tip: qsTr("Stop scanning this folder")
                                onClicked: root.save(entry.modelData.id, root.removed(entry.modelData.romFolders, folderRow.index))
                            }
                        }
                    }

                    Label {
                        visible: entry.modelData.romFolders.length === 0
                        Layout.fillWidth: true
                        text: qsTr("No ROM folders yet.")
                        color: Theme.settingsWindow.descriptionColor
                        font.pixelSize: Theme.settingsWindow.descriptionFontSize
                    }
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
                  : root.notInstalled.length === 1
                  ? qsTr("1 more platform has no emulator installed here (%1).").arg(root.notInstalled[0].name)
                  : qsTr("%1 more platforms have no emulator installed here (%2).")
                        .arg(root.notInstalled.length).arg(root.notInstalled.map(p => p.name).join(", "))
            color: Theme.onboarding.descriptionColor
            wrapMode: Text.WordWrap
        }
        SettingsButton {
            text: root.showAll ? qsTr("Show installed only") : qsTr("Show all platforms")
            onClicked: root.showAll = !root.showAll
        }
    }

    Loader {
        id: folderDialog
        property string platformId: ""
        property var folders: []
        active: false
        sourceComponent: RomFolderDialog {
            onPicked: path => {
                if (!folderDialog.folders.includes(path))
                    root.save(folderDialog.platformId, folderDialog.folders.concat([path]))
            }
            onClosed: folderDialog.active = false
        }
    }

    Item { Layout.fillHeight: true }
}
