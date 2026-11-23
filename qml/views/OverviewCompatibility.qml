import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    // Whether ProtonDB is still being asked about the games with no entry.
    property bool asking: false
    property bool barsGrown: false

    readonly property var stats: overview.compatibility ?? ({})
    readonly property var tiers: stats.tiers ?? []
    readonly property int checked: tiers.reduce((sum, t) => sum + t.count, 0)
    readonly property int games: stats.games ?? 0

    property real stackGrow: barsGrown ? 1 : 0
    Behavior on stackGrow { enabled: Motion.on; MotionNumber { speed: "slow" } }

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    function tierColor(tier: string): color {
        return Theme.statChip["protondb" + tier.charAt(0).toUpperCase() + tier.slice(1)]
    }

    function tierName(tier: string): string {
        const names = {
            native: qsTr("Native"), platinum: qsTr("Platinum"), gold: qsTr("Gold"), silver: qsTr("Silver"),
            bronze: qsTr("Bronze"), borked: qsTr("Borked"), pending: qsTr("Pending")
        }
        return names[tier] ?? tier
    }

    function countText(count: int): string {
        return qsTr("%1 · %2").arg(count).arg(percentText(count, games))
    }

    OverviewQuiet {
        id: sample
        visible: false
        text: root.countText(root.games)
    }

    RowLayout {
        Layout.fillWidth: true

        OverviewCardTitle {
            Layout.fillWidth: true
            text: qsTr("Compatibility")
        }
        OverviewQuiet {
            visible: root.games > 0
            text: qsTr("ProtonDB tiers of your installed Windows games")
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.overview.storageBarHeight
        visible: root.games > 0
        radius: Theme.overview.storageBarRadius
        color: Theme.overview.barTrack

        Rectangle {
            id: barMask
            anchors.fill: parent
            radius: parent.radius
            visible: false
            layer.enabled: true
        }

        Row {
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: barMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }
            spacing: Theme.overview.storageSegmentGap

            Repeater {
                model: root.tiers.concat(root.stats.noReports > 0 ? [{ tier: "", count: root.stats.noReports }] : [])

                Rectangle {
                    required property var modelData
                    width: root.games > 0 ? parent.parent.width * modelData.count / root.games * root.stackGrow : 0
                    height: parent.height
                    color: modelData.tier === "" ? Theme.overview.compatNone : root.tierColor(modelData.tier)
                }
            }
        }
    }

    Repeater {
        model: root.tiers

        OverviewBarRow {
            regrow: root.regrow
            required property var modelData
            name: root.tierName(modelData.tier)
            countText: root.countText(modelData.count)
            countWidth: sample.implicitWidth
            fraction: root.games > 0 ? modelData.count / root.games : 0
            fill: root.tierColor(modelData.tier)
            dot: true
            grown: root.barsGrown
            onGrew: root.barsGrown = true
        }
    }

    OverviewBarRow {
        regrow: root.regrow
        visible: root.games > 0 && root.stats.noReports > 0
        name: qsTr("No reports")
        countText: root.countText(root.stats.noReports ?? 0)
        countWidth: sample.implicitWidth
        fraction: root.games > 0 ? (root.stats.noReports ?? 0) / root.games : 0
        fill: Theme.overview.compatNone
        dot: true
        grown: root.barsGrown
    }

    OverviewQuiet {
        Layout.fillWidth: true
        visible: root.games > 0 && root.stats.unchecked > 0
        wrapMode: Text.WordWrap
        text: root.asking ? qsTr("%n still being asked", "", root.stats.unchecked ?? 0)
                          : qsTr("%n not asked yet", "", root.stats.unchecked ?? 0)
    }

    RowLayout {
        Layout.fillWidth: true
        visible: (root.stats.native ?? 0) + (root.stats.proton ?? 0) > 0
        spacing: Theme.layout.spacing * 1.5

        Repeater {
            model: [
                { label: qsTr("Native games"), value: root.stats.native ?? 0 },
                { label: qsTr("Proton games"), value: root.stats.proton ?? 0 }
            ]

            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: tile.implicitHeight + 2 * Theme.layout.spacing * 1.5
                radius: Theme.overview.compatTileRadius
                color: Theme.overview.compatTileBackground

                ColumnLayout {
                    id: tile
                    anchors.fill: parent
                    anchors.margins: Theme.layout.spacing * 1.5
                    spacing: 0

                    OverviewQuiet { text: parent.parent.modelData.label }
                    PlainLabel {
                        text: parent.parent.modelData.value
                        color: Theme.palette.text
                        font.pixelSize: Theme.overview.cardTitleSize
                        font.weight: Theme.font.strongWeight
                    }
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        visible: topTool.visible
        color: Theme.overview.compatDivider
    }

    RowLayout {
        id: topTool
        Layout.fillWidth: true
        visible: root.stats.topTool !== undefined && root.stats.topTool !== ""

        Label {
            text: qsTr("Most used")
            color: Theme.overview.textMuted
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            color: Theme.overview.textMuted
            text: qsTr("%1 · %n game(s)", "", root.stats.topToolGames ?? 0).arg(root.stats.topTool)
        }
    }
}
