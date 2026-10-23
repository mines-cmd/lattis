import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property string text
    property var parts: []
    readonly property alias hovered: hover.hovered
    default property alias actions: actionRow.data

    radius: Theme.notice.radius
    fill: Theme.notice.background
    fillEnd: Theme.notice.backgroundEnd
    direction: Theme.notice.gradientDirection
    border.color: Theme.notice.border
    border.width: Theme.notice.borderWidth

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.border.width
        radius: Math.max(0, root.radius - root.border.width)
        color: Qt.alpha(Theme.notice.tint, Theme.notice.tintOpacity)
    }

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
        Item {
            id: message
            readonly property bool truncated: root.parts.length > 0 ? pieces.implicitWidth > width : label.truncated
            Layout.fillWidth: true
            implicitHeight: root.parts.length > 0 ? pieces.implicitHeight : label.implicitHeight
            clip: root.parts.length > 0

            Label {
                id: label
                visible: root.parts.length === 0
                width: parent.width
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                elide: Text.ElideRight
                color: Theme.notice.text
            }
            Row {
                id: pieces
                visible: root.parts.length > 0
                anchors.verticalCenter: parent.verticalCenter

                Repeater {
                    model: root.parts

                    delegate: Label {
                        required property var modelData
                        readonly property bool chip: modelData.chip === true
                        anchors.verticalCenter: parent?.verticalCenter
                        text: modelData.text
                        color: chip ? Theme.notice.chipText : Theme.notice.text
                        leftPadding: chip ? Theme.notice.chipPadding : 0
                        rightPadding: chip ? Theme.notice.chipPadding : 0
                        topPadding: chip ? 2 : 0
                        bottomPadding: chip ? 2 : 0
                        background: Rectangle {
                            visible: parent.chip
                            radius: height / 2
                            color: Qt.alpha(Theme.notice.chip, Theme.notice.chipOpacity)
                        }
                    }
                }
            }
        }
        RowLayout {
            id: actionRow
            spacing: Theme.layout.spacing
        }
    }

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && message.truncated
    ToolTip.text: root.text
    ToolTip.delay: 500
}
