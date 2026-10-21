import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

Menu {
    id: root

    property Item growFrom: null
    readonly property bool grows: growFrom !== null

    // For a menu that opens beside what opened it, or at the cursor, rather than under a field:
    // dropping down from nothing would look detached, so it springs out of the corner it opened
    // from and its entries fade in one after another, starting from that corner.
    property bool pops: atPointer
    property bool atPointer: false
    property string popDirection: "right"
    property real slide: 0
    readonly property real slideX: popDirection === "right" ? slide : 0
    readonly property real slideY: popDirection === "down" || popDirection === "below" ? slide : popDirection === "up" ? -slide : 0
    // Under a button at the menu's right edge, it grows only downward: grown from that corner, its whole left
    // edge travelled, and the menu looked like it opened off to the right and slid into place.
    property real pop: 1
    readonly property real popY: popDirection === "below" ? pop : 1
    scale: popDirection === "below" ? 1 : pop

    clip: !grows
    property real reveal: 1
    height: grows ? implicitHeight : implicitHeight * reveal
    transformOrigin: !pops ? Popup.Center : popDirection === "up" ? Popup.BottomLeft : Popup.TopLeft
    readonly property Transition dropEnter: DropdownEnter {}
    readonly property Transition dropExit: DropdownExit {}
    readonly property Transition popEnter: MenuPopEnter {}
    readonly property Transition popExit: MenuPopExit {}
    enter: pops ? popEnter : dropEnter
    exit: pops ? popExit : dropExit

    property Item openSubmenuItem: null
    property point lastPointer: Qt.point(-1, -1)

    // Whether the pointer, moving from lastPointer to `p`, is heading for the open submenu: inside the
    // triangle from lastPointer to the submenu's near corners, taken a little past them so a wobble on the way
    // still counts. (A tip behind lastPointer would forgive more, but catch moves straight down the menu.)
    function headingToSubmenu(p) {
        const sub = openSubmenuItem?.subMenu?.background
        if (!sub || lastPointer.x < 0)
            return false
        const corner = sub.mapToItem(null, 0, 0)
        const right = corner.x >= p.x
        const x = right ? corner.x : corner.x + sub.width
        const a = lastPointer, b = Qt.point(x, corner.y - 40), c = Qt.point(x, corner.y + sub.height + 40)
        const side = (u, v, w) => (u.x - w.x) * (v.y - w.y) - (v.x - w.x) * (u.y - w.y)
        const d1 = side(p, a, b), d2 = side(p, b, c), d3 = side(p, c, a)
        return !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0))
    }

    // For a button's chevron: opened is false while the enter transition runs, and visible stays
    // true through the exit one, so either would turn the chevron late.
    property bool showing: false
    Component.onCompleted: {
        aboutToShow.connect(() => {
            showing = true
            flatten(false)
            if (atPointer) {
                const asked = y
                popDirection = Qt.binding(() => y < asked ? "up" : "down")
            }
        })
        aboutToHide.connect(() => {
            showing = false
            flatten(pops)
        })
        closed.connect(() => flatten(false))
    }
    // Faded as separate items, the entries showed over whatever was behind the see-through panel, so
    // a popping menu closes as one flattened image. Only then: the layer softens text.
    function flatten(on) {
        const popupItem = background?.parent
        if (popupItem) {
            popupItem.layer.smooth = true
            popupItem.layer.enabled = on
        }
    }

    property real minWidth: Theme.menu.minWidth
    property bool checkColumn: false
    property string iconPath: ""
    readonly property bool iconColumn: {
        if (!Theme.menu.icons) return false
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item && (item.want ?? item.visible) && (item.iconPath || item.subMenu?.iconPath)) return true
        }
        return false
    }

    padding: Theme.menu.padding
    width: {
        let widest = implicitBackgroundWidth - leftPadding - rightPadding
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item && (item.want ?? item.visible))
                widest = Math.max(widest, item.naturalWidth ?? item.implicitWidth)
        }
        return widest + leftPadding + rightPadding
    }

    delegate: SettingsMenuItem {}

    // Basic's list, plus a cache that lays out every entry. While the menu drops down, the list
    // is shorter than its entries; without it, contentHeight is estimated from the laid-out ones,
    // and a zero-height hidden entry throws that off, so the height snaps once the rest appear.
    contentItem: ListView {
        id: entries
        implicitHeight: contentHeight
        // Dropping down, the entries slide with the bottom edge like a drawer instead of staying put under
        // a window that grows (the popup clips them).
        readonly property real drawerShift: root.grows || root.pops ? 0 : (1 - root.reveal) * contentHeight
        transform: [
            Scale { origin.y: -entries.y; yScale: root.popY },
            Translate { x: root.slideX; y: root.slideY - entries.drawerShift }
        ]
        model: root.contentModel
        interactive: Window.window
                     ? contentHeight + root.topPadding + root.bottomPadding > root.height
                     : false
        clip: root.grows || root.reveal >= 1
        currentIndex: root.currentIndex
        cacheBuffer: 10000

        // While growing, the entries show only inside the outline. The layer softens text, so it's
        // dropped once the menu is fully open.
        layer.enabled: root.grows && root.reveal < 1
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: growMask
        }

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: Rectangle {
        id: outline
        transform: [
            Scale { yScale: root.popY },
            Translate { x: root.slideX; y: root.slideY }
        ]

        // The popup keeps its full size while growing; only its outline moves, from a sliver under the
        // button as wide as it.
        readonly property real fromX: root.grows ? -root.x : 0
        readonly property real fromWidth: root.grows ? root.growFrom.width : root.width
        readonly property real t: root.grows ? root.reveal : 1

        x: fromX * (1 - t)
        width: fromWidth + (root.width - fromWidth) * t
        height: root.height * t
        implicitWidth: root.minWidth
        radius: Theme.controls.inputRadius
        color: Theme.menu.popupBackground
        border.color: Theme.controls.inputBorder
        border.width: Math.max(1, Theme.controls.inputBorderWidth)

        Item {
            id: growMask
            visible: false
            layer.enabled: root.grows
            width: root.contentItem.width
            height: root.contentItem.height

            Rectangle {
                x: outline.x - root.contentItem.x
                y: outline.y - root.contentItem.y
                width: outline.width
                height: outline.height
                radius: outline.radius
            }
        }
    }
}
