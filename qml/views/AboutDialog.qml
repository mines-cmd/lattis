import QtQuick
import QtQuick.Controls.Basic

MotionPopup {
    id: root

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.about.width, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    contentItem: AboutContent {
        backgroundColor: Theme.gameEditor.background
        labelColor: Theme.gameEditor.labelColor
        linkColor: Theme.gameEditor.linkColor
    }

    Icon {
        parent: root.background
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.about.closeMargin
        path: Icons.close
        size: Theme.about.closeSize
        color: closeMouse.containsMouse ? Theme.about.closeHoverColor : Theme.about.closeColor

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            anchors.margins: -6
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.close()
        }

        ToolTip.visible: closeMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: qsTr("Close")
    }
}
