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
        visible: !editor.adding
        actions: SettingsButton {
            visible: editor.backupGame
            text: GameSaves.working ? qsTr("Backing up") : qsTr("Back up now")
            enabled: (editor.saves.paths ?? []).length > 0 && !GameSaves.working
            onClicked: GameSaves.backUpNow(editor.game.id)
        }

        EditorField {
            label: qsTr("Save folder")
            help: qsTr("The folder Open save folder opens. When it isn't set, it's looked for: next to "
                       + "the ROM for an emulator that saves there, the emulator's save folder, or the "
                       + "game's Proton prefix.")

            PathField {
                onCopied: editor.message(qsTr("Copied"))
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.controls.inputHeight
                path: editor.saveFolderShown !== "" ? editor.saveFolderShown : editor.foundSaveFolder
                missingText: qsTr("None found")
                missingColor: Theme.gameEditor.hintColor
                fontSize: Theme.gameEditor.pathFontSize

                SettingsButton {
                    iconPath: Icons.editLine
                    tip: qsTr("Choose the save folder")
                    onClicked: editor.folderPickerLoader.pickFor("saves")
                }
                SettingsButton {
                    visible: editor.saveFolderShown !== ""
                    iconPath: Icons.close
                    tip: qsTr("Look for the save folder")
                    onClicked: {
                        editor.newSaveFolder = ""
                        editor.saveFolderCleared = true
                    }
                }
            }
        }
        EditorHint {
            open: editor.saveFolderShown === "" && editor.foundSaveFolder !== ""
            text: qsTr("Save folder found for you. Choose another folder to open that one instead.")
        }

        EditorField {
            visible: editor.backupGame && (editor.saves.paths ?? []).length > 0
            label: qsTr("Backed up")
            help: qsTr("What a backup takes now, found from the Ludusavi manifest or from the save folder above.")

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
                text: (editor.saves.paths ?? []).length === 4 ? qsTr("and 1 more")
                    : qsTr("and %1 more").arg((editor.saves.paths ?? []).length - 3)
            }
        }
        EditorField {
            visible: editor.backupGame
            label: qsTr("Back up saves when it closes")
            help: qsTr("Saves are found from the Ludusavi manifest, or from the save folder above, and "
                       + "backed up to the folder in Settings, Library, Game saves.")

            SettingsComboBox {
                id: autoBackupBox
                Layout.fillWidth: true
                model: [qsTr("Platform default (%1)").arg(editor.game.platformAutoBackup ? qsTr("on") : qsTr("off")),
                        qsTr("On"), qsTr("Off")]
            }
        }
        SavesPanel {
            visible: editor.backupGame
            game: editor.game
            onMessage: text => editor.message(text)
            onChooseRequested: editor.chooseSavesGame()
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
