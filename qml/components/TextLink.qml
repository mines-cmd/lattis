import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: root

    property color textColor: Theme.gameEditor.hintColor
    property color hoverColor: Theme.gameEditor.lookupText
    property bool underlined: false
    property color underlineColor: "transparent"
    property color underlineHoverColor: underlineColor
    property int underlineOffset: 2
    // Shown after the text (an Icons path), in the text's color.
    property string trailingIcon
    property int iconSize: font.pixelSize > 0 ? font.pixelSize : Theme.font.size
    property int textFormat: Text.PlainText

    hoverEnabled: true
    padding: Theme.layout.spacing / 2
    leftPadding: Theme.layout.spacing
    rightPadding: Theme.layout.spacing
    opacity: enabled ? 1 : Theme.gameEditor.readOnlyOpacity
    Accessible.role: Accessible.Link

    readonly property color shownColor: hovered && enabled ? hoverColor : textColor

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: Theme.controls.inputRadius / 2
        color: "transparent"
        border.color: Theme.controls.inputBorderFocus
        border.width: root.visualFocus ? Theme.controls.inputBorderWidth : 0
    }
    contentItem: Row {
        spacing: Theme.layout.spacing / 2

        Label {
            id: label
            text: root.text
            font: root.font
            textFormat: root.textFormat
            color: root.shownColor
            anchors.verticalCenter: parent.verticalCenter

            Behavior on color { enabled: Motion.fades; MotionColor {} }

            Rectangle {
                visible: root.underlined
                x: 0
                y: label.baselineOffset + root.underlineOffset
                width: label.width
                height: 1
                color: root.hovered && root.enabled ? root.underlineHoverColor : root.underlineColor

                Behavior on color { enabled: Motion.fades; MotionColor {} }
            }
        }
        Icon {
            visible: root.trailingIcon !== ""
            path: root.trailingIcon
            size: root.iconSize
            color: root.shownColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
