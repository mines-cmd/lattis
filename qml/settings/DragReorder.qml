import QtQuick

// See docs/code/qml.md. Placed beside `column` in the card, it draws the line where the row will land.
Item {
    id: root

    required property Item column
    required property Repeater repeater
    // The ids of the repeater's rows in order; the first `count` of them can be dragged.
    property var ids: []
    property int count: ids.length
    property int settleTime: Motion.duration(Theme.motion.normal)

    // A drop that moves the row. The page either writes the move and calls settle(), whose end is the
    // list reloading in the new order, or calls settle() and writes on settled(), once the row is in place.
    signal dropped(string id, int from, int to)
    signal settled(string id, int from, int to)

    property string dragId: ""
    property int from: -1
    property int to: -1
    property real pressY: 0
    property real dy: 0
    property bool settling: false
    // The order when the drag began: once the list reloads in the new order the drag is over, so the rebuilt
    // rows start in place instead of animating from where the drag had put them.
    property string order: ""
    readonly property bool dragging: dragId !== "" && ids.join(",") === order

    function entryAt(i) {
        return repeater.itemAt(i)
    }

    function shiftOf(index) {
        if (!dragging || index === from)
            return 0
        const height = entryAt(from).height
        if (to > from && index > from && index <= to)
            return -height
        if (to < from && index >= to && index < from)
            return height
        return 0
    }

    function settledOffset() {
        let offset = 0
        if (to > from) {
            for (let i = from + 1; i <= to; ++i)
                offset += entryAt(i).height
        } else {
            for (let i = to; i < from; ++i)
                offset -= entryAt(i).height
        }
        return offset
    }

    function isDragged(index) {
        return dragging && index === from
    }

    function offsetOf(index) {
        return isDragged(index) ? (settling ? settledOffset() : dy) : shiftOf(index)
    }

    // A row under the pointer follows it directly; every other move eases.
    function follows(index) {
        return isDragged(index) && !settling
    }

    function start(handle, index, x, y) {
        dragId = ids[index]
        order = ids.join(",")
        from = index
        to = index
        pressY = handle.mapToItem(column, x, y).y
        dy = 0
        settling = false
    }

    function move(handle, x, y) {
        if (!dragging || settling)
            return
        const row = entryAt(from)
        const first = entryAt(0)
        const last = entryAt(count - 1)
        const moved = handle.mapToItem(column, x, y).y - pressY
        dy = Math.max(first.y - row.y, Math.min(last.y + last.height - row.y - row.height, moved))
        const center = row.y + row.height / 2 + dy
        let slot = 0
        for (let i = 0; i < count; ++i) {
            if (i !== from && entryAt(i).y + entryAt(i).height / 2 < center)
                ++slot
        }
        to = slot
    }

    function end(drop) {
        if (!dragging || settling)
            return
        if (!drop || to === from) {
            reset()
            return
        }
        dropped(dragId, from, to)
    }

    function settle() {
        settling = true
        settleTimer.interval = settleTime
        settleTimer.restart()
    }

    function reset() {
        settleTimer.stop()
        dragId = ""
        from = -1
        to = -1
        dy = 0
        settling = false
    }

    // The ids in the order a drop from `from` to `to` leaves them.
    function movedIds(from, to) {
        const out = ids.slice()
        out.splice(to, 0, out.splice(from, 1)[0])
        return out
    }

    function dragForScreenshot(index, offset) {
        const handle = entryAt(index).handle
        start(handle, index, 0, 0)
        pressY = handle.mapToItem(column, 0, 0).y
        move(handle, 0, offset)
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.dragging && !root.settling
        onActivated: root.reset()
    }

    Timer {
        id: settleTimer
        onTriggered: {
            root.settled(root.dragId, root.from, root.to)
            root.reset()
        }
    }

    x: column.x
    y: column.y
    width: column.width
    height: column.height
    visible: dragging
    z: 2

    Rectangle {
        y: (root.dragging ? (root.to > root.from ? root.entryAt(root.to).y + root.entryAt(root.to).height
                                                 : root.entryAt(root.to).y) : 0) - height / 2
        width: parent.width
        height: Theme.settingsWindow.dropIndicatorWidth
        radius: height / 2
        color: Theme.settingsWindow.dropIndicator
    }
}
