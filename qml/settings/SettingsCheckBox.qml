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
        implicitWidth: Theme.settingsWindow.checkboxSize
        implicitHeight: Theme.settingsWindow.checkboxSize
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: Theme.settingsWindow.checkboxRadius
        color: root.checked ? Theme.settingsWindow.checkboxOn
             : root.hovered ? Theme.settingsWindow.buttonHoverBackground : Theme.settingsWindow.inputBackground
        border.color: root.visualFocus ? Theme.settingsWindow.inputBorderFocus : Theme.settingsWindow.inputBorder
        border.width: root.visualFocus || (!root.checked && (root.bordered || root.hovered)) ? Math.max(1, Theme.settingsWindow.inputBorderWidth) : 0

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
                color: Theme.settingsWindow.checkboxMark
            }
        }
    }

    contentItem: Label {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: Theme.settingsWindow.labelColor
        font.pixelSize: Theme.settingsWindow.labelFontSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
