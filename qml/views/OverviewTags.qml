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
    readonly property var colors: {
        const byName = {}
        for (const t of Library.tags)
            byName[t.name] = t.color
        return byName
    }

    signal filterRequested(string tag)

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    RowLayout {
        Layout.fillWidth: true

        OverviewCardTitle {
            Layout.fillWidth: true
            text: qsTr("Top tags")
        }
        OverviewQuiet {
            visible: root.counts.length > 0
            text: qsTr("Click to filter")
        }
    }

    JustifiedFlow {
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        spacing: Theme.overview.tagSpacing
        maxRows: Theme.overview.tagRows

        Repeater {
            model: root.counts

            OverviewTagChip {
                required property var modelData
                name: modelData.tag
                countText: String(modelData.count)
                colorIndex: root.colors[modelData.tag] ?? 0
                onClicked: root.filterRequested(modelData.tag)
            }
        }
    }
}
