import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property string icon
    property string label
    property string value
    property bool attention: false
    property var attentionColor: undefined
    // The icon's color when set; attention wins over it.
    property var iconTint: undefined
    property bool clickable: false
    property bool leftAligned: false
    property real contentInset: Theme.statChip.padding
    property bool valueFirst: false
    property bool showIcon: Theme.statChip.showIcons
    property int iconSize: Theme.statChip.iconSize
    property int valueFontSize: Theme.font.size
    property int labelFontSize: Theme.statChip.labelFontSize
    property bool uppercaseLabels: Theme.statChip.uppercaseLabels
    property color valueColor: Theme.statChip.valueColor
    property color labelColor: Theme.statChip.labelColor
    property color hoverValueColor: Theme.statChip.hoverValueColor
    property color hoverLabelColor: Theme.statChip.hoverLabelColor
    readonly property real fitWidth: row.implicitWidth + 2 * contentInset
    property string tip
    // A rating shown as stars in the tooltip instead of tip; -1 for none.
    property real tipStars: -1
    readonly property bool lit: clickable && mouse.containsMouse
    default property alias extras: row.data

    signal clicked()

    implicitWidth: row.implicitWidth + 2 * Theme.statChip.padding
    implicitHeight: Theme.statChip.height
    radius: Theme.statChip.radius
    fill: Theme.statChip.background
    fillEnd: Theme.statChip.backgroundEnd
    direction: Theme.statChip.gradientDirection
    border.color: root.attention ? (root.attentionColor ?? Theme.notice.border) : Theme.statChip.border
    border.width: root.attention ? Theme.notice.borderWidth :Theme.statChip.borderWidth
    scale: clickable && mouse.pressed ? Theme.motion.pressScale : 1

    Behavior on scale { enabled: Motion.on; MotionNumber {} }

    MouseArea {
        id: mouse
        anchors.fill: parent
        // Hidden, not just disabled: a disabled MouseArea still gives the pointer its cursor.
        visible: root.clickable
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()

        ToolTip.visible: root.tip !== "" && containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: root.tip
    }

    HoverHandler {
        id: hover
        enabled: root.tipStars >= 0
    }
    // Its own tooltip: the shared one shows text only.
    Loader {
        active: root.tipStars >= 0
        sourceComponent: ToolTip {
            parent: root
            visible: hover.hovered
            delay: Theme.tooltip.delay
            enter: Transition { MotionNumber { property: "opacity"; from: 0; to: 1; fade: true } }
            exit: Transition { MotionNumber { property: "opacity"; to: 0; fade: true } }
            background: Rectangle {
                radius: Theme.radius.tiny
                color: Theme.tooltip.background
                border.color: Theme.tooltip.border
                border.width: Theme.tooltip.borderWidth
            }
            contentItem: Stars {
                value: root.tipStars
                starSize: Theme.statChip.starSize
                spacing: Theme.infoPanel.starSpacing
                color: Theme.infoPanel.star
                emptyColor: Theme.infoPanel.starEmpty
            }
        }
    }

    RowLayout {
        id: row
        x: root.leftAligned ? root.contentInset : (root.width - width) / 2
        y: (root.height - height) / 2
        spacing: Theme.layout.spacing

        Icon {
            visible: root.showIcon && root.icon !== ""
            path: root.icon
            size: root.iconSize
            color: root.attention ? (root.attentionColor ?? Theme.notice.icon)
                 : root.iconTint !== undefined ? root.iconTint
                 : root.lit ? Theme.statChip.hoverIconColor : Theme.statChip.iconColor
            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }

        GridLayout {
            columns: 1
            rowSpacing: 1

            Label {
                Layout.row: root.valueFirst ? 1 : 0
                text: root.uppercaseLabels ? root.label.toUpperCase() : root.label
                color: root.lit ? root.hoverLabelColor : root.labelColor
                Behavior on color { enabled: Motion.fades; MotionColor {} }
                font.pixelSize: root.labelFontSize
                font.letterSpacing: root.uppercaseLabels ? 1 : 0
            }
            Label {
                Layout.row: root.valueFirst ? 0 : 1
                text: root.value
                color: root.lit ? root.hoverValueColor : root.valueColor
                Behavior on color { enabled: Motion.fades; MotionColor {} }
                font.pixelSize: root.valueFontSize
            }
        }
    }
}
