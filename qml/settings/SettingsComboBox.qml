import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: root

    implicitHeight: Theme.settingsWindow.inputHeight
    leftPadding: 12
    rightPadding: Theme.settingsWindow.iconSize + 20
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    contentItem: Label {
        text: root.displayText
        color: Theme.settingsWindow.inputText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: root.width - width - 10
        y: (root.height - height) / 2
        path: Icons.chevron
        size: Theme.settingsWindow.iconSize
        color: Theme.settingsWindow.iconColor
        rotation: dropdown.showing ? -90 : 90

        Behavior on rotation { enabled: Motion.on; MotionNumber {} }
    }

    background: Rectangle {
        radius: Theme.settingsWindow.inputRadius
        color: root.hovered ? Theme.settingsWindow.buttonHoverBackground : Theme.settingsWindow.inputBackground
        border.color: root.visualFocus ? Theme.settingsWindow.inputBorderFocus : Theme.settingsWindow.inputBorder
        border.width: root.visualFocus ? Theme.settingsWindow.inputBorderFocusWidth : Theme.settingsWindow.inputBorderWidth

        Behavior on color { enabled: Motion.fades; MotionColor {} }
        Behavior on border.color { enabled: Motion.fades; MotionColor {} }
    }

    delegate: ItemDelegate {
        id: item
        required property var modelData
        required property int index

        width: ListView.view.width
        height: Theme.settingsWindow.menuItemHeight
        leftPadding: 10
        highlighted: root.highlightedIndex === index
        hoverEnabled: true

        contentItem: Label {
            text: root.textRole ? item.modelData[root.textRole] : item.modelData
            color: Theme.settingsWindow.inputText
            font.bold: root.currentIndex === item.index
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.settingsWindow.menuItemRadius
            color: item.highlighted ? Theme.settingsWindow.rowHover : Qt.alpha(Theme.settingsWindow.rowHover, 0)

            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }
    }

    popup: Popup {
        id: dropdown

        y: root.height + 4
        clip: true
        property real reveal: 1
        height: implicitHeight * reveal
        enter: DropdownEnter {}
        exit: DropdownExit {}
        property bool showing: false
        onAboutToShow: showing = true
        onAboutToHide: showing = false
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320)
        padding: 4

        // The list keeps its full height while the popup drops down, and slides down with its bottom edge
        // (the popup clips it) like a drawer. Squeezed with it, the list would scroll to keep the current
        // entry in view, then back once there's room.
        contentItem: Item {
            implicitHeight: list.implicitHeight

            ListView {
                id: list
                width: parent.width
                y: -(1 - dropdown.reveal) * height
                height: dropdown.implicitHeight - dropdown.topPadding - dropdown.bottomPadding
                clip: true
                implicitHeight: contentHeight
                model: root.popup.visible ? root.delegateModel : null
                currentIndex: root.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical: ScrollIndicator {}
            }
        }

        background: Rectangle {
            radius: Theme.settingsWindow.inputRadius
            color: Theme.settingsWindow.popupBackground
            border.color: Theme.settingsWindow.inputBorder
            border.width: Math.max(1, Theme.settingsWindow.inputBorderWidth)
        }
    }
}
