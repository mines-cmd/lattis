import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide

    spacing: Theme.onboarding.spacing

    readonly property bool summary: !guide.reopened
    readonly property bool scanning: Library.scanning
    readonly property bool lookingUp: ScanLookup.running
    readonly property bool busy: scanning || lookingUp
    readonly property int found: Library.gameCount

    property bool lookedUpAny: false
    property int lookedUp: 0
    property int unresolved: 0

    Connections {
        target: ScanLookup
        enabled: root.summary
        function onFinished(written, unresolved) {
            root.lookedUpAny = true
            root.lookedUp += written
            root.unresolved += unresolved
        }
    }

    readonly property string description: {
        if (!summary)
            return qsTr("Everything the guide set up can be changed in Settings.")
        if (scanning)
            return qsTr("Bringing your games in. You can start using the app meanwhile.")
        const games = found === 1 ? qsTr("Found 1 game.") : qsTr("Found %1 games.").arg(found)
        if (lookingUp)
            return games + " " + qsTr("Looking them up.")
        if (found === 0)
            return qsTr("No games were found. Add ROM folders in Settings → Platforms, or add a game with +.")
        if (!lookedUpAny)
            return games + " " + qsTr("They came in with their names; “Look up missing info” in Settings → Metadata "
                                      + "can fill them in.")
        let text = games + " " + (lookedUp === 1 ? qsTr("1 was looked up") : qsTr("%1 were looked up").arg(lookedUp))
        if (unresolved > 0)
            text += qsTr("; %n had no clear match and wait in “Look up missing info”", "", unresolved)
        return text + "."
    }

    SettingsCard {
        panel: true
        visible: root.summary

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Bringing your games in")
                color: Theme.settingsWindow.labelColor
                font.pixelSize: Theme.settingsWindow.labelFontSize
                font.bold: true
            }
            Label {
                visible: root.busy
                text: root.scanning ? qsTr("Scanning…")
                                    : qsTr("Looking up: %1 of %2").arg(ScanLookup.progressDone).arg(ScanLookup.progressTotal)
                color: Theme.onboarding.descriptionColor
            }
            Icon {
                visible: !root.busy
                path: Icons.check
                size: Theme.settingsWindow.labelFontSize
                color: Theme.onboarding.foundColor
            }
            Label {
                visible: !root.busy
                text: qsTr("Done")
                color: Theme.onboarding.foundColor
            }
        }

        Rectangle {
            id: track
            Layout.fillWidth: true
            implicitHeight: Theme.gameEditor.progressHeight
            radius: Theme.gameEditor.progressRadius
            color: Theme.gameEditor.progressTrack
            clip: true

            Rectangle {
                id: fill
                height: parent.height
                radius: parent.radius
                color: Theme.gameEditor.progressFill
                width: root.scanning ? track.width / 3
                     : root.lookingUp && ScanLookup.progressTotal > 0
                       ? track.width * Math.min(1, ScanLookup.progressDone / ScanLookup.progressTotal)
                     : root.lookingUp ? 0 : track.width

                Behavior on width { enabled: Motion.on && track.visible; MotionNumber { speed: "normal" } }

                NumberAnimation on x {
                    running: root.scanning && track.visible
                    loops: Animation.Infinite
                    from: -track.width / 3
                    to: track.width
                    duration: Theme.onboarding.scanSlideDuration
                    onRunningChanged: if (!running) fill.x = 0
                }
            }
        }

        // Four to a row, however many there are. The tiles' width comes from the grid's, so the
        // grid takes its width from this item: sized by itself, it would size itself from the tiles
        // and never settle (as when the card is hidden on a reopened guide).
        Item {
            visible: Library.gameCounts.length > 0
            Layout.fillWidth: true
            implicitHeight: tiles.implicitHeight

            Grid {
                id: tiles
                width: parent.width
                columns: 4
                spacing: Theme.layout.spacing

                Repeater {
                    model: Library.gameCounts

                    Surface {
                        required property var modelData
                        width: (tiles.width - (tiles.columns - 1) * tiles.spacing) / tiles.columns
                        height: tile.implicitHeight + 2 * Theme.settingsWindow.rowPadding
                        radius: Theme.settingsWindow.inputRadius
                        fill: Theme.onboarding.tileBackground
                        fillEnd: Theme.onboarding.tileBackgroundEnd
                        direction: Theme.onboarding.gradientDirection
                        border.color: Theme.onboarding.panelBorder
                        border.width: 1

                        ColumnLayout {
                            id: tile
                            anchors.fill: parent
                            anchors.margins: Theme.settingsWindow.rowPadding
                            spacing: 2

                            Label {
                                text: modelData.count
                                color: Theme.palette.text
                                font.pixelSize: Theme.onboarding.tileCountSize
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: Theme.onboarding.descriptionColor
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }

    SettingsCard {
        panel: true
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.layout.spacing
            text: qsTr("Good to know")
            color: Theme.settingsWindow.labelColor
            font.pixelSize: Theme.settingsWindow.labelFontSize
            font.bold: true
        }

        Repeater {
            id: tips
            model: [
                { text: qsTr("All keyboard shortcuts, and how to change them"), keys: Actions.keys.show_shortcuts },
                { text: qsTr("The app menu: themes, backups, this guide and more"), keys: Actions.keys.app_menu },
                { text: qsTr("Add a game: a ROM, a Windows program or a Linux one"), keys: Actions.keys.add_game },
                { text: qsTr("Right-click a game for its menu"), keys: Actions.keys.game_menu },
                { text: qsTr("Settings"), keys: Actions.keys.open_settings },
            ]

            ColumnLayout {
                id: tip
                required property var modelData
                required property int index
                readonly property string keyText: Actions.label(modelData.keys)

                Layout.fillWidth: true
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.settingsWindow.rowDivider
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.settingsWindow.rowPadding
                    Layout.bottomMargin: tip.index < tips.count - 1 ? Theme.settingsWindow.rowPadding : 0
                    spacing: Theme.layout.spacing

                    Label {
                        Layout.fillWidth: true
                        text: tip.modelData.text
                        color: Theme.settingsWindow.labelColor
                        wrapMode: Text.Wrap
                    }
                    KeyChip {
                        visible: tip.keyText !== ""
                        text: tip.keyText
                    }
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
