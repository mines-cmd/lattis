import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: root

    implicitHeight: Theme.settingsWindow.inputHeight
    selectByMouse: true
    color: Theme.settingsWindow.inputText
    placeholderTextColor: Theme.settingsWindow.placeholderColor
    verticalAlignment: TextInput.AlignVCenter

    HoverHandler { id: fieldPointer }
    ContextMenu.menu: TextContextMenu { field: root; pointer: fieldPointer }

    background: Rectangle {
        radius: Theme.settingsWindow.inputRadius
        color: Theme.settingsWindow.inputBackground
        border.color: root.activeFocus ? Theme.settingsWindow.inputBorderFocus : Theme.settingsWindow.inputBorder
        border.width: root.activeFocus ? Theme.settingsWindow.inputBorderFocusWidth : Theme.settingsWindow.inputBorderWidth

        Behavior on border.color { enabled: Motion.fades; MotionColor {} }
    }
}
