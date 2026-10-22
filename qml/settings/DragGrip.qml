import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Item {
    id: root

    property bool draggable: true
    property string tip: ""
    readonly property Item area: mouse

    // Pointer positions are in the area's coordinates; DragReorder maps them (the row moves while dragged).
    signal dragStarted(var handle, real x, real y)
    signal dragMoved(var handle, real x, real y)
    signal dragEnded(bool dropped)

    implicitWidth: Theme.controls.iconSize
    implicitHeight: Theme.controls.iconSize

    Icon {
        anchors.centerIn: parent
        path: Icons.grip
        size: Theme.controls.iconSize
        color: mouse.containsMouse || mouse.pressed ? Theme.controls.labelColor : Theme.controls.iconColor
    }

    MouseArea {
        id: mouse
        property real pressX
        property real pressY
        property bool dragging: false

        anchors.fill: parent
        anchors.margins: -8
        enabled: root.draggable
        // The page's Flickable would take a vertical drag for a scroll.
        preventStealing: true
        hoverEnabled: true
        cursorShape: dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        onPressed: m => { pressX = m.x; pressY = m.y }
        onPositionChanged: m => {
            if (!pressed)
                return
            if (!dragging) {
                if (Math.hypot(m.x - pressX, m.y - pressY) <= Qt.styleHints.startDragDistance)
                    return
                dragging = true
                root.dragStarted(mouse, pressX, pressY)
            }
            root.dragMoved(mouse, m.x, m.y)
        }
        onReleased: {
            const was = dragging
            dragging = false
            if (was)
                root.dragEnded(true)
        }
        onCanceled: {
            const was = dragging
            dragging = false
            if (was)
                root.dragEnded(false)
        }

        ToolTip.visible: containsMouse && !dragging && root.tip !== ""
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: root.tip
    }
}
