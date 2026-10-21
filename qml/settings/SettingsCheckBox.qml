import QtQuick
import QtQuick.Controls.Basic

CheckBox {
    id: root

    padding: 0
    spacing: Theme.layout.spacing
    hoverEnabled: true
    property bool bordered: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    indicator: Rectangle {
        id: box
        implicitWidth: Theme.controls.checkboxSize
        implicitHeight: Theme.controls.checkboxSize
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: Theme.controls.checkboxRadius
        color: root.checked ? Theme.controls.checkboxOn
             : root.hovered ? Theme.controls.buttonHoverBackground : Theme.controls.inputBackground
        border.color: root.visualFocus ? Theme.controls.inputBorderFocus : Theme.controls.inputBorder
        border.width: root.visualFocus || (!root.checked && (root.bordered || root.hovered)) ? Math.max(1, Theme.controls.inputBorderWidth) : 0

        Behavior on color { enabled: Motion.fades; MotionColor {} }
        Behavior on border.color { enabled: Motion.fades; MotionColor {} }

        BoxBump { value: root.checked }

        MarkPop {
            anchors.fill: parent
            shown: root.checked
            hiddenScale: 0
            overshoot: Theme.motion.markOvershoot * 1.3

            Icon {
                anchors.centerIn: parent
                path: Icons.check
                size: parent.width - 4
                color: Theme.controls.checkboxMark
            }
        }
    }

    contentItem: Label {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: Theme.controls.labelColor
        font.pixelSize: Theme.controls.labelFontSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
