import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string folder: ""
    property var matches: []
    property var unchecked: ({})
    property string initialFolder: ""

    readonly property var matched: root.matches.filter(m => m.platformId !== "")
    readonly property var unmatched: root.matches.filter(m => m.platformId === "")
    readonly property var toAdd: root.matched.filter(m => !m.already && !root.unchecked[m.path])

    function refresh(path) {
        root.folder = path
        root.matches = path !== "" ? Library.matchRomFolders(path) : []
        root.unchecked = {}
    }
    function toggle(path) {
        const u = Object.assign({}, root.unchecked)
        u[path] = !u[path]
        root.unchecked = u
    }
    function apply() {
        const byPlatform = {}
        for (const m of root.toAdd) {
            if (!byPlatform[m.platformId])
                byPlatform[m.platformId] = []
            byPlatform[m.platformId].push(m.path)
        }
        for (const id in byPlatform) {
            const platform = Library.platformSettings.find(p => p.id === id)
            if (platform)
                Library.setPlatformValue(id, "rom_folders", platform.romFolders.concat(byPlatform[id]))
        }
        root.close()
    }

    parent: Overlay.overlay
    // Not anchors.centerIn: the matched/unmatched lists change the popup's height after it
    // opens, and centerIn doesn't re-run then.
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    modal: true
    Overlay.modal: Scrim {}
    padding: Theme.settingsWindow.dialogPadding
    width: 480
    background: Rectangle {
        color: Theme.settingsWindow.popupBackground
        border.color: Theme.onboarding.cardBorder
        border.width: 1
        radius: Theme.onboarding.cardRadius
    }

    Component.onCompleted: {
        open()
        if (root.initialFolder !== "") {
            field.text = root.initialFolder
            root.refresh(root.initialFolder)
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 12

        Label { text: qsTr("Find ROM folders"); color: Theme.settingsWindow.labelColor; font.bold: true }
        Label {
            Layout.fillWidth: true
            text: qsTr("Pick the folder that holds one folder per platform. Its folders are matched to platforms by name.")
            color: Theme.settingsWindow.descriptionColor
            font.pixelSize: Theme.settingsWindow.descriptionFontSize
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.settingsWindow.controlSpacing

            SettingsTextField {
                id: field
                Layout.fillWidth: true
                implicitHeight: Theme.settingsWindow.inputHeight
                placeholderText: qsTr("e.g. ~/Games/roms")
                onEditingFinished: root.refresh(text)
            }
            SettingsButton {
                text: qsTr("Browse")
                onClicked: pickerLoader.active = true
            }
        }

        Loader {
            id: pickerLoader
            active: false
            sourceComponent: RomFolderDialog {
                onPicked: path => { root.refresh(path); field.text = path }
                onClosed: pickerLoader.active = false
            }
        }

        ColumnLayout {
            visible: root.matched.length > 0
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Matched")
                color: Theme.settingsWindow.descriptionColor
                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                font.bold: true
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: matchedColumn.implicitHeight + 8
                radius: Theme.settingsWindow.inputRadius
                color: Theme.settingsWindow.inputBackground
                border.color: Theme.settingsWindow.inputBorder
                border.width: Theme.settingsWindow.inputBorderWidth

                ColumnLayout {
                    id: matchedColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 4
                    spacing: 0

                    Repeater {
                        model: root.matched

                        RowLayout {
                            id: matchRow
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            spacing: Theme.settingsWindow.controlSpacing
                            opacity: matchRow.modelData.already ? 0.5 : 1

                            SettingsCheckBox {
                                checked: matchRow.modelData.already || !root.unchecked[matchRow.modelData.path]
                                enabled: !matchRow.modelData.already
                                onToggled: root.toggle(matchRow.modelData.path)
                            }
                            Label {
                                Layout.preferredWidth: 130
                                text: matchRow.modelData.folder
                                font.family: Theme.font.monoFamily
                                font.pixelSize: Theme.settingsWindow.pathFontSize
                                color: Theme.settingsWindow.inputText
                                elide: Text.ElideRight
                            }
                            Icon { path: Icons.chevron; size: 14; color: Theme.settingsWindow.iconColor }
                            Label {
                                Layout.fillWidth: true
                                text: matchRow.modelData.platformName
                                color: Theme.settingsWindow.labelColor
                            }
                            Label {
                                visible: matchRow.modelData.already
                                text: qsTr("Already added")
                                color: Theme.settingsWindow.descriptionColor
                                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            visible: root.unmatched.length > 0
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Not matched")
                color: Theme.settingsWindow.descriptionColor
                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                font.bold: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: root.unmatched

                    Rectangle {
                        id: chip
                        required property var modelData

                        implicitWidth: chipLabel.implicitWidth + 16
                        implicitHeight: 24
                        radius: Theme.settingsWindow.inputRadius
                        color: Theme.settingsWindow.inputBackground

                        Label {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: chip.modelData.folder
                            font.family: Theme.font.monoFamily
                            font.pixelSize: Theme.settingsWindow.pathFontSize
                            color: Theme.settingsWindow.descriptionColor
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Add these by hand if they hold games.")
                color: Theme.settingsWindow.descriptionColor
                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                wrapMode: Text.WordWrap
            }
        }

        Label {
            visible: root.folder !== "" && root.matches.length === 0
            Layout.fillWidth: true
            text: qsTr("Nothing found in this folder.")
            color: Theme.settingsWindow.descriptionColor
            font.pixelSize: Theme.settingsWindow.descriptionFontSize
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.settingsWindow.controlSpacing

            SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
            SettingsButton {
                primary: true
                text: root.toAdd.length === 1 ? qsTr("Add 1 folder") : qsTr("Add %1 folders").arg(root.toAdd.length)
                enabled: root.toAdd.length > 0
                onClicked: root.apply()
            }
        }
    }
}
