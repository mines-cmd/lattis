import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: root

    hoverEnabled: true
    padding: Theme.layout.spacing / 2
    leftPadding: Theme.layout.spacing
    rightPadding: Theme.layout.spacing
    opacity: enabled ? 1 : Theme.gameEditor.readOnlyOpacity

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: Theme.settingsWindow.inputRadius / 2
        color: "transparent"
        border.color: Theme.settingsWindow.inputBorderFocus
        border.width: root.visualFocus ? Theme.settingsWindow.inputBorderWidth : 0
    }
    contentItem: Label {
        text: root.text
        color: root.hovered && root.enabled ? Theme.gameEditor.lookupText : Theme.gameEditor.hintColor
        verticalAlignment: Text.AlignVCenter

        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }
}
