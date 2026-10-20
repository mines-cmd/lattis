import QtQuick
import QtQuick.Controls.Basic

Surface {
    id: root

    property string icon
    property string tip
    property bool selected: false
    property bool draggable: false
    property real offsetY: 0
    // offsetY follows the pointer, so it must not animate.
    property bool following: false
    readonly property bool lifted: draggable && (mouse.dragging || (mouse.armed && Theme.rail.dragHold > 0 && mouse.pressed))

    signal clicked()
    signal menuRequested()
    property bool hasMenu: false
    property bool menuOpen: false
    readonly property bool hot: mouse.containsMouse || menuOpen
    // Pointer positions are in this button's coordinates: map them with mapToItem(), since the
    // button itself moves while it's dragged.
    signal dragStarted(real x, real y)
    signal dragMoved(real x, real y)
    signal dragEnded(bool dropped)

    function cancelDrag() {
        mouse.dragging = false
        mouse.armed = false
        mouse.suppressClick = true
    }

    implicitWidth: Theme.rail.buttonSize
    implicitHeight: Theme.rail.buttonSize
    radius: Theme.rail.buttonRadius
    fill: selected ? Theme.rail.selectedBackground
                   : root.hot ? Theme.rail.hoverBackground : idle
    fillEnd: selected ? Theme.rail.selectedBackgroundEnd
                      : root.hot ? Theme.rail.hoverBackground : idle
    direction: Theme.rail.gradientDirection
    scale: lifted ? Theme.rail.dragScale : mouse.pressed ? Theme.motion.pressScale : 1
    // Invisible, but the hover color's own, so the fade to and from it never passes through black.
    readonly property color idle: Qt.alpha(Theme.rail.hoverBackground, 0)

    z: lifted || offsetY !== 0 ? 1 : 0

    // Not at creation: when the rail reloads in its new order, the new buttons first see the drag's offsets,
    // and drop them in the same event, so nothing should animate yet.
    property bool ready: false
    Component.onCompleted: Qt.callLater(() => { ready = true })

    Behavior on scale { enabled: Motion.on; MotionNumber {} }
    Behavior on fill { enabled: Motion.fades; MotionColor {} }
    Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
    // Off for a button whose opacity is already an eased value (the rail's `shown`): easing it again makes it lag behind.
    property bool easeOpacity: true
    Behavior on opacity { enabled: Motion.fades && root.ready && root.easeOpacity; MotionNumber { fade: true } }

    // Separate from `scale`, which the drag and the press own.
    property real popScale: 1

    transform: [
        Translate {
            y: root.offsetY
            Behavior on y { enabled: Motion.on && root.ready && !root.following; MotionSpring {} }
        },
        Scale {
            origin.x: root.width / 2
            origin.y: root.height / 2
            xScale: root.popScale
            yScale: root.popScale
        }
    ]

    Icon {
        anchors.centerIn: parent
        path: root.icon
        size: Theme.rail.iconSize
        color: root.selected ? Theme.rail.selectedIcon
                             : root.hot ? Theme.rail.iconHover : Theme.rail.icon

        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }

    MouseArea {
        id: mouse
        property real pressX
        property real pressY
        property bool armed: false
        property bool dragging: false
        property bool suppressClick: false

        anchors.fill: parent
        acceptedButtons: root.hasMenu ? Qt.LeftButton | Qt.RightButton : Qt.LeftButton
        hoverEnabled: true
        cursorShape: dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor

        function finish() {
            holdTimer.stop()
            armed = false
            dragging = false
        }

        onPressed: m => {
            if (m.button === Qt.RightButton) {
                suppressClick = true
                root.menuRequested()
                return
            }
            pressX = m.x
            pressY = m.y
            suppressClick = false
            if (!root.draggable)
                return
            if (Theme.rail.dragHold > 0)
                holdTimer.restart()
            else
                armed = true
        }
        onPositionChanged: m => {
            if (!pressed || !root.draggable)
                return
            if (!dragging) {
                // Moving before the hold is over is a swipe or a slipped click, never a drag.
                if (Math.hypot(m.x - pressX, m.y - pressY) <= Qt.styleHints.startDragDistance)
                    return
                if (!armed) {
                    holdTimer.stop()
                    return
                }
                dragging = true
                root.dragStarted(pressX, pressY)
            }
            root.dragMoved(m.x, m.y)
        }
        onReleased: {
            const wasDragging = dragging
            finish()
            if (wasDragging) {
                suppressClick = true
                root.dragEnded(true)
            }
        }
        onCanceled: {
            const wasDragging = dragging
            finish()
            if (wasDragging)
                root.dragEnded(false)
        }
        onClicked: {
            if (!suppressClick)
                root.clicked()
        }

        Timer {
            id: holdTimer
            interval: Theme.rail.dragHold
            onTriggered: mouse.armed = true
        }
    }

    ToolTip.visible: mouse.containsMouse && !root.lifted && !mouse.dragging && !root.menuOpen
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: root.tip
}
