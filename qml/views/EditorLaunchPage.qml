import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components/Args.js" as Args

// A tab of GameEditor; the editor keeps the state and reads these controls.
ColumnLayout {
    required property var editor
    property alias platformBox: platformBox
    property alias emulatorBox: emulatorBox
    property alias argsField: argsField
    property alias protonBox: protonBox
    property alias launchOptionsField: launchOptionsField
    property alias makeExecutableBox: makeExecutableBox

    Layout.fillWidth: true
    Layout.fillHeight: false // as tall as its cards; layouts fill by default
    Layout.alignment: Qt.AlignTop
    spacing: Theme.gameEditor.cardSpacing

    EditorCard {
        title: qsTr("Install")

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gameEditor.columnSpacing

            EditorField {
                Layout.fillWidth: false
                Layout.preferredWidth: platformBox.visible ? Theme.gameEditor.platformFieldWidth : -1
                Layout.alignment: Qt.AlignTop
                label: qsTr("Platform")

                RowLayout {
                    visible: !platformBox.visible
                    Layout.fillWidth: true
                    Layout.minimumHeight: Theme.controls.inputHeight
                    spacing: Theme.layout.spacing

                    Icon {
                        path: Icons.forPlatform(editor.adding ? (editor.platformChoices[platformBox.currentIndex]?.icon ?? "")
                                                            : (editor.game.platformIcon ?? ""))
                        size: Theme.controls.iconSize
                        color: Theme.controls.inputText
                    }
                    Label {
                        Layout.fillWidth: true
                        text: editor.adding ? (editor.platformChoices[platformBox.currentIndex]?.name ?? "")
                                          : (editor.game.platformName ?? "")
                        color: Theme.controls.inputText
                        font.pixelSize: Theme.gameEditor.platformFontSize
                        elide: Text.ElideRight
                    }
                }
                SettingsComboBox {
                    id: platformBox
                    visible: editor.adding && editor.pick.platforms.length !== 1
                    Layout.fillWidth: true
                    model: editor.platformChoices.map(c => c.name)
                    displayText: currentIndex < 0 ? qsTr("Choose a platform") : currentText
                    onActivated: {
                        editor.errorLabel.text = ""
                        emulatorBox.currentIndex = 0
                    }
                }
            }

            EditorField {
                visible: editor.romGame
                Layout.alignment: Qt.AlignTop
                label: qsTr("ROM")

                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumHeight: Theme.controls.inputHeight
                    spacing: Theme.layout.spacing

                    PathField {
                        onCopied: editor.message(qsTr("Copied"))
                        Layout.fillWidth: true
                        path: editor.romShown
                        missingText: qsTr("ROM not found")
                        missingColor: Theme.gameEditor.warningColor
                        fontSize: Theme.gameEditor.pathFontSize
                    }
                    SettingsButton {
                        visible: !editor.adding && ((editor.game.romKey ?? false) || editor.locating)
                        text: editor.locating ? qsTr("Locate") : qsTr("Change")
                        onClicked: editor.romPickerLoader.active = true
                    }
                }
            }

            EditorField {
                visible: !editor.romGame
                Layout.alignment: Qt.AlignTop
                label: qsTr("Install location")
                help: editor.programGame
                      ? qsTr("The game's whole folder, with the program somewhere inside. Open folder "
                             + "opens it, and the game's size is this folder's.")
                      : qsTr("Where Steam installed the game.")

                PathField {
                    id: installField
                    onCopied: editor.message(qsTr("Copied"))
                    Layout.fillWidth: true
                    Layout.minimumHeight: Theme.controls.inputHeight
                    path: editor.newInstallDir !== "" ? editor.newInstallDir : (editor.game.installPath ?? "")
                    missingText: editor.programGame ? qsTr("Not set") : qsTr("Not installed")
                    missingColor: Theme.gameEditor.hintColor
                    fontSize: Theme.gameEditor.pathFontSize

                    SettingsButton {
                        visible: !editor.adding && installField.path !== "" && editor.newInstallDir === ""
                        iconPath: Icons.folder
                        tip: qsTr("Open the install folder")
                        onClicked: editor.openFolder()
                    }
                    SettingsButton {
                        visible: editor.programGame
                        iconPath: Icons.editLine
                        tip: qsTr("Change the install folder")
                        onClicked: editor.folderPickerLoader.pickFor("install")
                    }
                }
            }
        }

        EditorField {
            visible: editor.programGame
            label: qsTr("Program")
            help: editor.linuxGame
                  ? qsTr("The program or script the launcher runs, with the launch options below. "
                         + "It counts as running until every process it started has ended.")
                  : qsTr("The .exe Steam runs, the target of the game's shortcut in Steam. Changing it "
                         + "here changes the shortcut, and the other way around.")

            PathField {
                onCopied: editor.message(qsTr("Copied"))
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.controls.inputHeight
                path: editor.exeShown
                missingText: qsTr("Not set")
                missingColor: Theme.gameEditor.warningColor
                fontSize: Theme.gameEditor.pathFontSize

                SettingsButton {
                    visible: !editor.adding
                    readonly property bool locating: editor.newExe === ""
                        && ((editor.game.gameNotFound ?? false) || (editor.game.uninstalled ?? false))
                    text: locating ? qsTr("Locate") : ""
                    iconPath: locating ? "" : Icons.editLine
                    tip: locating ? "" : qsTr("Change the program")
                    onClicked: editor.exePickerLoader.active = true
                }
            }
            EditorHint {
                open: editor.newExe === "" && (editor.game.gameNotFound ?? false)
                color: Theme.gameEditor.warningColor
                text: editor.linuxGame
                      ? qsTr("Game not found: there's no program there, or it isn't executable. "
                             + "Locate it if the game moved; picking it makes it executable.")
                      : qsTr("Game not found: there's no program there, so Steam can't start it. "
                             + "Locate it if the game moved.")
            }
            EditorHint {
                open: editor.newExe === "" && (editor.game.uninstalled ?? false)
                text: editor.linuxGame
                      ? qsTr("Uninstalled: its files were deleted. Locating the program brings it "
                             + "back, with the same play time.")
                      : qsTr("Uninstalled: it has no shortcut in Steam. Locating the program adds one "
                             + "again, with the same prefix and play time.")
            }
            SettingsCheckBox {
                id: makeExecutableBox
                visible: editor.addingLinux && !(editor.pick.executable ?? true)
                text: qsTr("Make it executable")
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("It isn't marked as a program yet (chmod +x); without that, "
                                   + "it can't be launched")
            }
            EditorTextAction {
                visible: editor.startDirShown === ""
                text: qsTr("Start in another folder")
                iconPath: Icons.folder
                tip: editor.linuxGame ? qsTr("The folder the program starts in: by default, its own")
                                    : qsTr("Steam's \"Start in\": by default, the program's own folder")
                onClicked: editor.folderPickerLoader.pickFor("start")
            }
        }
        EditorField {
            visible: editor.programGame && editor.startDirShown !== ""
            label: qsTr("Start in")

            PathField {
                onCopied: editor.message(qsTr("Copied"))
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.controls.inputHeight
                path: editor.startDirShown
                fontSize: Theme.gameEditor.pathFontSize

                SettingsButton {
                    iconPath: Icons.editLine
                    tip: qsTr("Change the start folder")
                    onClicked: editor.folderPickerLoader.pickFor("start")
                }
                SettingsButton {
                    iconPath: Icons.close
                    tip: qsTr("Start in the program's own folder")
                    onClicked: {
                        editor.newStartDir = ""
                        editor.startDirCleared = true
                    }
                }
            }
        }
        EditorHint {
            open: editor.adding && editor.pick.platforms.length === 0
            text: qsTr("None of your platforms' emulators run %1 files. They take:")
                      .arg(editor.pick.fileName?.includes(".") ? "." + editor.pick.fileName.split(".").pop()
                                                               : editor.pick.fileName ?? "")
                  + editor.platformChoices.map(c => "\n" + c.name + ": " + c.takes).join("")
        }
        EditorHint {
            open: editor.newRomOwner !== null
            text: qsTr("This file is also %1 in your library. Saving merges it into this game: its play "
                       + "time, favorite, tags and missing art come over, and it's removed.")
                      .arg(editor.newRomOwner?.title ?? "")
        }
    }

    EditorCard {
        title: qsTr("Run")
        visible: editor.romGame

        EditorField {
            label: qsTr("Emulator")

            SettingsComboBox {
                id: emulatorBox
                Layout.fillWidth: true
                model: [qsTr("Platform default (%1)").arg(editor.emulators.defaultEmulator || qsTr("none"))]
                       .concat(editor.emulators.choices.map(c => c.installed ? c.name
                                                                           : qsTr("%1 (not installed)").arg(c.name)))
            }
            EditorHint {
                readonly property string before: editor.emulators.choices.find(c => c.id === editor.game.emulator)?.name
                                                 ?? editor.emulators.defaultEmulator
                readonly property string after: editor.emulator?.name ?? editor.emulators.defaultEmulator
                open: (editor.game.playMinutes ?? 0) > 0 && before !== after
                color: Theme.gameEditor.warningColor
                text: qsTr("Saves stay in each emulator's own folders; your progress in %1 won't be in %2.")
                          .arg(before).arg(after)
            }
        }

        EditorField {
            label: qsTr("Launch arguments")

            SettingsTextField {
                id: argsField
                Layout.fillWidth: true
                placeholderText: qsTr("None")
                onAccepted: editor.save()
            }
            EditorHint {
                readonly property var shown: editor.emulator ?? editor.emulators.choices.find(
                                                 c => c.name === editor.emulators.defaultEmulator) ?? null
                open: shown !== null
                text: shown === null ? ""
                    : shown.args.length > 0 ? qsTr("Added after %1's own: %2").arg(shown.name).arg(Args.join(shown.args))
                    : qsTr("%1 has no arguments of its own").arg(shown.name)
            }
        }
    }

    EditorCard {
        title: qsTr("Run")
        visible: !editor.romGame

        EditorField {
            visible: editor.onSteam
            label: qsTr("Proton version")
            help: editor.protonGame
                  ? qsTr("The compatibility tool Steam runs the program with, as in its Properties. "
                         + "None for a Linux program.")
                  : qsTr("The compatibility tool that runs the game, as in Steam's Properties.")

            SettingsComboBox {
                id: protonBox
                Layout.fillWidth: true
                model: editor.protonChoices.map(c => c.displayName)
            }
        }

        EditorField {
            visible: (editor.game.steamPresets ?? []).length > 0
            label: qsTr("Presets")

            Flow {
                Layout.fillWidth: true
                spacing: Theme.gameEditor.presetSpacing

                Repeater {
                    model: editor.game.steamPresets ?? []

                    SettingsCheckBox {
                        id: presetBox
                        required property var modelData
                        text: modelData.name
                        checked: Library.steamPresetOn(launchOptionsField.text, modelData.id)
                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("Adds %1").arg(modelData.adds)
                        onToggled: {
                            launchOptionsField.text = Library.setSteamPreset(launchOptionsField.text,
                                                                             modelData.id, checked)
                            // Clicking broke the binding; the text says what's on.
                            checked = Qt.binding(() => Library.steamPresetOn(launchOptionsField.text,
                                                                             presetBox.modelData.id))
                        }
                    }
                }
            }
        }

        EditorField {
            label: qsTr("Launch options")

            SettingsTextField {
                id: launchOptionsField
                Layout.fillWidth: true
                placeholderText: qsTr("None")
                onAccepted: editor.save()
            }
            EditorHint {
                text: editor.linuxGame
                      ? qsTr("Written like Steam's: variables, then a wrapper before %command%, "
                             + "arguments after it, e.g. mangohud %command% --fullscreen")
                      : qsTr("Same as Steam's launch options, e.g. mangohud %command%")
            }
        }

        Unfold {
            open: editor.onSteam && (editor.adding ? editor.steamRunning
                                               : editor.steamPending || (editor.steamEdited && editor.steamRunning))
            visible: open || progress > 0
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    text: editor.adding ? qsTr("Steam is running: the game is added to it when it closes")
                        : editor.steamEdited ? qsTr("Steam is running: saving applies this when it closes")
                        : editor.shortcutQueued ? qsTr("Waiting for Steam to close to add this game to it")
                        : qsTr("Waiting for Steam to close to apply this")
                    color: Theme.gameEditor.pendingColor
                    font.pixelSize: Theme.gameEditor.hintFontSize
                    wrapMode: Text.WordWrap
                }
                SettingsButton {
                    text: GameRunner.closingSteam ? qsTr("Closing Steam") : qsTr("Close Steam and apply")
                    enabled: !GameRunner.closingSteam
                    tip: qsTr("Saves, closes Steam, writes the changes and starts Steam again")
                    onClicked: editor.closeSteamAndApply()
                }
            }
        }
    }
}
