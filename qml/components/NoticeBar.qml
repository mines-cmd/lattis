import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// See docs/code/qml.md.
Surface {
    id: root

    property string text
    property var parts: []
    property bool warning: false
    property int count: 1
    property bool peeking: false
    readonly property alias hovered: hover.hovered
    default property alias actions: actionRow.data

    radius: Theme.notice.radius
    fill: Theme.notice.background
    fillEnd: Theme.notice.backgroundEnd
    direction: Theme.notice.gradientDirection
    border.color: warning ? Theme.notice.warningBorder : Theme.notice.border
    border.width: Theme.notice.borderWidth

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.border.width
        radius: Math.max(0, root.radius - root.border.width)
        color: Qt.alpha(root.warning ? Theme.notice.warningTint : Theme.notice.tint, Theme.notice.tintOpacity)
    }

    RowLayout {
        visible: !root.peeking
        anchors.fill: parent
        anchors.leftMargin: Theme.notice.padding
        anchors.rightMargin: Theme.notice.padding
        spacing: Theme.layout.spacing

        Icon {
            path: root.warning ? Icons.alertLine : Icons.informationLine
            size: Theme.notice.iconSize
            color: root.warning ? Theme.notice.warningIcon : Theme.notice.icon
        }
        Rectangle {
            visible: root.count >= 2
            Layout.preferredWidth: Math.max(height, countLabel.implicitWidth + 2 * Theme.layout.spacingSmall)
            Layout.preferredHeight: countLabel.implicitHeight + 2
            radius: Theme.radius.pill
            color: Qt.alpha(Theme.notice.count, Theme.notice.countOpacity)

            Label {
                id: countLabel
                anchors.centerIn: parent
                text: root.count
                color: Theme.notice.countText
                font.pixelSize: Theme.notice.countSize
                font.weight: Theme.font.emphasisWeight
            }
        }
        Item {
            id: message
            readonly property bool truncated: root.parts.length > 0 ? pieces.implicitWidth > width : label.truncated
            Layout.fillWidth: true
            implicitHeight: root.parts.length > 0 ? pieces.implicitHeight : label.implicitHeight
            clip: root.parts.length > 0
            layer.enabled: truncated && root.parts.length > 0
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: fadeMask
            }

            Item {
                id: fadeMask
                visible: false
                layer.enabled: true
                width: message.width
                height: message.height
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "white" }
                        GradientStop { position: Math.max(0, 1 - 2 * Theme.layout.margin / Math.max(1, message.width)); color: "white" }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }
            }

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
    ToolTip.visible: hover.hovered && message.truncated && !root.peeking
    ToolTip.text: root.text
    ToolTip.delay: 500
}
