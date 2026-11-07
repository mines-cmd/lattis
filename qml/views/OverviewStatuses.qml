import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property bool barsGrown: false

    readonly property var counts: overview.statusCounts ?? []
    readonly property int total: overview.games ?? 0

    signal filterRequested(string status)

    function countText(c): string {
        return qsTr("%1 · %2").arg(c.count).arg(percentText(c.count, total))
    }

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    OverviewQuiet {
        id: sample
        visible: false
        text: root.counts.reduce((widest, c) => root.countText(c).length > widest.length ? root.countText(c) : widest, "")
    }

    OverviewCardTitle { text: qsTr("Status") }

    Repeater {
        model: root.counts

        OverviewBarRow {
            required property var modelData
            name: modelData.name
            countText: root.countText(modelData)
            countWidth: sample.implicitWidth
            fraction: root.total > 0 ? modelData.count / root.total : 0
            grown: root.barsGrown
            onGrew: root.barsGrown = true
            onClicked: root.filterRequested(modelData.status)
        }
    }
}
