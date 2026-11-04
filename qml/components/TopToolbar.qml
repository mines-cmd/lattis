import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Rectangle {
    id: root

    property string checkedButton: ""
    readonly property bool hovered: hover.hovered || dragging

    signal triggered(string name)
    signal message(string text)

    readonly property var known: ({
        add_game: { icon: Icons.add, tip: qsTr("Add game"), action: "add_game" },
        play_statistics: { icon: Icons.barChartFill, tip: qsTr("Play statistics"), action: "play_statistics" },
        gallery: { icon: Icons.image, checkedIcon: Icons.imageFill, tip: qsTr("Gallery"), action: "gallery" },
        open_settings: { icon: Icons.gear, tip: qsTr("Settings"), action: "open_settings" }
    })
    readonly property var buttons: General.toolbar
        ? General.toolbarButtonOrder.filter(b => known[b] !== undefined && !General.toolbarHiddenButtons.includes(b)) : []

    property string dragName: ""
    property int dragFrom: -1
    property int dragTo: -1
    property real dragPressX: 0
    property real dragDx: 0
    property bool dragOutside: false
    property bool dragSettling: false
    // The order when the drag began: once the toolbar reloads in the new order the drag is over, so the
    // rebuilt buttons start in place instead of animating from where the drag had put them.
    property string dragOrder: ""
    readonly property bool dragging: dragName !== "" && buttons.join(",") === dragOrder
    readonly property real dragStep: Theme.toolbar.size + row.spacing

    function shiftOf(index) {
        if (!dragging || index === dragFrom)
            return 0
        if (dragTo > dragFrom && index > dragFrom && index <= dragTo)
            return -dragStep
        if (dragTo < dragFrom && index >= dragTo && index < dragFrom)
            return dragStep
        return 0
    }

    function startDrag(button, index, x, y) {
        dragName = buttons[index]
        dragOrder = buttons.join(",")
        dragFrom = index
        dragTo = index
        dragPressX = button.mapToItem(row, x, y).x
        dragDx = 0
        dragOutside = false
        dragSettling = false
    }

    function moveDrag(button, x, y) {
        if (!dragging || dragSettling)
            return
        const p = button.mapToItem(root, x, y)
        dragDx = button.mapToItem(row, x, y).x - dragPressX
        dragOutside = p.x < 0 || p.x > width || p.y < 0 || p.y > height
        const last = buttons.length - 1
        dragDx = Math.max(-dragFrom * dragStep, Math.min((last - dragFrom) * dragStep, dragDx))
        dragTo = dragOutside ? dragFrom : Math.max(0, Math.min(last, dragFrom + Math.round(dragDx / dragStep)))
    }

    function endDrag(dropped) {
        if (!dragging || dragSettling)
            return
        if (!dropped || dragOutside || dragTo === dragFrom) {
            resetDrag()
            return
        }
        // The hidden buttons keep their places among the shown ones' slots.
        const moved = buttons.slice()
        moved.splice(dragTo, 0, moved.splice(dragFrom, 1)[0])
        let next = 0
        const order = General.toolbarButtonOrder.map(name => buttons.includes(name) ? moved[next++] : name)
        const error = General.setToolbarButtonOrder(order)
        if (error !== "") {
            resetDrag()
            message(error)
            return
        }
        dragSettling = true
        settleTimer.restart()
    }

    function dragForScreenshot(name, offset) {
        const from = buttons.indexOf(name)
        if (from < 0)
            return
        const button = repeater.itemAt(from)
        const mid = button.height / 2
        startDrag(button, from, button.width / 2, mid)
        moveDrag(button, button.width / 2 + offset, mid)
    }

    function cancelDrag() {
        repeater.itemAt(dragFrom)?.cancelDrag()
        resetDrag()
    }

    function resetDrag() {
        settleTimer.stop()
        dragName = ""
        dragFrom = -1
        dragTo = -1
        dragDx = 0
        dragOutside = false
        dragSettling = false
    }

    function openMenu(x, y) {
        menu.active = true
        const popup = menu.item
        if (popup.visible) {
            popup.close()
            return
        }
        if (Date.now() - menu.closedAt < 200)
            return
        popup.popup(root, x, y)
    }

    visible: buttons.length > 0
    implicitWidth: row.implicitWidth + 2 * Theme.toolbar.padding
    implicitHeight: row.implicitHeight + 2 * Theme.toolbar.padding
    radius: Theme.toolbar.backgroundRadius
    color: Theme.toolbar.background
    border.color: Theme.toolbar.border
    border.width: Theme.layout.borderWidth

    Shortcut {
        sequence: "Escape"
        enabled: root.dragging && !root.dragSettling
        onActivated: root.cancelDrag()
    }

    Connections {
        target: General
        function onToolbarButtonOrderChanged() { root.resetDrag() }
        function onToolbarHiddenButtonsChanged() { root.resetDrag() }
    }

    Timer {
        id: settleTimer
        interval: 800
        onTriggered: root.resetDrag()
    }

    HoverHandler { id: hover }

    // Also keeps the strip's drag-to-move zone from starting under the toolbar.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: mouse => {
            if (mouse.button === Qt.RightButton)
                root.openMenu(mouse.x, mouse.y)
        }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.toolbar.spacing

        Repeater {
            id: repeater
            model: root.buttons

            Surface {
                id: button
                required property string modelData
                required property int index
                readonly property var info: root.known[modelData]
                readonly property bool checked: root.checkedButton === modelData
                readonly property bool dragged: root.dragging && root.dragName === modelData
                readonly property bool lifted: mouse.dragging
                readonly property real offsetX: !dragged ? root.shiftOf(index)
                                                : root.dragSettling ? (root.dragTo - root.dragFrom) * root.dragStep : root.dragDx

                function cancelDrag() {
                    mouse.dragging = false
                    mouse.suppressClick = true
                }

                width: Theme.toolbar.size
                height: Theme.toolbar.size
                radius: Theme.toolbar.radius
                readonly property color restFill: mouse.containsMouse ? Theme.toolbar.hoverBackground
                                                                      : Qt.alpha(Theme.toolbar.hoverBackground, 0)
                fill: checked ? Theme.toolbar.checkedBackground : restFill
                fillEnd: checked ? Theme.toolbar.checkedBackgroundEnd : restFill
                direction: Theme.layout.gradientDirection
                scale: lifted ? Theme.toolbar.dragScale : mouse.pressed ? Theme.motion.pressScale : 1
                opacity: lifted && !root.dragSettling ? Theme.toolbar.dragOpacity : 1
                z: lifted || offsetX !== 0 ? 1 : 0

                // Not at creation: when the toolbar reloads in its new order, the new buttons first see the
                // drag's offsets and drop them in the same event, so nothing should animate yet.
                property bool ready: false
                Component.onCompleted: Qt.callLater(() => { ready = true })

                Behavior on scale { enabled: Motion.on; MotionNumber {} }
                Behavior on fill { enabled: Motion.fades; MotionColor {} }
                Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
                Behavior on opacity { enabled: Motion.fades && button.ready; MotionNumber { fade: true } }

                transform: Translate {
                    x: button.offsetX
                    Behavior on x { enabled: Motion.on && button.ready && !(button.dragged && !root.dragSettling); MotionSpring {} }
                }

                Icon {
                    anchors.centerIn: parent
                    size: Theme.toolbar.iconSize
                    color: button.checked ? Theme.toolbar.checkedIcon : Theme.toolbar.icon
                    path: button.checked && button.info.checkedIcon ? button.info.checkedIcon : button.info.icon

                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                }

                MouseArea {
                    id: mouse
                    property real pressX
                    property real pressY
                    property bool dragging: false
                    property bool suppressClick: false

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor

                    onPressed: m => {
                        pressX = m.x
                        pressY = m.y
                        suppressClick = false
                    }
                    onPositionChanged: m => {
                        if (!pressed)
                            return
                        if (!dragging) {
                            if (Math.hypot(m.x - pressX, m.y - pressY) <= Qt.styleHints.startDragDistance)
                                return
                            dragging = true
                            root.startDrag(button, button.index, pressX, pressY)
                        }
                        root.moveDrag(button, m.x, m.y)
                    }
                    onReleased: {
                        const wasDragging = dragging
                        dragging = false
                        if (wasDragging) {
                            suppressClick = true
                            root.endDrag(true)
                        }
                    }
                    onCanceled: {
                        const wasDragging = dragging
                        dragging = false
                        if (wasDragging)
                            root.endDrag(false)
                    }
                    onClicked: {
                        if (!suppressClick)
                            root.triggered(button.info.action)
                    }
                }

                ToolTip.visible: mouse.containsMouse && !mouse.dragging && !root.dragging
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: Actions.tip(button.info.tip, Actions.keys[button.info.action] ?? [])
            }
        }
    }

    Loader {
        id: menu
        property double closedAt: 0
        active: false
        source: "ToolbarMenu.qml"
    }
    Binding {
        target: menu.item
        property: "known"
        value: root.known
    }
    Connections {
        target: menu.item
        function onClosed() { menu.closedAt = Date.now() }
        function onMessage(text) { root.message(text) }
    }
}
