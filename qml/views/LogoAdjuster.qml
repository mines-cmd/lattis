import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    required property var hero

    readonly property bool busy: dragging || nudgeTimer.running
    property bool dragging: false
    readonly property int cursor: area.containsMouse || area.pressed ? area.cursorShape : Qt.ArrowCursor
    property real guideX: NaN
    property real guideY: NaN

    property bool dirty: false
    property Item previousFocus: null
    property var original: ({})

    anchors.fill: parent
    focus: true

    Component.onCompleted: {
        previousFocus = Window.window?.activeFocusItem ?? null
        original = { scale: hero.game.logoScale ?? 1, position: hero.game.logoPosition ?? "",
                     x: hero.game.logoOffsetX ?? 0, y: hero.game.logoOffsetY ?? 0 }
        forceActiveFocus()
    }
    onEnabledChanged: {
        if (!enabled && previousFocus)
            previousFocus.forceActiveFocus()
    }
    Component.onDestruction: {
        if (enabled && previousFocus)
            previousFocus.forceActiveFocus()
    }

    Connections {
        target: root.hero
        function onLogoOnlyChanged() {
            if (root.hero.logoOnly)
                root.finish()
        }
    }

    function round4(value: real): real {
        return Math.round(value * 10000) / 10000
    }

    function spot(h: string, v: string, w: real, ht: real): point {
        const edge = hero.edge
        return Qt.point(h === "left" ? edge : h === "right" ? hero.width - edge - w : (hero.width - w) / 2,
                        v === "top" ? edge : v === "bottom" ? hero.height - edge - ht : (hero.height - ht) / 2)
    }

    function anchorName(h: string, v: string): string {
        if (v === "center")
            return h === "center" ? "center" : "center_" + h
        return v + "_" + h
    }

    function place(x: real, y: real) {
        const t = hero.target
        const s = spot(hero.hAlign, hero.vAlign, t.width, t.height)
        hero.pendingOffsetX = (x - s.x) / hero.height
        hero.pendingOffsetY = (y - s.y) / hero.height
    }

    function commit(withScale: bool) {
        nudgeTimer.stop()
        const t = hero.target
        const cx = t.x + t.width / 2
        const cy = t.y + t.height / 2
        const h = cx < hero.width / 3 ? "left" : cx > hero.width * 2 / 3 ? "right" : "center"
        const v = cy < hero.height / 3 ? "top" : cy > hero.height * 2 / 3 ? "bottom" : "center"
        const name = anchorName(h, v)
        const s = spot(h, v, t.width, t.height)
        const ox = round4((t.x - s.x) / hero.height)
        const oy = round4((t.y - s.y) / hero.height)
        hero.pendingPosition = name
        hero.pendingOffsetX = ox
        hero.pendingOffsetY = oy
        const values = {
            logo_position: name === Theme.hero.logoPosition && ox === 0 && oy === 0 ? null : name,
            logo_offset_x: ox === 0 ? null : ox,
            logo_offset_y: oy === 0 ? null : oy
        }
        if (withScale) {
            const scale = Math.round(hero.logoScale * 100) / 100
            values.logo_scale = Math.abs(scale - 1) < 0.005 ? null : scale
        }
        dirty = true
        hero.save(values)
    }

    function finish() {
        if (nudgeTimer.running)
            commit(false)
        hero.adjusting = false
    }

    function undo() {
        nudgeTimer.stop()
        if (dirty) {
            hero.pendingScale = original.scale
            hero.pendingPosition = original.position !== "" ? original.position : Theme.hero.logoPosition
            hero.pendingOffsetX = original.x
            hero.pendingOffsetY = original.y
            hero.save({
                logo_scale: Math.abs(original.scale - 1) < 0.005 ? null : original.scale,
                logo_position: original.position !== "" ? original.position : null,
                logo_offset_x: original.x !== 0 ? original.x : null,
                logo_offset_y: original.y !== 0 ? original.y : null
            })
        }
        hero.adjusting = false
    }

    function nudge(dx: real, dy: real) {
        const t = hero.target
        place(Math.max(0, Math.min(hero.width - t.width, t.x + dx)), Math.max(0, Math.min(hero.height - t.height, t.y + dy)))
        nudgeTimer.restart()
    }

    Timer {
        id: nudgeTimer
        interval: 400
        onTriggered: root.commit(false)
    }

    function handled(key: int): bool {
        return key === Qt.Key_Escape || key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Left
            || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down
    }

    Keys.onShortcutOverride: event => event.accepted = handled(event.key)
    Keys.onPressed: event => {
        const step = event.modifiers & Qt.ShiftModifier ? 10 : 1
        switch (event.key) {
        case Qt.Key_Escape: undo(); break
        case Qt.Key_Return:
        case Qt.Key_Enter: finish(); break
        case Qt.Key_Left: nudge(-step, 0); break
        case Qt.Key_Right: nudge(step, 0); break
        case Qt.Key_Up: nudge(0, -step); break
        case Qt.Key_Down: nudge(0, step); break
        default: return
        }
        event.accepted = true
    }

    component Guide: Rectangle {
        property real at: NaN
        property real lastAt: 0
        onAtChanged: if (!isNaN(at)) lastAt = at
        visible: opacity > 0
        opacity: isNaN(at) ? 0 : 1
        color: Theme.hero.guideColor

        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
    }
    Guide {
        at: root.guideX
        x: lastAt - Theme.hero.guideWidth / 2
        width: Theme.hero.guideWidth
        height: parent.height
    }
    Guide {
        at: root.guideY
        y: lastAt - Theme.hero.guideWidth / 2
        width: parent.width
        height: Theme.hero.guideWidth
    }

    Rectangle {
        x: root.hero.target.x - border.width
        y: root.hero.target.y - border.width
        width: root.hero.target.width + 2 * border.width
        height: root.hero.target.height + 2 * border.width
        color: "transparent"
        border.color: Theme.hero.resizeFrame
        border.width: Theme.hero.resizeFrameWidth

        Rectangle {
            width: Theme.hero.resizeHandleSize
            height: width
            radius: width / 4
            color: Theme.hero.resizeFrame
            x: root.hero.hAlign === "right" ? -width / 2 : parent.width - width / 2
            y: root.hero.vAlign === "bottom" ? -height / 2 : parent.height - height / 2
        }
    }

    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: root.hero.edge / 2 }
        width: hint.implicitWidth + 24
        height: hint.implicitHeight + 12
        radius: Theme.toast.radius
        color: Theme.toast.background
        border.color: Theme.toast.border
        border.width: Theme.toast.borderWidth

        Label {
            id: hint
            anchors.centerIn: parent
            text: qsTr("Drag to move · Corner to resize %1 · Esc to undo").arg(Math.round(root.hero.logoScale * 100) + "%")
            color: Theme.toast.text
        }
    }

    MouseArea {
        id: area

        property string hover: "none"
        property string mode: "none"
        property point pressPoint
        property rect startRect
        property point anchorPoint
        property real startDistance: 1
        property real startScale: 1
        property bool moved: false

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        cursorShape: (pressed ? mode : hover) === "none" ? Qt.ArrowCursor
            : pressed && mode === "move" ? Qt.ClosedHandCursor : Qt.PointingHandCursor

        function modeAt(x: real, y: real): string {
            const t = root.hero.target
            const reach = Math.max(Theme.hero.resizeHandleSize, 16) * 0.75
            const hx = root.hero.hAlign === "right" ? t.x : t.x + t.width
            const hy = root.hero.vAlign === "bottom" ? t.y : t.y + t.height
            if (Math.abs(x - hx) <= reach && Math.abs(y - hy) <= reach)
                return "resize"
            return x >= t.x && x <= t.x + t.width && y >= t.y && y <= t.y + t.height ? "move" : "none"
        }

        function distance(x: real, y: real): real {
            return Math.max(1, Math.hypot(x - anchorPoint.x, y - anchorPoint.y))
        }

        function snapped(pairs: var): var {
            let best = null
            for (const [position, line] of pairs) {
                const d = line - position
                if (Math.abs(d) <= Theme.hero.snapDistance && (best === null || Math.abs(d) < Math.abs(best.delta)))
                    best = { delta: d, line: line }
            }
            return best
        }

        onPressed: mouse => {
            mode = modeAt(mouse.x, mouse.y)
            pressPoint = Qt.point(mouse.x, mouse.y)
            startRect = root.hero.target
            moved = false
            if (mode === "resize") {
                const t = startRect
                anchorPoint = Qt.point(root.hero.hAlign === "left" ? t.x : root.hero.hAlign === "right" ? t.x + t.width : t.x + t.width / 2,
                                       root.hero.vAlign === "top" ? t.y : root.hero.vAlign === "bottom" ? t.y + t.height : t.y + t.height / 2)
                startDistance = distance(mouse.x, mouse.y)
                startScale = root.hero.logoScale
            }
            root.dragging = mode !== "none"
        }
        onPositionChanged: mouse => {
            if (!pressed) {
                hover = modeAt(mouse.x, mouse.y)
                return
            }
            if (mode === "resize") {
                moved = true
                const scale = startScale * distance(mouse.x, mouse.y) / startDistance
                root.hero.pendingScale = Math.max(root.hero.minScale, Math.min(root.hero.maxScale, scale))
            } else if (mode === "move") {
                moved = true
                const r = startRect
                let x = r.x + mouse.x - pressPoint.x
                let y = r.y + mouse.y - pressPoint.y
                root.guideX = NaN
                root.guideY = NaN
                if (Theme.hero.snapDistance > 0 && !(mouse.modifiers & Qt.ShiftModifier)) {
                    const edge = root.hero.edge
                    const sx = snapped([[x, edge], [x + r.width, root.hero.width - edge], [x + r.width / 2, root.hero.width / 2]])
                    const sy = snapped([[y, edge], [y + r.height, root.hero.height - edge], [y + r.height / 2, root.hero.height / 2]])
                    if (sx) {
                        x += sx.delta
                        root.guideX = sx.line
                    }
                    if (sy) {
                        y += sy.delta
                        root.guideY = sy.line
                    }
                }
                x = Math.max(0, Math.min(root.hero.width - r.width, x))
                y = Math.max(0, Math.min(root.hero.height - r.height, y))
                root.place(x, y)
            }
        }
        onReleased: {
            root.dragging = false
            root.guideX = NaN
            root.guideY = NaN
            if (mode === "none")
                root.finish()
            else if (moved)
                root.commit(mode === "resize")
            mode = "none"
        }
    }
}
