import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property var download: ({})

    readonly property bool paused: download.state === "paused"
    readonly property int percent: download.percent ?? -1

    property bool tooltips: true

    readonly property bool hovered: area.containsMouse
    property bool carriedHover: false
    readonly property bool hot: hovered || carriedHover

    signal expand()

    implicitWidth: row.implicitWidth + 2 * Theme.downloadBar.padding
    radius: Theme.downloadBar.pillRadius
    fill: Theme.downloadBar.background
    fillEnd: Theme.downloadBar.backgroundEnd
    direction: Theme.downloadBar.gradientDirection
    border.color: Theme.downloadBar.border
    border.width: Theme.downloadBar.borderWidth

    property real rowFromBottom: Theme.downloadBar.pillHeight / 2

    // From the morph: 0 bar .. 1 pill, the bar's width at rest, and how far below the percent row the
    // bar's icon sits.
    property real slide: 1
    property real fullWidth: width
    property real iconDrop: 0

    RowLayout {
        id: row
        x: root.width - Theme.downloadBar.padding - width
        y: root.height - root.rowFromBottom - height / 2
        spacing: Theme.layout.spacing

        Item {
            id: iconSlot
            visible: Theme.downloadBar.pillIconSize > 0
            Layout.preferredWidth: Theme.downloadBar.pillIconSize
            Layout.preferredHeight: Theme.downloadBar.pillIconSize
        }
        Label {
            visible: root.percent >= 0
            text: root.percent + "%"
            color: Theme.downloadBar.text
        }
        Icon {
            visible: Theme.downloadBar.chevronSize > 0
            path: Icons.chevron
            size: Theme.downloadBar.chevronSize
            rotation: 90 - 180 * root.slide
            color: hot ? Theme.downloadBar.chevronHover : Theme.downloadBar.text
        }
    }

    Icon {
        readonly property real startSize: Theme.downloadBar.iconSize > 0 ? Theme.downloadBar.iconSize : Theme.downloadBar.pillIconSize
        readonly property real startX: Theme.downloadBar.padding + startSize / 2 - (root.fullWidth - root.width)
        readonly property real endX: row.x + iconSlot.x + iconSlot.width / 2
        readonly property real endY: row.y + row.height / 2

        visible: Theme.downloadBar.pillIconSize > 0
        path: Icons.download
        size: startSize + (Theme.downloadBar.pillIconSize - startSize) * root.slide
        color: root.paused ? Theme.downloadBar.pausedFill : Theme.downloadBar.fill
        opacity: Theme.downloadBar.iconSize > 0 ? 1 : root.slide
        x: startX + (endX - startX) * root.slide - width / 2
        y: endY + root.iconDrop * (1 - root.slide) - height / 2
    }

    MouseArea {
        id: area
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: root.expand()
    }
    ToolTip.visible: area.containsMouse && tooltips
    ToolTip.text: qsTr("Show the download bar")
    ToolTip.delay: 1000
}
