import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    carded: false
    size: "small"
    title: trashless !== "" ? qsTr("Delete it permanently?") : qsTr("Uninstall %1?").arg(game.title ?? "")

    property var game: ({})
    property var choices: ({})
    property string filesSize: ""
    property string prefixSize: ""
    property string trashless: ""
    property bool working: false
    readonly property bool running: GameRunner.running.includes(game.id ?? "")
    readonly property bool hasFiles: (choices.installDir ?? "") !== "" && (choices.installDirRefused ?? "") === ""
    readonly property bool hasPrefix: (choices.prefix ?? "") !== ""
    readonly property bool deletesPrefix: prefixBox.checked && hasPrefix

    signal done()

    function ask(g) {
        game = g
        filesSize = ""
        prefixSize = ""
        trashless = ""
        working = false
        error.text = ""
        prefixBox.checked = false
        choices = Library.epicUninstallChoices(g.id)
        open()
    }

    function run() {
        error.text = ""
        const permanent = trashless !== ""
        const e = GameRunner.uninstallEpic(game.id, hasFiles && !permanent, deletesPrefix, permanent)
        if (e !== "")
            error.text = e
        else
            working = true
    }

    width: Math.min(Theme.gameEditor.uninstallWidth, parent.width - 2 * Theme.gameEditor.padding)
    closePolicy: working ? Popup.NoAutoClose : Popup.CloseOnEscape

    onOpened: cancelButton.forceActiveFocus()

    Connections {
        target: Library
        function onUninstallSizes(id, installDir, prefix) {
            if (id !== root.game.id)
                return
            root.filesSize = installDir
            root.prefixSize = prefix
        }
    }
    Connections {
        target: GameRunner
        function onEpicUninstalled(id, result) {
            if (id !== root.game.id)
                return
            root.working = false
            if (result.trashless) {
                root.trashless = result.trashless
            } else if (result.error) {
                error.text = result.error
            } else {
                root.close()
                root.done()
            }
        }
    }

    component SizeLabel: Label {
        property string size

        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
        font.features: { "tnum": 1 }
        text: size !== "" ? size : qsTr("Adding up…")
    }

    Label {
        visible: root.trashless !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("There's no trash for %1, so it can only be deleted for good. This can't be undone.")
                  .arg(root.trashless)
    }

    ColumnLayout {
        visible: root.trashless === ""
        Layout.fillWidth: true
        spacing: Theme.gameEditor.rowSpacing

        Label {
            visible: !root.hasFiles
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: (root.choices.installDirRefused ?? "") !== ""
                  ? qsTr("The game's files stay: %1.").arg(root.choices.installDirRefused)
                  : qsTr("The game's files are gone already.")
        }

        ColumnLayout {
            visible: root.hasFiles
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Item {
                    implicitWidth: Theme.controls.checkboxSize
                    implicitHeight: Theme.controls.checkboxSize

                    Icon {
                        anchors.centerIn: parent
                        path: Icons.deleteBin6Line
                        size: Theme.controls.checkboxSize
                        color: Theme.gameEditor.dangerColor
                    }
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: Theme.controls.labelColor
                    font.pixelSize: Theme.controls.labelFontSize
                    text: qsTr("Game files")
                }
                SizeLabel {
                    size: root.filesSize
                }
            }
            DialogPathHint {
                text: root.choices.installDir ?? ""
            }
            DialogHint {
                color: GameRunner.legendaryFound() ? Theme.gameEditor.dangerColor : Theme.gameEditor.errorColor
                text: GameRunner.legendaryFound() ? qsTr("Deleted for good")
                                                  : qsTr("Legendary isn't found; uninstall the game in Heroic.")
            }
        }

        ColumnLayout {
            visible: root.hasPrefix
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                SettingsCheckBox {
                    id: prefixBox
                    Layout.fillWidth: true
                    text: qsTr("Wine prefix")
                }
                SizeLabel {
                    size: root.prefixSize
                }
            }
            DialogPathHint {
                text: root.choices.prefix ?? ""
            }
            DialogHint {
                text: qsTr("Saves and settings · moved to the trash")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.gameEditor.rowSpacing / 2
            implicitHeight: Theme.layout.borderWidth > 0 ? 1 : 0
            color: Theme.gameEditor.divider
        }

        DialogHint {
            indented: false
            text: qsTr("The game stays in your library as not installed, with its play time, tags and art.")
        }
    }

    Label {
        id: error
        visible: text !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.errorColor
        text: ""
    }
    PlainLabel {
        visible: root.running
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.errorColor
        text: qsTr("%1 is running; close it first.").arg(root.game.title ?? "")
    }

    footer: [
        SettingsButton {
            id: cancelButton
            text: qsTr("Cancel")
            enabled: !root.working
            onClicked: root.close()
        },
        SettingsButton {
            text: root.working ? qsTr("Uninstalling…")
                : root.trashless !== "" ? qsTr("Delete permanently") : qsTr("Uninstall")
            danger: true
            enabled: !root.running && !root.working && (root.hasFiles ? GameRunner.legendaryFound() : root.deletesPrefix)
            onClicked: root.run()
        }
    ]
}
