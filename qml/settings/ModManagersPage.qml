import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Mod managers (Mod Organizer 2, Vortex, …) you open from the games they mod. A Windows program runs through Proton as a Steam shortcut; anything else runs directly. They never count as games.")

    readonly property var problems: Library.problems.filter(p => p.includes("mod-managers"))
    readonly property var managers: Library.modManagers

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {}
    function clearSearch() { return false }

    signal message(string text)

    function open(manager) {
        const missing = GameRunner.missingPrefixGame(manager.id)
        if (missing !== "") {
            message(qsTr("Run %1 once first, so Proton creates its prefix").arg(Library.gameTitle(missing)))
            return
        }
        const err = GameRunner.launchModManager(manager.id)
        if (err !== "")
            message(err)
    }

    ModManagerDialog {
        id: dialog
    }

    ModManagerRemove {
        id: removeDialog
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
            spacing: 10

            Item { Layout.fillWidth: true }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                text: qsTr("Add mod manager")
                iconPath: Icons.add
                onClicked: dialog.openFor(null, [])
            }

            ProblemsButton {
                problems: root.problems
            }
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
                width: flick.width - scrollBar.width - 8
                spacing: 0

                RowLayout {
                    visible: root.managers.length === 0
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("No mod managers yet. Point Lattis at one that's already on disk, then link the games it mods.")
                        color: Theme.controls.descriptionColor
                        font.pixelSize: Theme.controls.descriptionFontSize
                        wrapMode: Text.WordWrap
                    }
                    SettingsButton {
                        text: qsTr("Add mod manager")
                        iconPath: Icons.add
                        onClicked: dialog.openFor(null, [])
                    }
                }

                RowCard {
                    visible: root.managers.length > 0

                    Repeater {
                        model: root.managers

                        ListRow {
                            id: row
                            required property var modelData
                            required property int index
                            readonly property bool running: GameRunner.runningManagers.includes(modelData.id)
                            readonly property string kind: modelData.proton
                                ? qsTr("Proton · %1").arg(modelData.protonName) : qsTr("Native")
                            readonly property string status: running ? qsTr("Running")
                                : !modelData.inSteam ? qsTr("Not in Steam")
                                : modelData.pendingSteamEdits ? qsTr("Waiting for Steam to close") : ""

                            last: index === root.managers.length - 1
                            implicitHeight: Math.max(Theme.controls.inputHeight, info.implicitHeight + 16)

                            Icon {
                                path: Icons.puzzle
                                size: Theme.controls.iconSize * 1.4
                                color: Theme.controls.descriptionColor
                            }
                            ColumnLayout {
                                id: info
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: Theme.controls.controlSpacing
                                    Label {
                                        text: row.modelData.name
                                        color: Theme.controls.labelColor
                                        font.pixelSize: Theme.controls.labelFontSize
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: row.kind
                                        color: Theme.controls.descriptionColor
                                        font.pixelSize: Theme.settingsWindow.markerFontSize
                                    }
                                    Label {
                                        visible: row.status !== ""
                                        text: row.status
                                        color: row.running ? Theme.settingsWindow.installFound
                                                           : Theme.settingsWindow.installMissing
                                        font.pixelSize: Theme.settingsWindow.markerFontSize
                                    }
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Label {
                                        visible: row.modelData.games.length === 0
                                        text: qsTr("No games")
                                        color: Theme.controls.descriptionColor
                                        font.pixelSize: Theme.controls.descriptionFontSize
                                    }
                                    Repeater {
                                        model: row.modelData.games
                                        Label {
                                            required property var modelData
                                            required property int index
                                            text: (modelData.missing ? qsTr("%1 (missing)").arg(modelData.title) : modelData.title)
                                                  + (index < row.modelData.games.length - 1 ? "," : "")
                                            color: modelData.missing ? Theme.settingsWindow.installMissing
                                                                     : Theme.controls.descriptionColor
                                            font.pixelSize: Theme.controls.descriptionFontSize
                                        }
                                    }
                                }
                                Label {
                                    visible: row.modelData.sharedGame !== ""
                                    text: qsTr("Runs in %1's prefix").arg(row.modelData.sharedGame)
                                    color: Theme.controls.descriptionColor
                                    font.pixelSize: Theme.controls.descriptionFontSize
                                }
                            }

                            SettingsButton {
                                visible: row.modelData.proton && !row.modelData.inSteam
                                text: qsTr("Locate")
                                tip: qsTr("Add its shortcut back to Steam")
                                onClicked: {
                                    const err = Library.setModManagerValue(row.modelData.id, "in_steam", "")
                                    if (err !== "")
                                        root.message(err)
                                }
                            }
                            SettingsButton {
                                visible: row.running
                                text: qsTr("Force close")
                                onClicked: {
                                    const err = GameRunner.forceCloseModManager(row.modelData.id)
                                    if (err !== "")
                                        root.message(err)
                                }
                            }
                            SettingsButton {
                                visible: !row.running
                                text: qsTr("Open")
                                onClicked: root.open(row.modelData)
                            }
                            SettingsButton {
                                text: qsTr("Edit")
                                onClicked: dialog.openFor(row.modelData)
                            }
                            SettingsButton {
                                danger: true
                                text: qsTr("Remove")
                                onClicked: removeDialog.ask(row.modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
