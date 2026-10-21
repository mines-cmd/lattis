import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Import the games installed in Steam, and choose what new ones start with in their launch options.")

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {}
    function clearSearch() { return false }

    signal message(string text)

    readonly property var steam: Library.steam

    function set(key, value) {
        error.text = Library.setSteamValue(key, value)
    }

    function reveal(item) {
        if (item === "shortcuts")
            openPicker()
        else if (item === "queue")
            Qt.callLater(() => flick.contentY = Math.max(0, Math.min(queueHeader.y, flick.contentHeight - flick.height)))
    }

    function openPicker() {
        picker.active = true
        picker.item.openFresh()
    }

    Loader {
        id: picker
        active: false
        sourceComponent: ShortcutPicker {}
    }

    function setDefault(id, on) {
        const ids = steam.defaultPresets.filter(p => p !== id)
        if (on)
            ids.push(id)
        // In the presets' order, so the file doesn't depend on the clicks'.
        set("default_presets", steam.presets.map(p => p.id).filter(p => ids.includes(p)))
    }

    component SectionHeader: Label {
        Layout.topMargin: Theme.settingsWindow.sectionSpacing / 2
        Layout.bottomMargin: Theme.layout.spacing
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.bold: true
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.settingsWindow.rowDivider
    }

    component Field: ColumnLayout {
        id: field
        property alias label: name.text
        property bool marked: false
        property string markerText: qsTr("found automatically")

        Layout.fillWidth: true
        Layout.topMargin: Theme.layout.spacing
        spacing: Theme.layout.spacing / 2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                id: name
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
            }
            Label {
                visible: field.marked
                Layout.fillWidth: true
                text: field.markerText
                color: Theme.settingsWindow.markerColor
                font.pixelSize: Theme.settingsWindow.markerFontSize
                elide: Text.ElideRight
            }
            Item {
                visible: !field.marked
                Layout.fillWidth: true
            }
        }
    }

    Loader {
        id: folderDialog
        active: false
        sourceComponent: RomFolderDialog {
            onPicked: path => root.set("path", path)
            onClosed: folderDialog.active = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: Theme.layout.spacing

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            color: Theme.controls.problemsText
            font.pixelSize: Theme.controls.descriptionFontSize
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: content
                width: flick.width - scrollBar.width - Theme.layout.spacing
                spacing: 0

                SectionHeader { text: qsTr("Steam") }
                SettingsNote {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("Steam's installed games show up in the library by themselves, launch through Steam, "
                               + "and keep the play time Steam records.")
                }
                SettingsCard {
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Import Steam games")
                            color: Theme.controls.labelColor
                            font.pixelSize: Theme.controls.labelFontSize
                        }
                        SettingsSwitch {
                            checked: root.steam.enabled
                            onToggled: root.set("enabled", checked)
                        }
                    }
                    SettingsNote {
                        Layout.fillWidth: true
                        text: root.steam.status
                        color: root.steam.problem ? Theme.controls.problemsText : Theme.controls.descriptionColor
                    }

                    Field {
                        label: qsTr("Folder")
                        marked: root.steam.found && root.steam.path === ""
                        enabled: root.steam.enabled

                        PathField {
                            Layout.fillWidth: true
                            path: root.steam.found ? root.steam.root : root.steam.path
                            onCopied: root.message(qsTr("Copied"))
                            missingText: root.steam.enabled ? qsTr("Not found") : qsTr("Not looked for")
                            missingColor: root.steam.problem ? Theme.controls.problemsText
                                                             : Theme.controls.descriptionColor

                            SettingsButton {
                                visible: root.steam.path !== ""
                                text: qsTr("Find automatically")
                                tip: qsTr("Look in ~/.steam/steam, then the Flatpak's folder")
                                onClicked: root.set("path", "")
                            }
                            SettingsButton {
                                text: qsTr("Change")
                                tip: qsTr("Choose Steam's folder: the one with steamapps in it")
                                onClicked: folderDialog.active = true
                            }
                        }
                    }

                    Field {
                        label: qsTr("Account")
                        enabled: root.steam.found && root.steam.users.length > 0

                        SteamAccountBox {
                            Layout.fillWidth: true
                            onFailed: message => error.text = message
                        }
                    }
                    SettingsNote {
                        Layout.fillWidth: true
                        text: qsTr("Play time, launch options and custom art are per account.")
                    }
                }

                SectionHeader { text: qsTr("Non-Steam games") }
                SettingsNote {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("Windows games Steam runs with Proton, as non-Steam shortcuts. They launch through "
                               + "Steam and keep the play time it records.")
                }
                SettingsCard {
                    enabled: root.steam.found

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing
                        Label {
                            Layout.fillWidth: true
                            text: !root.steam.found ? qsTr("Steam isn't found.")
                                  : !root.steam.shortcutsOk ? qsTr("Steam's list of non-Steam games couldn't be read.")
                                  : root.steam.shortcuts === 0 ? qsTr("Steam has no non-Steam games yet.")
                                  : qsTr("Steam has %n non-Steam game(s), %1 in the library.", "", root.steam.shortcuts)
                                        .arg(root.steam.shortcutsInLibrary)
                            color: Theme.controls.labelColor
                            font.pixelSize: Theme.controls.labelFontSize
                            wrapMode: Text.WordWrap
                        }
                        SettingsButton {
                            text: qsTr("Add existing shortcuts")
                            enabled: root.steam.shortcuts > root.steam.shortcutsInLibrary
                            tip: qsTr("Choose which of Steam's non-Steam games to bring into the library")
                            onClicked: root.openPicker()
                        }
                    }

                    Field {
                        label: qsTr("Proton version for new games")

                        SettingsComboBox {
                            Layout.fillWidth: true
                            readonly property var names: {
                                const n = [""].concat(root.steam.tools.map(t => t.name))
                                if (root.steam.defaultProton !== "" && !n.includes(root.steam.defaultProton))
                                    n.push(root.steam.defaultProton)
                                return n
                            }
                            model: names.map((name, i) => i === 0
                                             ? qsTr("Steam's default (%1)").arg(root.steam.steamDefaultProton)
                                             : root.steam.tools.find(t => t.name === name)?.displayName
                                               ?? qsTr("%1 (not installed)").arg(name))
                            currentIndex: Math.max(0, names.indexOf(root.steam.defaultProton))
                            onActivated: index => root.set("default_proton", names[index])
                        }
                    }
                }

                // Steam overwrites its files when it exits, so edits made while it runs wait.
                SectionHeader {
                    id: queueHeader
                    visible: root.steam.queue.length > 0
                    text: qsTr("Waiting for Steam")
                }
                SettingsNote {
                    visible: root.steam.queue.length > 0
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("Steam takes these only while it's closed, so they're applied when it next closes.")
                }
                SettingsCard {
                    visible: root.steam.queue.length > 0
                    Repeater {
                        model: root.steam.queue
                        ColumnLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 2

                            Divider {
                                visible: index > 0 && Theme.settingsWindow.rowDividers
                                Layout.leftMargin: -Theme.settingsWindow.rowPadding
                                Layout.rightMargin: -Theme.settingsWindow.rowPadding
                                Layout.bottomMargin: Theme.layout.spacing
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: Theme.controls.labelColor
                                    font.pixelSize: Theme.controls.labelFontSize
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                SettingsButton {
                                    implicitHeight: Theme.controls.footerButtonHeight
                                    text: (modelData.adds ?? false) ? qsTr("Undo and remove the game") : qsTr("Undo")
                                    enabled: !GameRunner.closingSteam
                                    onClicked: error.text = Library.undoSteamEdits(modelData.id)
                                }
                            }
                            Repeater {
                                model: modelData.changes
                                RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: Theme.layout.spacing
                                    SettingsNote {
                                        Layout.alignment: Qt.AlignTop
                                        text: modelData.name
                                        wrapMode: Text.NoWrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1  →  %2").arg(modelData.from !== "" ? modelData.from : qsTr("none"))
                                                               .arg(modelData.to !== "" ? modelData.to : qsTr("none"))
                                        color: Theme.controls.inputText
                                        font.family: Theme.font.monoFamily
                                        font.pixelSize: Theme.controls.descriptionFontSize
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                        }
                    }
                    Divider {
                        visible: Theme.settingsWindow.rowDividers
                        Layout.leftMargin: -Theme.settingsWindow.rowPadding
                        Layout.rightMargin: -Theme.settingsWindow.rowPadding
                        Layout.topMargin: Theme.layout.spacing / 2
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.layout.spacing / 2
                        Layout.bottomMargin: -Theme.settingsWindow.rowPadding / 2
                        spacing: Theme.layout.spacing
                        SettingsNote {
                            Layout.fillWidth: true
                            // Asked again when the queue or the closing changes; Steam starting
                            // and stopping isn't signalled otherwise.
                            readonly property bool steamRunning: {
                                root.steam
                                GameRunner.closingSteam
                                return GameRunner.steamRunning()
                            }
                            text: (root.steam.queue.length === 1 ? qsTr("1 game waiting")
                                  : qsTr("%1 games waiting").arg(root.steam.queue.length)) + "  ·  "
                                  + (steamRunning ? qsTr("Steam is running") : qsTr("Steam is closed"))
                        }
                        SettingsButton {
                            implicitHeight: Theme.controls.footerButtonHeight
                            text: GameRunner.closingSteam ? qsTr("Closing Steam") : qsTr("Close Steam and apply")
                            enabled: !GameRunner.closingSteam
                            onClicked: error.text = GameRunner.closeSteamAndApply()
                        }
                    }
                }

                SectionHeader { text: qsTr("Default presets") }
                SettingsNote {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("New Steam games start with these in their launch options. Games already in the "
                               + "library keep theirs, unless you apply one to all of them.")
                }
                SettingsCard {
                    Repeater {
                        model: root.steam.presets
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            SettingsCheckBox {
                                text: modelData.name
                                checked: root.steam.defaultPresets.includes(modelData.id)
                                onToggled: root.setDefault(modelData.id, checked)
                            }
                            SettingsNote {
                                Layout.fillWidth: true
                                text: root.steam.games === 0 ? ""
                                      : qsTr("on in %1 of %2 Steam games").arg(modelData.games).arg(root.steam.games)
                                wrapMode: Text.NoWrap
                                elide: Text.ElideRight
                            }
                            SettingsButton {
                                text: qsTr("Apply to all")
                                enabled: modelData.games < root.steam.games
                                tip: qsTr("Turn %1 on in every Steam game's launch options").arg(modelData.name)
                                onClicked: error.text = Library.setSteamPresetForAll(modelData.id, true)
                            }
                            SettingsButton {
                                text: qsTr("Remove from all")
                                danger: enabled
                                enabled: modelData.games > 0
                                tip: qsTr("Turn %1 off in every Steam game's launch options").arg(modelData.name)
                                onClicked: error.text = Library.setSteamPresetForAll(modelData.id, false)
                            }
                        }
                    }
                }

                SectionHeader { text: qsTr("Presets") }
                SettingsNote {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("Each one is a check box in a Steam game's settings, and adds these to its launch options.")
                }
                SettingsCard {
                    Repeater {
                        model: root.steam.presets
                        ColumnLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.topMargin: index > 0 ? Theme.layout.spacing : 0
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing
                                Label {
                                    text: modelData.name
                                    color: Theme.controls.labelColor
                                    font.pixelSize: Theme.controls.labelFontSize
                                    font.bold: true
                                }
                                SettingsNote {
                                    text: modelData.builtIn ? qsTr("built in, [steam.presets.%1]").arg(modelData.id)
                                                            : qsTr("[steam.presets.%1]").arg(modelData.id)
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.adds
                                color: Theme.controls.inputText
                                font.family: Theme.font.monoFamily
                                font.pixelSize: Theme.controls.descriptionFontSize
                                wrapMode: Text.WrapAnywhere
                            }
                            SettingsNote {
                                visible: modelData.note !== ""
                                Layout.fillWidth: true
                                text: modelData.note
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.settingsWindow.sectionSpacing
                    spacing: Theme.layout.spacing
                    SettingsNote {
                        Layout.fillWidth: true
                        text: qsTr("Add your own, or change a built-in one, as [steam.presets.<id>] in settings.toml, "
                                   + "with a name and any of env, wrapper and args.")
                    }
                    SettingsButton {
                        text: qsTr("Open settings.toml")
                        iconPath: Icons.openExternal
                        onClicked: {
                            const err = GameRunner.openFile(Actions.settingsFile())
                            if (err !== "")
                                message(qsTr("Could not open — path: %1").arg(err))
                        }
                    }
                }
            }
        }
    }
}
