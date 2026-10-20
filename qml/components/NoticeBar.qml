import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property string text
    default property alias actions: actionRow.data

    radius: Theme.notice.radius
    fill: Theme.notice.background
    fillEnd: Theme.notice.backgroundEnd
    direction: Theme.notice.gradientDirection
    border.color: Theme.notice.border
    border.width: Theme.notice.borderWidth

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.notice.padding
        anchors.rightMargin: Theme.notice.padding
        spacing: Theme.layout.spacing

        Icon {
            path: Icons.info
            size: Theme.notice.iconSize
            color: Theme.notice.icon
        }
        Label {
            id: label
            Layout.fillWidth: true
            text: root.text
            elide: Text.ElideRight
            color: Theme.notice.text
        }
        RowLayout {
            id: actionRow
            Layout.fillHeight: true
            Layout.topMargin: Theme.notice.buttonInset
            Layout.bottomMargin: Theme.notice.buttonInset
            spacing: Theme.layout.spacing
        }
    }

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && label.truncated
    ToolTip.text: root.text
    ToolTip.delay: 500
}
