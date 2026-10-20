import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property bool open: false
    property real topPadding: 0
    property real bottomPadding: 0
    property real leftPadding: 0
    property real rightPadding: 0
    property alias spacing: column.spacing
    default property alias content: column.data

    property real progress: open ? 1 : 0
    Behavior on progress { enabled: Motion.on; MotionNumber { speed: "normal" } }

    implicitHeight: (column.implicitHeight + topPadding + bottomPadding) * progress

    // Only while it moves: once open, focus rings and the like may reach past its edge.
    clip: progress < 1

    ColumnLayout {
        id: column
        x: root.leftPadding
        y: root.topPadding
        width: root.width - root.leftPadding - root.rightPadding
        // Hidden rather than the wrapper, which keeps the width the row gives it, so the entries lay
        // out at their real width from the first frame of unfolding.
        visible: root.progress > 0
        opacity: root.progress
    }
}
