import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: root

    implicitHeight: Theme.controls.inputHeight
    selectByMouse: true
    color: Theme.controls.inputText
    placeholderTextColor: Theme.controls.placeholderColor
    verticalAlignment: TextInput.AlignVCenter

    HoverHandler { id: fieldPointer }
    ContextMenu.menu: TextContextMenu { field: root; pointer: fieldPointer }

    background: Rectangle {
        radius: Theme.controls.inputRadius
        color: Theme.controls.inputBackground
        border.color: root.activeFocus ? Theme.controls.inputBorderFocus : Theme.controls.inputBorder
        border.width: root.activeFocus ? Theme.controls.inputBorderFocusWidth : Theme.controls.inputBorderWidth

        Behavior on border.color { enabled: Motion.fades; MotionColor {} }
    }
}
