import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property bool barsGrown: false

    readonly property var counts: overview.statusCounts ?? []
    readonly property int most: counts.reduce((most, c) => Math.max(most, c.count), 0)

    signal filterRequested(string status)

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    OverviewCardTitle { text: qsTr("Status") }

    Repeater {
        model: root.counts

        OverviewBarRow {
            required property var modelData
            name: modelData.name
            countText: String(modelData.count)
            fraction: root.most > 0 ? modelData.count / root.most : 0
            grown: root.barsGrown
            onGrew: root.barsGrown = true
            onClicked: root.filterRequested(modelData.status)
        }
    }
}
