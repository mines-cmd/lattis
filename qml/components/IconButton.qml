import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Surface {
    id: root

    property string icon
    property string tip
    property real iconSize: Theme.iconButton.iconSize
    property color iconColor: Theme.iconButton.icon
    property bool badge: false
    property color badgeColor: Theme.iconButton.badge
    property bool busy: false
    property bool plain: false
    property color hoverTint: "transparent"

    readonly property bool hovered: mouse.containsMouse

    signal clicked()

    implicitWidth: Theme.iconButton.size
    implicitHeight: Theme.iconButton.size
    radius: Theme.iconButton.radius
    fill: plain ? "transparent" : mouse.containsMouse ? Theme.iconButton.hoverBackground : Theme.iconButton.background
    fillEnd: plain ? "transparent" : mouse.containsMouse ? Theme.iconButton.hoverBackgroundEnd : Theme.iconButton.backgroundEnd
    direction: Theme.iconButton.gradientDirection
    border.color: plain ? "transparent" : Theme.iconButton.border
    border.width: Theme.iconButton.borderWidth
    scale: mouse.pressed ? Theme.motion.pressScale : 1

    Behavior on scale { enabled: Motion.on; MotionNumber {} }
    Behavior on fill { enabled: Motion.fades; MotionColor {} }
    Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.hoverTint
        opacity: mouse.containsMouse ? 1 : 0
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
    }

    Icon {
        id: icon
        anchors.centerIn: parent
        path: root.busy ? Icons.loader : root.icon
        size: root.iconSize
        color: root.iconColor
        opacity: root.plain && !mouse.containsMouse ? 0.7 : 1

        RotationAnimation on rotation {
            running: root.busy
            from: 0
            to: 360
            duration: Theme.iconButton.spinDuration
            loops: Animation.Infinite
            onStopped: icon.rotation = 0
        }
    }

    Rectangle {
        visible: root.badge
        width: 9
        height: 9
        radius: height / 2
        color: root.badgeColor
        border.color: Theme.palette.background
        border.width: 2
        anchors { top: parent.top; right: parent.right; margins: 6 }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: root.tip !== "" && mouse.containsMouse
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: root.tip
}
