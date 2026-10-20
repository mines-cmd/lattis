import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton {
    id: root

    property string iconPath
    property real iconRotation: 0
    property bool flat: false
    property bool bordered: false
    property bool primary: false
    property bool danger: false
    property string tip

    readonly property color foreground: primary ? Theme.settingsWindow.buttonPrimaryText
                                        : danger ? Theme.settingsWindow.buttonDangerText : Theme.settingsWindow.buttonText
    readonly property bool filled: primary || danger
    property color textColor: foreground

    implicitHeight: text === "" ? Theme.settingsWindow.inputHeight : Theme.settingsWindow.buttonHeight
    implicitWidth: content.implicitWidth + leftPadding + rightPadding
    leftPadding: 14
    rightPadding: 14

    ToolTip.visible: tip !== "" && hovered
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: tip
    opacity: enabled ? 1 : 0.45
    hoverEnabled: true
    // The dip scales what's drawn, not the button: a popup follows its parent's scale, so a menu
    // opened from the button would shift as it springs back.
    property real dip: down ? Theme.motion.pressScale : 1
    Behavior on dip { enabled: Motion.on; MotionNumber {} }
    Binding { target: root.contentItem; property: "scale"; value: root.dip }

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    contentItem: Item {
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight

        RowLayout {
            id: content
            anchors.centerIn: parent
            spacing: Theme.settingsWindow.controlSpacing

            Icon {
                visible: root.iconPath !== ""
                path: root.iconPath
                size: Theme.settingsWindow.iconSize
                color: root.textColor
                rotation: root.iconRotation

                Behavior on rotation { enabled: Motion.on; MotionNumber {} }
            }
            Label {
                visible: root.text !== ""
                text: root.text
                color: root.textColor
                font.family: root.font.family
                font.pixelSize: root.font.pixelSize
                font.weight: root.primary ? Theme.font.onAccentWeight : root.font.weight
            }
        }
    }

    background: Surface {
        scale: root.dip
        radius: Theme.settingsWindow.inputRadius
        fill: root.flat ? "transparent"
            : root.primary ? Theme.settingsWindow.buttonPrimaryBackground
            : root.danger ? Theme.settingsWindow.buttonDangerBackground
            : root.hovered ? Theme.settingsWindow.buttonHoverBackground
            : Theme.settingsWindow.buttonBackground
        fillEnd: root.flat ? "transparent"
               : root.primary ? Theme.settingsWindow.buttonPrimaryBackgroundEnd
               : root.danger ? Theme.settingsWindow.buttonDangerBackground
               : root.hovered ? Theme.settingsWindow.buttonHoverBackgroundEnd
               : Theme.settingsWindow.buttonBackgroundEnd
        direction: Theme.settingsWindow.gradientDirection
        border.color: Theme.settingsWindow.inputBorder
        border.width: root.filled || root.flat || !root.bordered ? 0 : Theme.settingsWindow.inputBorderWidth

        Behavior on fill { enabled: Motion.fades; MotionColor {} }
        Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

        Rectangle {
            visible: root.filled || root.flat
            opacity: root.hovered ? 1 : 0
            anchors.fill: parent
            radius: parent.radius
            color: Theme.palette.hoverOverlay

            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
        }
    }
}
