import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    property bool barsGrown: false

    readonly property var stats: overview.saves ?? ({})
    readonly property int played: stats.played ?? 0
    readonly property int backedUp: stats.backedUp ?? 0
    readonly property bool empty: (stats.archives ?? 0) === 0

    property real grow: barsGrown ? 1 : 0
    Behavior on grow { enabled: Motion.on; MotionNumber { speed: "slow" } }

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    RowLayout {
        Layout.fillHeight: false
        Layout.fillWidth: true

        OverviewCardTitle {
            Layout.fillWidth: true
            text: qsTr("Save backups")
        }
        SettingsButton {
            primary: root.empty
            iconPath: Icons.save2Line
            text: GameSaves.working ? qsTr("Backing up") : qsTr("Back up now")
            tip: qsTr("Back up the saves of every game you've played")
            enabled: !GameSaves.working
            onClicked: GameSaves.backUpAll()
        }
    }

    RowLayout {
        Layout.fillHeight: false
        Layout.fillWidth: true
        visible: root.empty
        spacing: Theme.layout.spacing * 1.5

        Rectangle {
            Layout.preferredWidth: Theme.overview.attentionIconTile
            Layout.preferredHeight: Theme.overview.attentionIconTile
            radius: Theme.overview.savesTileRadius
            color: Theme.overview.savesIconBackground

            Icon {
                anchors.centerIn: parent
                size: Theme.overview.attentionIconSize
                color: Theme.overview.savesIcon
                path: Icons.shieldCheckLine
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: root.played > 0 ? qsTr("%n played game(s), no backups", "", root.played) : qsTr("Nothing backed up yet")
                color: Theme.palette.text
                elide: Text.ElideRight
            }
            OverviewQuiet {
                Layout.fillWidth: true
                visible: root.played > 0
                wrapMode: Text.WordWrap
                text: qsTr("Saves stay safe if a prefix breaks or a game updates.")
            }
        }
    }

    ColumnLayout {
        Layout.fillHeight: false
        Layout.fillWidth: true
        visible: !root.empty && root.played > 0
        spacing: Theme.layout.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing / 2

            Label {
                text: root.backedUp
                color: Theme.palette.text
                font.pixelSize: Theme.overview.cardTitleSize * 1.5
                font.weight: Theme.font.strongWeight
            }
            OverviewQuiet {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBaseline
                text: qsTr("of %n played game(s)", "", root.played)
            }
            OverviewQuiet { text: root.percentText(root.backedUp, root.played) }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.overview.barHeight
            radius: Theme.overview.barRadius
            color: Theme.overview.barTrack

            Rectangle {
                width: root.backedUp > 0 ? Math.max(Theme.overview.barHeight, parent.width * root.backedUp / root.played * root.grow * root.regrow) : 0
                height: parent.height
                radius: parent.radius
                color: Theme.overview.barFill
            }
        }
        Component.onCompleted: {
            root.barsGrown = true
        }
    }

    RowLayout {
        Layout.fillHeight: false
        Layout.fillWidth: true
        visible: !root.empty
        spacing: Theme.layout.spacing * 1.5

        Repeater {
            model: [
                { label: qsTr("Last backup"), value: root.stats.lastText ?? "" },
                { label: qsTr("Backups"), value: root.stats.archives > 0 ? String(root.stats.archives) : "" },
                { label: qsTr("Space used"), value: root.stats.archives > 0 ? Library.sizeText(root.stats.bytes) : "" }
            ]

            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                visible: modelData.value !== ""
                implicitHeight: tile.implicitHeight + 2 * Theme.layout.spacing * 1.5
                radius: Theme.overview.savesTileRadius
                color: Theme.overview.savesTileBackground

                ColumnLayout {
                    id: tile
                    anchors.fill: parent
                    anchors.margins: Theme.layout.spacing * 1.5
                    spacing: 0

                    OverviewQuiet { text: parent.parent.modelData.label }
                    PlainLabel {
                        Layout.fillWidth: true
                        text: parent.parent.modelData.value
                        color: Theme.palette.text
                        font.weight: Theme.font.emphasisWeight
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
