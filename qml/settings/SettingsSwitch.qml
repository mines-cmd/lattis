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
        implicitWidth: Theme.settingsWindow.switchWidth
        implicitHeight: Theme.settingsWindow.switchHeight
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: height / 2
        color: root.checked ? Theme.settingsWindow.switchTrackOn : Theme.settingsWindow.switchTrack
        border.color: Theme.settingsWindow.inputBorder
        border.width: root.checked ? 0 : Theme.settingsWindow.inputBorderWidth

        Behavior on color { enabled: Motion.fades; MotionColor {} }

        Rectangle {
            readonly property real inset: 3

            width: parent.height - inset * 2
            height: width
            radius: width / 2
            y: inset
            x: root.checked ? parent.width - width - inset : inset
            color: root.checked ? Theme.settingsWindow.switchHandleOn : Theme.settingsWindow.switchHandle
            scale: root.hovered ? 1.08 : 1

            Behavior on x { enabled: Motion.on; MotionSpring {} }
            Behavior on scale { enabled: Motion.on; MotionNumber {} }
        }
    }

    contentItem: Item {}
}
