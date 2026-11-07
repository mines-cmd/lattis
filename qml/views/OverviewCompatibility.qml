import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    // Whether ProtonDB is still being asked about the games with no entry.
    property bool asking: false
    property bool barsGrown: false

    readonly property var stats: overview.compatibility ?? ({})
    readonly property var tiers: stats.tiers ?? []
    readonly property int checked: tiers.reduce((sum, t) => sum + t.count, 0)
    readonly property int games: stats.games ?? 0

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

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

    OverviewCardTitle { text: qsTr("Compatibility") }

    OverviewQuiet {
        visible: root.games > 0
        text: qsTr("ProtonDB tiers of your installed Steam games")
    }

    Repeater {
        model: root.tiers

        OverviewBarRow {
            required property var modelData
            name: root.tierName(modelData.tier)
            countText: root.countText(modelData.count)
            countWidth: sample.implicitWidth
            fraction: root.games > 0 ? modelData.count / root.games : 0
            fill: Theme.statChip["protondb" + modelData.tier.charAt(0).toUpperCase() + modelData.tier.slice(1)]
            grown: root.barsGrown
            onGrew: root.barsGrown = true
        }
    }

    OverviewQuiet {
        Layout.fillWidth: true
        visible: root.games > 0 && (root.stats.noReports > 0 || root.stats.unchecked > 0)
        wrapMode: Text.WordWrap
        text: {
            const parts = []
            if (root.stats.noReports > 0)
                parts.push(qsTr("%n without reports", "", root.stats.noReports))
            if (root.stats.unchecked > 0)
                parts.push(root.asking ? qsTr("%n still being asked", "", root.stats.unchecked)
                                       : qsTr("%n not asked yet", "", root.stats.unchecked))
            return parts.join(" · ")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: (root.stats.native ?? 0) + (root.stats.proton ?? 0) > 0
        spacing: Theme.layout.margin

        Repeater {
            model: [
                { label: qsTr("Native games"), value: root.stats.native ?? 0 },
                { label: qsTr("Proton games"), value: root.stats.proton ?? 0 }
            ]

            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 0

                OverviewQuiet { text: parent.modelData.label }
                Label {
                    text: parent.modelData.value
                    color: Theme.palette.text
                    font.pixelSize: Theme.overview.cardTitleSize
                    font.weight: Theme.font.strongWeight
                }
            }
        }
    }

    OverviewQuiet {
        Layout.fillWidth: true
        visible: root.stats.topTool !== undefined && root.stats.topTool !== ""
        elide: Text.ElideRight
        text: qsTr("Most used: %1 (%n game(s))", "", root.stats.topToolGames ?? 0).arg(root.stats.topTool)
    }
}
