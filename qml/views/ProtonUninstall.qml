import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property var game: ({})
    property bool scrim: false
    property var choices: ({})
    property string filesSize: ""
    property string prefixSize: ""
    property string trashless: ""
    readonly property bool running: GameRunner.running.includes(game.id ?? "")
    // From the choices, not the boxes' visibility: the boxes are out of sight while asking
    // about a folder there's no trash for.
    readonly property bool deletesFiles: filesBox.checked && (choices.installDir ?? "") !== ""
                                         && (choices.installDirRefused ?? "") === ""
    readonly property bool deletesPrefix: prefixBox.checked && (choices.prefix ?? "") !== ""
    readonly property bool linux: game.platform === "linux"
    readonly property bool shortcutOffered: !linux && !(game.uninstalled ?? false)
    // Deleting the files takes the shortcut too: nothing would be left for it to run.
    readonly property bool removesShortcut: shortcutOffered && (deletesFiles || steamBox.checked)
    readonly property bool removes: removeRadio.checked
    readonly property string action: removes ? "remove" : hideRadio.checked ? "hide" : "keep"
    readonly property string currentAction: (game.hidden ?? false) ? "hide" : "keep"
    readonly property bool nothingToDo: !deletesFiles && !deletesPrefix && !removesShortcut && action === currentAction

    signal done(bool removed)

    function ask(g) {
        game = g
        filesSize = ""
        prefixSize = ""
        trashless = ""
        error.text = ""
        filesBox.checked = false
        prefixBox.checked = false
        steamBox.checked = false
        keepRadio.checked = !(g.hidden ?? false)
        hideRadio.checked = g.hidden ?? false
        choices = Library.uninstallChoices(g.id)
        open()
    }

    function run(permanent) {
        const r = GameRunner.uninstallProton(game.id, deletesFiles, deletesPrefix, removesShortcut, action, permanent)
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
        done(removes)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.uninstallWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Scrim { color: root.scrim ? Theme.gameEditor.scrim : "transparent" }
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
            if (id !== root.game.id)
                return
            root.filesSize = installDir
            root.prefixSize = prefix
        }
    }

    component Hint: Label {
        Layout.fillWidth: true
        leftPadding: Theme.settingsWindow.checkboxSize + Theme.layout.spacing
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
    }
    component Card: Surface {
        default property alias fields: cardColumn.data

        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + 2 * Theme.gameEditor.cardPadding
        radius: Theme.gameEditor.cardRadius
        fill: Theme.gameEditor.cardBackground
        fillEnd: Theme.gameEditor.cardBackgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.cardBorder
        border.width: Theme.gameEditor.cardBorderWidth

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: Theme.gameEditor.cardPadding
            spacing: Theme.gameEditor.rowSpacing
        }
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
            text: root.trashless !== "" ? qsTr("Delete it permanently?") : qsTr("Uninstall %1?").arg(root.game.title ?? "")
            color: Theme.gameEditor.headingColor
            font.bold: true
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

            Card {
                visible: (root.choices.installDir ?? "") !== "" || (root.choices.prefix ?? "") !== ""
                         || root.shortcutOffered

                RowLayout {
                    visible: (root.choices.installDir ?? "") !== "" || root.shortcutOffered
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    ColumnLayout {
                        visible: (root.choices.installDir ?? "") !== ""
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        spacing: 2

                        SettingsCheckBox {
                            id: filesBox
                            visible: (root.choices.installDirRefused ?? "") === ""
                            text: qsTr("Delete the game's files")
                            onToggled: if (checked) steamBox.checked = true
                        }
                        PathHint {
                            text: root.choices.installDir ?? ""
                        }
                        Hint {
                            text: (root.choices.installDirRefused ?? "") !== ""
                                  ? qsTr("They stay: %1.").arg(root.choices.installDirRefused)
                                  : root.filesSize !== "" ? root.filesSize : qsTr("Adding up…")
                        }
                        // A Linux game's saves are its own business: often in ~/.local/share or ~/.config.
                        Hint {
                            visible: root.linux && root.deletesFiles
                            text: root.removes
                                  ? qsTr("Saves the game keeps elsewhere, like in ~/.local/share, stay.")
                                  : qsTr("Kept, it shows as not installed; locating its program brings it back. "
                                         + "Saves the game keeps elsewhere, like in ~/.local/share, stay.")
                        }
                    }

                    ColumnLayout {
                        visible: root.shortcutOffered
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        spacing: 2

                        SettingsCheckBox {
                            id: steamBox
                            enabled: !root.deletesFiles
                            text: qsTr("Remove its shortcut from Steam")
                        }
                        Hint {
                            text: {
                                if (root.deletesFiles)
                                    return qsTr("It goes with the game's files: nothing would be left for it to run.")
                                if (root.removesShortcut)
                                    return root.removes ? qsTr("Its Proton setting goes too.")
                                                        : qsTr("The game stays as not installed; locating its program "
                                                               + "adds the shortcut again.")
                                return root.removes ? qsTr("Steam keeps it as it is, with its art and Proton setting; "
                                                           + "Settings → Steam → “Add existing shortcuts…” brings "
                                                           + "it back.")
                                                    : qsTr("It stays in Steam as it is.")
                            }
                        }
                    }
                }

                ColumnLayout {
                    visible: (root.choices.prefix ?? "") !== ""
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
                              + qsTr("It holds the game's saves and settings.")
                              + (root.deletesPrefix && !root.removesShortcut
                                 ? " " + qsTr("Steam makes a new one the next time the game starts.") : "")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.gameEditor.rowSpacing
                spacing: Theme.layout.spacing

                Label {
                    Layout.alignment: Qt.AlignBaseline
                    text: qsTr("In the library")
                    color: Theme.gameEditor.headingColor
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBaseline
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.WordWrap
                    color: Theme.gameEditor.hintColor
                    font.pixelSize: Theme.gameEditor.hintFontSize
                    text: root.removes ? qsTr("Its game file and art go")
                        : root.action === "hide" ? qsTr("Stays hidden, with its play time, tags and art")
                        : qsTr("Stays in the list, with its play time, tags and art")
                }
            }
            Card {
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    SettingsRadioButton {
                        id: keepRadio
                        text: qsTr("Keep it")
                    }
                    SettingsRadioButton {
                        id: hideRadio
                        text: qsTr("Keep it, hidden")
                    }
                    SettingsRadioButton {
                        id: removeRadio
                        text: qsTr("Remove it from the library")
                    }
                }
            }

            Label {
                visible: filesBox.checked || prefixBox.checked
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
            text: qsTr("%1 is running; close it first.").arg(root.game.title ?? "")
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
                text: root.trashless !== "" ? qsTr("Delete permanently")
                    : root.removes ? qsTr("Remove")
                    : root.deletesFiles || (root.removesShortcut && root.deletesPrefix) ? qsTr("Uninstall")
                    : root.removesShortcut ? qsTr("Remove from Steam")
                    : root.deletesPrefix ? qsTr("Delete prefix")
                    : root.nothingToDo ? qsTr("Uninstall")
                    : root.action === "hide" ? qsTr("Hide") : qsTr("Unhide")
                danger: root.removes || root.deletesFiles || root.deletesPrefix || root.removesShortcut
                primary: !danger
                enabled: !root.running && !root.nothingToDo
                onClicked: root.run(root.trashless !== "")
            }
        }
    }
}
