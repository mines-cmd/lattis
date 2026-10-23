import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var emulator
    property string title: qsTr("Program")

    signal saved(string error)

    spacing: 6

    function save(value) {
        root.saved(Library.setEmulatorValue(root.emulator.id, "path", value))
    }

    SectionTitle { text: root.title }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.controls.controlSpacing

        SettingsTextField {
            id: pathField
            Layout.fillWidth: true
            implicitHeight: Theme.controls.inputHeight
            text: root.emulator.path
            placeholderText: root.emulator.found && root.emulator.install !== "" ? root.emulator.install
                           : root.emulator.found ? qsTr("Found automatically")
                                                 : qsTr("Not found; choose the program")
            rightPadding: copyPath.visible ? copyPath.width : leftPadding
            onEditingFinished: if (text !== root.emulator.path) root.save(text)
            // A new value from the file: show it unless it's being typed over.
            Connections {
                target: root
                function onEmulatorChanged() { if (!pathField.activeFocus) pathField.text = root.emulator.path }
            }

            AbstractButton {
                id: copyPath
                readonly property string value: pathField.text !== "" ? pathField.text
                                                : root.emulator.found ? root.emulator.install : ""
                property bool copied: false

                visible: value !== ""
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: height
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                background: Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(1, Theme.controls.inputBorderWidth)
                    height: parent.height / 2
                    color: Theme.controls.inputBorder
                }
                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        path: copyPath.copied ? Icons.check : Icons.copy
                        size: Theme.controls.iconSize
                        color: copyPath.hovered ? Theme.controls.inputText : Theme.controls.descriptionColor
                    }
                }
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: copied ? qsTr("Copied") : qsTr("Copy the full path")
                HoverHandler { cursorShape: Qt.PointingHandCursor }

                TextEdit { id: clipboard; visible: false }
                Timer { id: copiedReset; interval: 1500; onTriggered: copyPath.copied = false }
                onClicked: {
                    clipboard.text = value
                    clipboard.selectAll()
                    clipboard.copy()
                    copied = true
                    copiedReset.restart()
                }
            }
        }
        SettingsButton {
            text: qsTr("Browse")
            tip: qsTr("Choose the emulator's program or AppImage")
            onClicked: pathDialog.active = true

            // Here rather than in the layouts, where an empty Loader would still take a spacing.
            Loader {
                id: pathDialog
                active: false
                sourceComponent: ProgramDialog {
                    title: qsTr("Choose %1's program").arg(root.emulator.name)
                    onPicked: path => root.save(path)
                    onClosed: pathDialog.active = false
                }
            }
        }
        SettingsButton {
            visible: root.emulator.path !== ""
            text: qsTr("Detect automatically")
            tip: qsTr("Forget this path and look for the Flatpak, the program or an AppImage")
            onClicked: root.save("")
        }
    }
    Label {
        readonly property bool noProgram: !root.emulator.preset && !root.emulator.found && root.emulator.path === ""
                                          && root.emulator.flatpak === ""
        visible: root.emulator.pathMissing || noProgram
        Layout.fillWidth: true
        text: noProgram ? qsTr("Choose its program, or give its Flatpak app id below.")
            : root.emulator.found ? qsTr("Nothing is at this path, so the one found automatically is used.")
                                  : qsTr("Nothing is at this path.")
        color: Theme.settingsWindow.installMissing
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }
}
