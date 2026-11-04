import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A tab of GameEditor; the editor keeps the state and reads these controls.
ColumnLayout {
    required property var editor
    property alias autoBackupBox: autoBackupBox

    Layout.fillWidth: true
    Layout.fillHeight: false // as tall as its cards; layouts fill by default
    Layout.alignment: Qt.AlignTop
    spacing: Theme.gameEditor.cardSpacing

    EditorCard {
        title: qsTr("Saves")
        visible: editor.backupGame

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: editor.backups.length > 0 ? qsTr("Last backup %1").arg(editor.backups[0].when.toLowerCase())
                                                  : qsTr("No backups yet")
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: GameSaves.working ? qsTr("Backing up")
                        : GameSaves.message !== "" ? GameSaves.message
                        : editor.backups.length > 0 ? qsTr("%n backup(s), %1", "", editor.backups.length).arg(editor.backups[0].size)
                        : qsTr("Made when the game closes, or with Back up now.")
                    color: Theme.gameEditor.hintColor
                    font.pixelSize: Theme.gameEditor.hintFontSize
                    elide: Text.ElideRight
                }
            }
            SettingsButton {
                text: qsTr("Manage backups")
                flat: true
                onClicked: editor.openSaves()
            }
            SettingsButton {
                text: qsTr("Back up now")
                primary: true
                enabled: (editor.saves.paths ?? []).length > 0 && !GameSaves.working
                onClicked: GameSaves.backUpNow(editor.game.id)
            }
        }
        EditorField {
            visible: (editor.saves.paths ?? []).length > 0
            label: (editor.saves.paths ?? []).length > 1 ? qsTr("Save files") : qsTr("Save folder")
            help: qsTr("What a backup takes now, found from the Ludusavi manifest or from the save folder on the Launch tab.")

            Repeater {
                model: (editor.saves.paths ?? []).slice(0, 3)

                PathField {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.minimumHeight: Theme.controls.inputHeight
                    path: modelData.startsWith("~/") ? editor.homePath + modelData.slice(1) : modelData
                    fontSize: Theme.gameEditor.pathFontSize
                    onCopied: editor.message(qsTr("Copied"))
                }
            }
            EditorHint {
                open: (editor.saves.paths ?? []).length > 3
                text: qsTr("and %n more", "", (editor.saves.paths ?? []).length - 3)
            }
        }
        EditorField {
            label: qsTr("Back up saves when it closes")
            help: qsTr("Saves are found from the Ludusavi manifest, or from the save folder on the Launch tab, and "
                       + "backed up to the folder in Settings, Library, Game saves.")

            SettingsComboBox {
                id: autoBackupBox
                Layout.fillWidth: true
                model: [qsTr("Platform default (%1)").arg(editor.game.platformAutoBackup ? qsTr("on") : qsTr("off")),
                        qsTr("On"), qsTr("Off")]
            }
        }
    }

    EditorCard {
        title: qsTr("Mod managers")
        visible: editor.managerGame
        actions: Row {
            spacing: Theme.layout.spacing

            EditorTextAction {
                id: linkAction
                visible: editor.unlinkedManagers.length > 0
                text: qsTr("Link")
                iconPath: Icons.link
                tip: qsTr("Link a mod manager you already added")
                onClicked: editor.managerMenuItem.popup(linkAction, 0, linkAction.height)
            }
            EditorTextAction {
                text: qsTr("Add")
                iconPath: Icons.add
                tip: qsTr("Add a mod manager for this game")
                onClicked: editor.addModManagerRequested(editor.game.id)
            }
        }

        Label {
            visible: editor.linkedManagers.length === 0
            Layout.fillWidth: true
            text: qsTr("No mod manager is linked to this game.")
            color: Theme.gameEditor.hintColor
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: editor.linkedManagers

            RowLayout {
                id: managerRow
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing

                Icon {
                    path: Icons.puzzle
                    size: Theme.controls.iconSize
                    color: Theme.controls.inputText
                }
                Label {
                    Layout.fillWidth: true
                    text: managerRow.modelData.name
                    color: Theme.controls.inputText
                    elide: Text.ElideRight
                }
                Label {
                    text: managerRow.modelData.proton ? qsTr("Proton") : qsTr("Native")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                }
                ListButton {
                    flat: true
                    iconPath: Icons.close
                    textColor: hovered ? foreground : Theme.controls.descriptionColor
                    tip: qsTr("Unlink from this game")
                    onClicked: editor.setManagerLinked(managerRow.modelData.id, false)
                }
            }
        }
    }
}
