import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property bool barsGrown: false

    readonly property var allCounts: overview.statusCounts ?? []
    readonly property var counts: allCounts.filter(c => c.status !== "none")
    readonly property int unsetCount: allCounts.find(c => c.status === "none")?.count ?? 0
    readonly property int total: counts.reduce((sum, c) => sum + c.count, 0)

    signal filterRequested(string status)

    function countText(c): string {
        return String(c.count)
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

    OverviewQuiet {
        id: footnote
        Layout.fillWidth: true
        visible: root.unsetCount > 0
        text: qsTr("%n game(s) with no status", "", root.unsetCount)
        elide: Text.ElideRight
        font.underline: footnoteMouse.containsMouse

        MouseArea {
            id: footnoteMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.filterRequested("none")
        }
    }
}
