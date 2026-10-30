import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property var manager: ({})
    property var choices: ({})
    property string filesSize: ""
    property string prefixSize: ""
    property string trashless: ""
    readonly property string key: "mod-manager:" + (manager.id ?? "")
    readonly property bool running: GameRunner.runningManagers.includes(manager.id ?? "")
    readonly property bool filesOffered: (choices.installDir ?? "") !== ""
    readonly property bool prefixOffered: (choices.prefix ?? "") !== ""
    readonly property bool shortcutOffered: choices.shortcut ?? false
    // From the choices, not the boxes' visibility: the boxes are out of sight while asking
    // about a folder there's no trash for.
    readonly property bool deletesFiles: filesBox.checked && filesOffered && (choices.installDirRefused ?? "") === ""
    readonly property bool deletesPrefix: prefixBox.checked && prefixOffered
    readonly property bool removesShortcut: shortcutOffered && (deletesFiles || steamBox.checked)

    signal done

    function ask(m) {
        manager = m
        filesSize = ""
        prefixSize = ""
        trashless = ""
        error.text = ""
        filesBox.checked = false
        prefixBox.checked = false
        choices = Library.modManagerUninstallChoices(m.id)
        steamBox.checked = choices.shortcut ?? false
        open()
    }

    function run(permanent) {
        const r = GameRunner.removeModManager(manager.id, deletesFiles, deletesPrefix, removesShortcut, permanent)
        if (r.trashless) {
            trashless = r.trashless
            return
        }
        trashless = ""
        if (r.error) {
            error.text = r.error
            return
        }
        close()
        done()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.uninstallWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    Connections {
        target: Library
        function onUninstallSizes(id, installDir, prefix) {
            if (id !== root.key)
                return
            root.filesSize = installDir
            root.prefixSize = prefix
        }
    }

    component Hint: Label {
        Layout.fillWidth: true
        leftPadding: Theme.controls.checkboxSize + Theme.layout.spacing
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
    }
    component PathHint: Hint {
        font.family: Theme.font.monoFamily
        font.pixelSize: Theme.gameEditor.pathFontSize
        wrapMode: Text.WrapAnywhere
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: root.trashless !== "" ? qsTr("Delete it permanently?") : qsTr("Remove %1?").arg(root.manager.name ?? "")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
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
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
                text: qsTr("It's taken out of the list. Every game's files and prefix stay.")
            }

            ColumnLayout {
                visible: root.shortcutOffered
                Layout.fillWidth: true
                spacing: 2

                SettingsCheckBox {
                    id: steamBox
                    enabled: !root.deletesFiles
                    text: qsTr("Remove its shortcut from Steam")
                }
                Hint {
                    text: root.removesShortcut
                          ? (root.deletesFiles ? qsTr("It goes with the folder: nothing would be left for it to run.")
                                               : qsTr("Its Proton setting goes too."))
                          : qsTr("Steam keeps it as it is, with its Proton setting; Settings → Steam → “Add existing "
                                 + "shortcuts…” brings it back.")
                }
            }

            ColumnLayout {
                visible: root.filesOffered
                Layout.fillWidth: true
                spacing: 2

                SettingsCheckBox {
                    id: filesBox
                    visible: (root.choices.installDirRefused ?? "") === ""
                    text: qsTr("Delete its folder")
                    onToggled: if (checked) steamBox.checked = true
                }
                PathHint {
                    text: root.choices.installDir ?? ""
                }
                Hint {
                    text: (root.choices.installDirRefused ?? "") !== ""
                          ? qsTr("It stays: %1.").arg(root.choices.installDirRefused)
                          : root.filesSize !== "" ? root.filesSize : qsTr("Adding up…")
                }
            }

            ColumnLayout {
                visible: root.prefixOffered
                Layout.fillWidth: true
                spacing: 2

                SettingsCheckBox {
                    id: prefixBox
                    text: qsTr("Delete its Wine prefix")
                }
                PathHint {
                    text: root.choices.prefix ?? ""
                }
                Hint {
                    text: (root.prefixSize !== "" ? root.prefixSize : qsTr("Adding up…")) + "  ·  "
                          + qsTr("It holds the manager's own settings.")
                }
            }

            Label {
                visible: root.deletesFiles || root.deletesPrefix
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.labelColor
                text: qsTr("What's deleted goes to the trash.")
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
        Label {
            visible: root.running
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.errorColor
            text: qsTr("%1 is running; close it first.").arg(root.manager.name ?? "")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                text: root.trashless !== "" ? qsTr("Delete permanently") : qsTr("Remove")
                danger: true
                enabled: !root.running
                onClicked: root.run(root.trashless !== "")
            }
        }
    }
}
