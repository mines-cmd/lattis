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
    // The page's scroller: a drag held near its top or bottom edge scrolls it, faster the closer it gets.
    property Flickable flickable: null
    property real scrollEdge: 48
    property real scrollSpeed: 14

    // A drop that moves the row. The page either writes the move and calls settle(), whose end is the
    // list reloading in the new order, or calls settle() and writes on settled(), once the row is in place.
    signal dropped(string id, int from, int to)
    signal settled(string id, int from, int to)

    property string dragId: ""
    property int from: -1
    property int to: -1
    property real pressY: 0
    property real dy: 0
    // In scene coordinates, so the row's place can be worked out again after the page scrolls under the pointer.
    property point scenePointer
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
        from = index
        to = index
        pressY = handle.mapToItem(column, x, y).y
        scenePointer = handle.mapToItem(null, x, y)
        dy = 0
        settling = false
        // Last, as these turn `dragging` on and the rows read `from` and `to` the moment it is.
        dragId = ids[index]
        order = ids.join(",")
    }

    function move(handle, x, y) {
        if (!dragging || settling)
            return
        scenePointer = handle.mapToItem(null, x, y)
        follow()
    }

    function follow() {
        const row = entryAt(from)
        const first = entryAt(0)
        const last = entryAt(count - 1)
        const moved = column.mapFromItem(null, scenePointer.x, scenePointer.y).y - pressY
        dy = Math.max(first.y - row.y, Math.min(last.y + last.height - row.y - row.height, moved))
        // By the dragged row's leading edge, not its center: clamped at the end of the card, a center
        // never passes a neighbour of its own height, so the first and last slots were out of reach.
        const top = row.y + dy
        const bottom = top + row.height
        const middle = i => entryAt(i).y + entryAt(i).height / 2
        let slot = from
        for (let i = from + 1; i < count && bottom > middle(i); ++i)
            slot = i
        for (let i = from - 1; i >= 0 && top < middle(i); --i)
            slot = i
        to = slot
    }

    function autoScroll() {
        const y = flickable.mapFromItem(null, scenePointer.x, scenePointer.y).y
        const edge = Math.min(scrollEdge, flickable.height / 4)
        let step = 0
        if (y < edge)
            step = -scrollSpeed * Math.min(1, (edge - y) / edge)
        else if (y > flickable.height - edge)
            step = scrollSpeed * Math.min(1, (y - flickable.height + edge) / edge)
        // No further than the card's end, past which the row can't follow.
        const top = column.mapToItem(flickable, 0, 0).y
        if (step < 0)
            step = Math.min(0, Math.max(step, top - scrollEdge))
        else
            step = Math.max(0, Math.min(step, top + column.height + scrollEdge - flickable.height))
        const target = Math.max(0, Math.min(flickable.contentHeight - flickable.height, flickable.contentY + step))
        if (step === 0 || target === flickable.contentY)
            return
        flickable.contentY = target
        follow()
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
        interval: 16
        repeat: true
        running: root.flickable !== null && root.dragging && !root.settling
        onTriggered: root.autoScroll()
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
