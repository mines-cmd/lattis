import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    property bool barsGrown: false

    readonly property var counts: overview.topTags ?? []
    readonly property int most: counts.reduce((most, c) => Math.max(most, c.count), 0)

    signal filterRequested(string tag)

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    OverviewCardTitle { text: qsTr("Top tags") }

    Repeater {
        model: root.counts

        OverviewBarRow {
            regrow: root.regrow
            required property var modelData
            name: modelData.tag
            countText: String(modelData.count)
            fraction: root.most > 0 ? modelData.count / root.most : 0
            grown: root.barsGrown
            onGrew: root.barsGrown = true
            onClicked: root.filterRequested(modelData.tag)
        }
    }
}
