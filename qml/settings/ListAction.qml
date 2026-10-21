import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: listAction
    property string tip
    property string iconPath: Icons.add

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    ToolTip.visible: tip !== "" && hovered
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: tip
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    background: null
    contentItem: Row {
        spacing: 4

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            path: listAction.iconPath
            size: Theme.controls.iconSize
            color: listActionLabel.color
        }
        Label {
            id: listActionLabel
            anchors.verticalCenter: parent.verticalCenter
            text: listAction.text
            color: listAction.hovered || listAction.visualFocus ? Theme.controls.labelColor
                                                                : Theme.controls.descriptionColor

            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }
    }
}
