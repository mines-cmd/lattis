import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Surface {
    id: chrome

    property string icon
    property real iconRotation: 0
    property string label
    property string tip

    signal clicked()

    implicitHeight: Theme.layout.toolbarHeight
    implicitWidth: chromeRow.implicitWidth + 2 * (chrome.label === "" ? Theme.layout.spacing : Theme.layout.spacing * 1.5)
    radius: Theme.iconButton.radius
    opacity: enabled ? 1 : 0.35
    fill: chromeMouse.containsMouse && enabled ? Theme.iconButton.hoverBackground : Theme.iconButton.background
    fillEnd: chromeMouse.containsMouse && enabled ? Theme.iconButton.hoverBackgroundEnd : Theme.iconButton.backgroundEnd
    direction: Theme.iconButton.gradientDirection
    border.color: Theme.iconButton.border
    border.width: Theme.iconButton.borderWidth
    scale: chromeMouse.pressed ? Theme.motion.pressScale : 1

    Behavior on fill { enabled: Motion.fades; MotionColor {} }
    Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
    Behavior on scale { enabled: Motion.on; MotionNumber {} }

    Row {
        id: chromeRow
        anchors.centerIn: parent
        spacing: Theme.layout.spacing

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            size: Theme.iconButton.iconSize
            color: Theme.iconButton.icon
            path: chrome.icon
            rotation: chrome.iconRotation
        }
        Label {
            visible: chrome.label !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: chrome.label
            color: Theme.iconButton.icon
        }
    }

    MouseArea {
        id: chromeMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: chrome.clicked()
    }

    ToolTip.visible: chrome.tip !== "" && chromeMouse.containsMouse
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: chrome.tip
}
