import QtQuick
import QtQuick.Controls.Basic

SettingsMenu {
    id: root
    atPointer: true
    // A right click on the field while it's open asks Qt to open it again, which only moves it to the
    // text cursor: the move is the cue to close it instead.
    // Put back at once so it doesn't flash there, and closed after popup() has returned, which would
    // open it again if closed first.
    property point spot
    property bool pinned: false
    onOpened: {
        spot = Qt.point(x, y)
        pinned = true
    }
    onClosed: pinned = false
    onXChanged: if (pinned && x !== spot.x) unpin()
    onYChanged: if (pinned && y !== spot.y) unpin()
    function unpin() {
        x = spot.x
        y = spot.y
        pinned = false
        Qt.callLater(close)
    }

    required property Item field

    // A HoverHandler declared in the field: Qt places a ContextMenu.menu at the text cursor, so it moves
    // under the pointer as it opens. Not created here, as a menu is built when first requested and a
    // handler parented that late crashes.
    property HoverHandler pointer: null
    onAboutToShow: {
        if (pointer?.hovered) {
            x = pointer.point.position.x
            y = pointer.point.position.y
        }
    }

    readonly property bool hasSelection: field.selectedText !== ""
    readonly property bool editable: !field.readOnly

    SettingsMenuItem {
        text: qsTr("Undo")
        keys: "Ctrl+Z"
        enabled: root.editable && root.field.canUndo
        onTriggered: root.field.undo()
    }
    SettingsMenuItem {
        text: qsTr("Redo")
        keys: "Ctrl+Shift+Z"
        enabled: root.editable && root.field.canRedo
        onTriggered: root.field.redo()
    }
    MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.settingsWindow.rowDivider
        }
    }
    SettingsMenuItem {
        text: qsTr("Cut")
        keys: "Ctrl+X"
        enabled: root.editable && root.hasSelection
        onTriggered: root.field.cut()
    }
    SettingsMenuItem {
        text: qsTr("Copy")
        keys: "Ctrl+C"
        enabled: root.hasSelection
        onTriggered: root.field.copy()
    }
    SettingsMenuItem {
        text: qsTr("Paste")
        keys: "Ctrl+V"
        enabled: root.editable && root.field.canPaste
        onTriggered: root.field.paste()
    }
    SettingsMenuItem {
        text: qsTr("Delete")
        enabled: root.editable && root.hasSelection
        onTriggered: root.field.remove(root.field.selectionStart, root.field.selectionEnd)
    }
    MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.settingsWindow.rowDivider
        }
    }
    SettingsMenuItem {
        text: qsTr("Select all")
        keys: "Ctrl+A"
        enabled: root.field.length > 0
        onTriggered: root.field.selectAll()
    }
}
