import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// One line of a breakdown: a name, a bar and a count. Clicking it asks for the games behind it.
Item {
    id: root

    property string name
    property string countText
    // A quieter line under the name, for what would crowd the count.
    property string detailText
    property real fraction: 0
    // The widest count in the card, for when it needs more than count_width; otherwise the bars in every card end alike.
    property real countWidth: 0
    property color fill: Theme.overview.barFill
    // Whether the card's bars have already grown, so a refresh doesn't grow them again.
    property bool grown: false
    property real grow: grown ? 1 : 0
    readonly property bool hovered: hover.hovered

    signal clicked()
    signal grew()

    Layout.fillWidth: true
    implicitHeight: line.implicitHeight + Theme.layout.spacing

    Component.onCompleted: {
        grow = 1
        grew()
    }
    Behavior on grow { enabled: Motion.on; MotionNumber { speed: "slow" } }

    Rectangle {
        anchors.fill: parent
        radius: Theme.overview.barRadius
        color: Theme.palette.hoverOverlay
        opacity: root.hovered ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
    }

    RowLayout {
        id: line
        anchors.fill: parent
        anchors.leftMargin: Theme.layout.spacing
        anchors.rightMargin: Theme.layout.spacing
        spacing: Theme.layout.spacing * 1.5

        ColumnLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: Theme.overview.labelWidth
            Layout.maximumWidth: Theme.overview.labelWidth
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: root.name
                color: Theme.palette.text
                elide: Text.ElideRight
            }
            OverviewQuiet {
                Layout.fillWidth: true
                visible: root.detailText !== ""
                text: root.detailText
                elide: Text.ElideRight
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.overview.barHeight
            radius: Theme.overview.barRadius
            color: Theme.overview.barTrack

            Rectangle {
                width: root.fraction > 0 ? Math.max(Theme.overview.barHeight, parent.width * root.fraction * root.grow) : 0
                height: parent.height
                radius: parent.radius
                color: root.fill
            }
        }
        OverviewQuiet {
            Layout.preferredWidth: Math.max(Theme.overview.countWidth, root.countWidth)
            text: root.countText
            horizontalAlignment: Text.AlignRight
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
