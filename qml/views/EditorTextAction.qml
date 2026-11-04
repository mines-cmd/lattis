import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: action
    property alias text: actionLabel.text
    property alias iconPath: actionIcon.path
    property string tip
    property string disabledTip
    // Not `enabled`: the item stays enabled, so its tooltip shows while the button is disabled.
    property alias clickable: actionButton.enabled
    signal clicked()

    implicitWidth: actionButton.implicitWidth
    implicitHeight: actionButton.implicitHeight
    HoverHandler { id: actionHover }
    ToolTip.visible: actionHover.hovered && ToolTip.text !== ""
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: actionButton.enabled ? tip : disabledTip

    AbstractButton {
        id: actionButton
        anchors.fill: parent
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        opacity: enabled ? 1 : 0.45
        onClicked: action.clicked()

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        background: null
        contentItem: Row {
            spacing: 4

            Icon {
                id: actionIcon
                anchors.verticalCenter: parent.verticalCenter
                size: Theme.gameEditor.cardActionFontSize + 3
                color: actionLabel.color
            }
            Label {
                id: actionLabel
                anchors.verticalCenter: parent.verticalCenter
                color: actionButton.hovered || actionButton.visualFocus
                       ? Theme.gameEditor.cardActionHoverColor : Theme.gameEditor.cardActionColor
                font.pixelSize: Theme.gameEditor.cardActionFontSize
            }
        }
    }
}
