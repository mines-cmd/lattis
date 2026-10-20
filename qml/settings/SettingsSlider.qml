import QtQuick
import QtQuick.Controls.Basic

Slider {
    id: root

    readonly property real handleSize: Theme.settingsWindow.sliderHandleSize

    implicitHeight: Math.max(handleSize + 8, 24)
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        implicitWidth: 120
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        width: root.availableWidth
        height: Theme.settingsWindow.sliderTrackHeight
        radius: height / 2
        color: Theme.settingsWindow.sliderTrack

        // From the handle's animated place rather than visualPosition, so the fill keeps up with the spring.
        Rectangle {
            readonly property real travel: root.availableWidth - root.handle.width
            width: travel > 0 ? Math.max(0, Math.min(1, (root.handle.x - root.leftPadding) / travel)) * parent.width : 0
            height: parent.height
            radius: parent.radius
            color: Theme.settingsWindow.sliderFill
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + (root.availableHeight - height) / 2
        width: root.handleSize
        height: root.handleSize
        radius: width / 2
        color: Theme.settingsWindow.sliderHandle
        scale: root.pressed ? 1.15 : root.hovered ? 1.08 : 1

        Behavior on x { enabled: Motion.on; MotionSpring { damping: Theme.settingsWindow.sliderDamping } }
        Behavior on scale { enabled: Motion.on; MotionNumber {} }
    }
}
