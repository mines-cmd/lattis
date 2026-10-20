import QtQuick
import QtQuick.Controls.Basic

RadioButton {
    id: root

    padding: 0
    spacing: Theme.layout.spacing
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    indicator: Rectangle {
        implicitWidth: Theme.settingsWindow.checkboxSize
        implicitHeight: Theme.settingsWindow.checkboxSize
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: width / 2
        color: root.hovered && !root.checked ? Theme.settingsWindow.buttonHoverBackground
                                             : Theme.settingsWindow.inputBackground
        border.color: root.visualFocus ? Theme.settingsWindow.inputBorderFocus
                    : root.checked ? Theme.settingsWindow.checkboxOn : Theme.settingsWindow.inputBorder
        border.width: Math.max(1, Theme.settingsWindow.inputBorderWidth)

        Behavior on color { enabled: Motion.fades; MotionColor {} }
        Behavior on border.color { enabled: Motion.fades; MotionColor {} }

        MarkPop {
            anchors.fill: parent
            shown: root.checked

            // About half the circle, with a whole-pixel gap all round: centering on a half pixel
            // puts it off center.
            Rectangle {
                readonly property int inset: Math.round(parent.width / 4)
                x: inset
                y: inset
                width: parent.width - 2 * inset
                height: width
                radius: width / 2
                color: Theme.settingsWindow.checkboxOn
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
