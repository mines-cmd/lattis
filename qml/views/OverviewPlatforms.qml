import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property var info
    property bool barsGrown: false

    readonly property var counts: overview.platformCounts ?? []
    readonly property int most: counts.reduce((most, c) => Math.max(most, c.games), 0)

    signal filterRequested(string platform)

    function countText(c): string {
        return qsTr("%1 · %2 installed").arg(c.games).arg(c.installed)
    }

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    OverviewQuiet {
        id: sample
        visible: false
        text: root.counts.reduce((widest, c) => root.countText(c).length > widest.length ? root.countText(c) : widest, "")
    }

    OverviewCardTitle { text: qsTr("Games by platform") }

    Repeater {
        model: root.counts

        OverviewBarRow {
            required property var modelData
            name: root.info.nameOf(modelData.platform)
            countText: root.countText(modelData)
            countWidth: sample.implicitWidth
            fraction: root.most > 0 ? modelData.games / root.most : 0
            fill: root.info.colorOf(modelData.platform)
            grown: root.barsGrown
            onGrew: root.barsGrown = true
            onClicked: root.filterRequested(modelData.platform)
        }
    }

    OverviewQuiet {
        Layout.topMargin: Theme.layout.spacing
        text: qsTr("%1 installed · %2 played").arg(root.overview.installed ?? 0).arg(root.overview.played ?? 0)
    }
}
