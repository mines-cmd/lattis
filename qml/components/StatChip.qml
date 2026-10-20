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
    property bool clickable: false
    property string tip
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

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 10

        Icon {
            visible: Theme.statChip.showIcons && root.icon !== ""
            path: root.icon
            size: Theme.statChip.iconSize
            color: root.attention ? (root.attentionColor ?? Theme.notice.icon)
                 : root.lit ? Theme.statChip.hoverIconColor : Theme.statChip.iconColor
            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }

        ColumnLayout {
            spacing: 1

            Label {
                text: Theme.statChip.uppercaseLabels ? root.label.toUpperCase() : root.label
                color: root.lit ? Theme.statChip.hoverLabelColor : Theme.statChip.labelColor
                Behavior on color { enabled: Motion.fades; MotionColor {} }
                font.pixelSize: Theme.statChip.labelFontSize
                font.letterSpacing: Theme.statChip.uppercaseLabels ? 1 : 0
            }
            Label {
                text: root.value
                color: root.lit ? Theme.statChip.hoverValueColor : Theme.statChip.valueColor
                Behavior on color { enabled: Motion.fades; MotionColor {} }
            }
        }
    }
}
