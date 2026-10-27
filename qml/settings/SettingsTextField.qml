import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: root

    property color fieldBackground: Theme.controls.inputBackground
    property color textColor: Theme.controls.inputText
    property color placeholderColor: Theme.controls.placeholderColor

    implicitHeight: Theme.controls.inputHeight
    selectByMouse: true
    color: root.textColor
    placeholderTextColor: root.placeholderColor
    verticalAlignment: TextInput.AlignVCenter

    HoverHandler { id: fieldPointer }
    ContextMenu.menu: TextContextMenu { field: root; pointer: fieldPointer }

    background: Rectangle {
        radius: Theme.controls.inputRadius
        color: root.fieldBackground
        border.color: root.activeFocus ? Theme.controls.inputBorderFocus : Theme.controls.inputBorder
        border.width: root.activeFocus ? Theme.controls.inputBorderFocusWidth : Theme.controls.inputBorderWidth

        Behavior on border.color { enabled: Motion.fades; MotionColor {} }
    }
}
