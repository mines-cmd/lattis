import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: root

    implicitWidth: indicator.implicitWidth
    implicitHeight: indicator.implicitHeight
    padding: 0
    spacing: 0
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    indicator: Rectangle {
        implicitWidth: Theme.controls.switchWidth
        implicitHeight: Theme.controls.switchHeight
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: height / 2
        color: root.checked ? Theme.controls.switchTrackOn : Theme.controls.switchTrack
        border.color: Theme.controls.inputBorder
        border.width: root.checked ? 0 : Theme.controls.inputBorderWidth

        Behavior on color { enabled: Motion.fades; MotionColor {} }

        Rectangle {
            readonly property real inset: 3

            readonly property real restWidth: parent.height - inset * 2
            readonly property real targetWidth: root.down ? restWidth * 1.3 : restWidth

            width: targetWidth
            height: restWidth
            radius: height / 2
            y: inset
            x: root.checked ? parent.width - targetWidth - inset : inset
            color: root.checked ? Theme.controls.switchHandleOn : Theme.controls.switchHandle
            scale: root.hovered && !root.down ? 1.08 : 1

            Behavior on x { enabled: Motion.on; MotionSpring {} }
            Behavior on width { enabled: Motion.on; MotionNumber { speed: "fast" } }
            Behavior on scale { enabled: Motion.on; MotionNumber {} }
        }
    }

    contentItem: Item {}
}
