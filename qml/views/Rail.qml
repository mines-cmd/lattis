import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    signal openSettings()
    signal addGame()
    signal action(string name)
    signal editTheme()
    signal message(string text)
    signal undoable(string text, var link)
    signal editPlatform(string id)

    property string dragId: ""
    property int dragFrom: -1
    property int dragTo: -1
    property real dragPressY: 0
    property real dragDy: 0
    property bool dragOutside: false
    property bool dragSettling: false
    // The rail's order when the drag began: once the rail reloads in the new order the drag is over, so the
    // rebuilt buttons start in place instead of animating from where the drag had put them.
    property string dragOrder: ""
    readonly property bool dragging: dragId !== "" && Library.platforms.map(p => p.id).join(",") === dragOrder
    property Item firstButton: null
    readonly property real dragStep: firstButton ? firstButton.height + platformColumn.spacing : 0
    property real dragPointerY: 0

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
        dragId = Library.platforms[index].id
        dragOrder = Library.platforms.map(p => p.id).join(",")
        dragFrom = index
        dragTo = index
        dragPressY = button.mapToItem(platformColumn, x, y).y
        dragPointerY = button.mapToItem(scroller, x, y).y
        dragDy = 0
        dragOutside = false
        dragSettling = false
    }

    function moveDrag(button, x, y) {
        if (!dragging || dragSettling)
            return
        const p = button.mapToItem(panel, x, y)
        dragPointerY = button.mapToItem(scroller, x, y).y
        dragDy = button.mapToItem(platformColumn, x, y).y - dragPressY
        dragOutside = p.x < 0 || p.x > panel.width || p.y < 0 || p.y > panel.height
        updateDragTo()
    }

    function updateDragTo() {
        const last = platformRepeater.count - 1
        dragDy = Math.max(-dragFrom * dragStep, Math.min((last - dragFrom) * dragStep, dragDy))
        dragTo = dragOutside ? dragFrom : Math.max(0, Math.min(last, dragFrom + Math.round(dragDy / dragStep)))
    }

    function scrollDuringDrag() {
        const edge = dragStep
        const speed = Theme.rail.buttonSize / 4
        const wanted = dragPointerY < edge ? -speed : dragPointerY > scroller.height - edge ? speed : 0
        const before = scroller.contentY
        scroller.contentY = Math.max(0, Math.min(scroller.contentHeight - scroller.height, before + wanted))
        dragDy += scroller.contentY - before
        updateDragTo()
    }

    function showSelected() {
        const i = Library.platforms.findIndex(p => p.id === Library.platformFilter)
        const button = platformRepeater.itemAt(i)
        if (!button)
            return
        const top = button.y
        const bottom = top + button.height
        if (top < scroller.contentY)
            scroller.contentY = top
        else if (bottom > scroller.contentY + scroller.height)
            scroller.contentY = bottom - scroller.height
    }

    function endDrag(dropped) {
        if (!dragging || dragSettling)
            return
        if (!dropped || dragOutside || dragTo === dragFrom) {
            resetDrag()
            return
        }
        // Before the platform that follows it in its new place; the file also holds platforms
        // without games, which the rail doesn't show, so a step count wouldn't mean the same there.
        const others = Library.platforms.filter((p, i) => i !== dragFrom)
        const error = Library.movePlatformBefore(dragId, others[dragTo]?.id ?? "")
        if (error !== "") {
            resetDrag()
            message(error)
            return
        }
        dragSettling = true
        settleTimer.restart()
    }

    function dragForScreenshot(id, offset) {
        const from = Library.platforms.findIndex(p => p.id === id)
        if (from < 0)
            return
        startDrag(platformRepeater.itemAt(from), from, 0, 0)
        const mid = platformRepeater.itemAt(from).height / 2
        const button = platformRepeater.itemAt(from)
        dragPressY = button.mapToItem(platformColumn, 0, mid).y
        moveDrag(button, button.width / 2, mid + offset)
    }

    function cancelDrag() {
        platformRepeater.itemAt(dragFrom)?.cancelDrag()
        resetDrag()
    }

    function resetDrag() {
        settleTimer.stop()
        dragId = ""
        dragFrom = -1
        dragTo = -1
        dragDy = 0
        dragOutside = false
        dragSettling = false
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.dragging && !root.dragSettling
        onActivated: root.cancelDrag()
    }

    Connections {
        target: Library
        function onPlatformsChanged() { root.resetDrag() }
    }

    Timer {
        id: settleTimer
        interval: 800
        onTriggered: root.resetDrag()
    }

    readonly property var bottomDefs: ({
        add_game: { icon: Icons.add, tip: Actions.tip(qsTr("Add game"), Actions.keys.add_game), run: () => root.addGame() },
        play_statistics: { icon: Icons.barChartLine, tip: Actions.tip(qsTr("Play statistics"), Actions.keys.play_statistics),
                           run: () => root.action("play_statistics") },
        open_settings: { icon: Icons.gear, tip: Actions.tip(qsTr("Settings"), Actions.keys.open_settings),
                         run: () => root.openSettings() }
    })
    readonly property var bottomShown: General.railButtonOrder.filter(name => !General.railHiddenButtons.includes(name))

    property string bottomDragName: ""
    property int bottomFrom: -1
    property int bottomTo: -1
    property real bottomPressY: 0
    property real bottomDy: 0
    property bool bottomOutside: false
    property bool bottomSettling: false
    property string bottomOrder: ""
    readonly property bool bottomDragging: bottomDragName !== "" && bottomShown.join(",") === bottomOrder
    readonly property real bottomStep: Theme.rail.buttonSize + bottomColumn.spacing

    function bottomShiftOf(index) {
        if (!bottomDragging || index === bottomFrom)
            return 0
        if (bottomTo > bottomFrom && index > bottomFrom && index <= bottomTo)
            return -bottomStep
        if (bottomTo < bottomFrom && index >= bottomTo && index < bottomFrom)
            return bottomStep
        return 0
    }

    function startBottomDrag(button, index, x, y) {
        bottomDragName = bottomShown[index]
        bottomOrder = bottomShown.join(",")
        bottomFrom = index
        bottomTo = index
        bottomPressY = button.mapToItem(bottomColumn, x, y).y
        bottomDy = 0
        bottomOutside = false
        bottomSettling = false
    }

    function moveBottomDrag(button, x, y) {
        if (!bottomDragging || bottomSettling)
            return
        const p = button.mapToItem(panel, x, y)
        bottomDy = button.mapToItem(bottomColumn, x, y).y - bottomPressY
        bottomOutside = p.x < 0 || p.x > panel.width || p.y < 0 || p.y > panel.height
        const last = bottomShown.length - 1
        bottomDy = Math.max(-bottomFrom * bottomStep, Math.min((last - bottomFrom) * bottomStep, bottomDy))
        bottomTo = bottomOutside ? bottomFrom : Math.max(0, Math.min(last, bottomFrom + Math.round(bottomDy / bottomStep)))
    }

    function endBottomDrag(dropped) {
        if (!bottomDragging || bottomSettling)
            return
        if (!dropped || bottomOutside || bottomTo === bottomFrom) {
            resetBottomDrag()
            return
        }
        // The hidden buttons keep their places among the shown ones' slots.
        const moved = bottomShown.slice()
        moved.splice(bottomTo, 0, moved.splice(bottomFrom, 1)[0])
        let next = 0
        const order = General.railButtonOrder.map(name => General.railHiddenButtons.includes(name) ? name : moved[next++])
        const error = General.setRailButtonOrder(order)
        if (error !== "") {
            resetBottomDrag()
            message(error)
            return
        }
        bottomSettling = true
        bottomSettleTimer.restart()
    }

    function cancelBottomDrag() {
        bottomRepeater.itemAt(bottomFrom)?.cancelDrag()
        resetBottomDrag()
    }

    function resetBottomDrag() {
        bottomSettleTimer.stop()
        bottomDragName = ""
        bottomFrom = -1
        bottomTo = -1
        bottomDy = 0
        bottomOutside = false
        bottomSettling = false
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.bottomDragging && !root.bottomSettling
        onActivated: root.cancelBottomDrag()
    }

    Connections {
        target: General
        function onRailButtonOrderChanged() { root.resetBottomDrag() }
        function onRailHiddenButtonsChanged() { root.resetBottomDrag() }
    }

    Timer {
        id: bottomSettleTimer
        interval: 800
        onTriggered: root.resetBottomDrag()
    }

    function openMenu() {
        appMenu.active = true
        const menu = appMenu.item
        if (menu.visible) {
            menu.close()
            return
        }
        // The press on the logo that just closed it (a press outside the menu closes it, then
        // reaches the logo) doesn't open it again.
        if (Date.now() - appMenu.closedAt < 200)
            return
        if (logo.visible)
            menu.popup(panel, panel.width + Theme.layout.spacing / 2, logo.mapToItem(panel, 0, 0).y)
        else
            menu.popup(panel, Theme.layout.spacing, Theme.layout.margin)
    }

    function openPlatformMenu(button, platform) {
        platformMenu.active = true
        const menu = platformMenu.item
        if (menu.visible) {
            // The right press outside the menu has already started closing it by the time it reaches
            // the button, and popup() during the exit transition only moves the closing menu.
            platformMenu.next = platform.id === menu.platform.id ? null : { button, platform }
            menu.close()
            return
        }
        if (platform.id === menu.platform.id && Date.now() - platformMenu.closedAt < 200)
            return
        menu.platform = platform
        menu.popup(panel, panel.width + Theme.layout.spacing / 2, button.mapToItem(panel, 0, 0).y)
    }

    function openPlatformMenuForScreenshot(id) {
        const i = Library.platforms.findIndex(p => p.id === id)
        const button = platformRepeater.itemAt(i)
        if (button)
            openPlatformMenu(button, Library.platforms[i])
    }

    function hidePlatform(id) {
        const platform = Library.platforms.find(p => p.id === id)
        const error = Library.setPlatformValue(id, "in_rail", false)
        if (error !== "") {
            root.message(error)
            return
        }
        if (Library.platformFilter === id)
            Library.platformFilter = ""
        root.undoable(qsTr("%1 is hidden from the rail").arg(platform?.name ?? id), {
            text: qsTr("Undo"),
            isUndo: true,
            run: () => {
                const undoError = Library.setPlatformValue(id, "in_rail", true)
                if (undoError !== "")
                    root.message(undoError)
            }
        })
    }

    Loader {
        id: platformMenu
        property double closedAt: 0
        property var next: null
        active: false
        source: "PlatformMenu.qml"
    }
    Connections {
        target: platformMenu.item
        function onClosed() {
            platformMenu.closedAt = Date.now()
            const next = platformMenu.next
            platformMenu.next = null
            // Later: right after closed(), Popup restores the opacity and scale the exit transition
            // changed, which would show the reopened menu whole for a frame before it springs in.
            if (next)
                Qt.callLater(root.openPlatformMenu, next.button, next.platform)
        }
        function onEditRequested(id) { root.editPlatform(id) }
        function onHideRequested(id) { root.hidePlatform(id) }
    }

    function openRailMenu(y) {
        railMenu.active = true
        const menu = railMenu.item
        if (menu.visible) {
            menu.close()
            return
        }
        if (Date.now() - railMenu.closedAt < 200)
            return
        menu.popup(panel, panel.width + Theme.layout.spacing / 2, y)
    }

    Loader {
        id: railMenu
        property double closedAt: 0
        active: false
        source: "RailMenu.qml"
    }
    Connections {
        target: railMenu.item
        function onClosed() { railMenu.closedAt = Date.now() }
        function onMessage(text) { root.message(text) }
        function onPlatformHidden(id) {
            if (Library.platformFilter === id)
                Library.platformFilter = ""
        }
    }

    function openSubmenuForScreenshot(which) {
        appMenu.item?.openSubmenu(which)
    }

    Loader {
        id: appMenu
        property double closedAt: 0
        active: false
        source: "AppMenu.qml"
    }
    Connections {
        target: appMenu.item
        function onClosed() { appMenu.closedAt = Date.now() }
        function onChosen(name) { root.action(name) }
        function onEditTheme() { root.editTheme() }
    }

    readonly property int inset: Theme.rail.container ? Theme.rail.containerMargin : 0

    property bool ready: false
    Component.onCompleted: Qt.callLater(() => { ready = true })

    property real shown: General.railHidden ? 0 : 1
    Behavior on shown { enabled: Motion.on && root.ready; MotionNumber { speed: "normal" } }

    Connections {
        target: General
        function onRailHiddenChanged() {
            if (!General.railHidden)
                return
            if (root.dragging)
                root.cancelDrag()
            if (root.bottomDragging)
                root.cancelBottomDrag()
        }
    }

    Surface {
        id: panel
        opacity: root.shown
        visible: opacity > 0
        anchors.fill: parent
        anchors.margins: root.inset
        radius: Theme.rail.container ? Theme.rail.containerRadius : 0
        fill: Theme.rail.background
        fillEnd: Theme.rail.backgroundEnd
        direction: Theme.rail.gradientDirection
        border.color: Theme.rail.border
        border.width: Theme.rail.container ? Theme.rail.borderWidth : 0
    }

    MouseArea {
        anchors.fill: panel
        enabled: !General.railHidden
        acceptedButtons: Qt.RightButton
        onPressed: mouse => root.openRailMenu(mouse.y)
    }

    ColumnLayout {
        id: column
        anchors.fill: panel
        // Measured from the window edge, so the logo lines up with the search box and the
        // first button with the hero whatever the inset.
        anchors.topMargin: Math.max(0, Theme.layout.margin - root.inset)
        anchors.bottomMargin: Math.max(0, Theme.layout.margin - root.inset)
        spacing: Theme.layout.spacing

        AppLogo {
            id: logo
            visible: Theme.rail.showLogo
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            // A left margin moves a centered item by half of it: while hidden the logo shifts right by half the list's
            // padding, which puts it as far from the search box as from the window's edge.
            Layout.leftMargin: Theme.list.padding * (1 - root.shown)
            highlighted: logoMouse.containsMouse || (appMenu.item?.visible ?? false)

            MouseArea {
                id: logoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressed: {
                    logoDip.restart()
                    root.openMenu()
                }
            }
            // A dip and back rather than following `pressed`: the menu opens on the press and
            // takes the pointer, so `pressed` would let go before the dip showed.
            SequentialAnimation {
                id: logoDip
                MotionNumber { target: logo; property: "scale"; to: Theme.motion.pressScale }
                MotionPop { target: logo; property: "scale"; to: 1; speed: "fast" }
            }

            ToolTip.visible: logoMouse.containsMouse && !(appMenu.item?.visible ?? false)
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: Actions.tip(qsTr("Menu"), Actions.keys.app_menu)
        }

        RailButton {
            Layout.alignment: Qt.AlignHCenter
            opacity: root.shown
            easeOpacity: false
            visible: opacity > 0
            enabled: !General.railHidden
            icon: Icons.library
            tip: Actions.tip(qsTr("All games"), Actions.keys.show_all)
            selected: Library.platformFilter === ""
            onClicked: Library.platformFilter = ""
        }
        // The platforms leave the layout when they've faded, while the button above (which fades a little behind them)
        // is still showing: without this the layout would spread it down the rail for those frames.
        Item {
            Layout.fillHeight: true
            visible: !scroller.visible
        }
        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: platformColumn.height
            Layout.minimumHeight: 0
            opacity: root.shown
            visible: opacity > 0
            enabled: !General.railHidden
            contentHeight: platformColumn.height
            clip: true
            // A press-and-drag on a button reorders it (see RailButton), so only the wheel scrolls.
            interactive: false
            boundsBehavior: Flickable.StopAtBounds

            onHeightChanged: contentY = Math.max(0, Math.min(contentHeight - height, contentY))

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 2
                    scroller.contentY = Math.max(0, Math.min(scroller.contentHeight - scroller.height, scroller.contentY - dy))
                }
            }

            Timer {
                interval: 16
                repeat: true
                running: root.dragging && !root.dragSettling && !root.dragOutside
                onTriggered: root.scrollDuringDrag()
            }

            KeyedListModel {
                id: platformModel
                leaveSpeed: "fast"
                source: Library.platforms.map(p => ({ key: p.id, name: p.name, icon: p.icon }))
            }
            Column {
                id: platformColumn
                width: scroller.width
                spacing: Theme.layout.spacing

                add: Transition {
                    enabled: Motion.on && root.ready && !platformModel.rebuilt
                    MotionNumber { property: "opacity"; from: 0; to: 1 }
                    MotionPop { property: "popScale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
                }
                move: Transition {
                    enabled: Motion.on && root.ready && !platformModel.rebuilt
                    MotionNumber { property: "y"; speed: "normal" }
                }

                Repeater {
                    id: platformRepeater
                    model: platformModel
                    onItemAdded: (index, item) => { if (index === 0) root.firstButton = item }
                    onItemRemoved: (index, item) => { if (root.firstButton === item) root.firstButton = null }

                    RailButton {
                        id: button
                        required property var model
                        required property int index
                        readonly property var modelData: ({ id: model.key, name: model.name, icon: model.icon })
                        readonly property bool dragged: root.dragging && root.dragId === modelData.id
                        x: (platformColumn.width - width) / 2
                        // Positions in `Library.platforms` and in the model agree only while nothing is leaving.
                        draggable: !platformModel.leaving
                        enabled: !model.leaving
                        opacity: (dragged && !root.dragSettling) || lifted ? Theme.rail.dragOpacity : 1
                        readonly property bool leaving: model.leaving
                        onLeavingChanged: {
                            if (!leaving)
                                return
                            if (Motion.on && root.ready)
                                leaveAnimation.start()
                            else
                                opacity = 0
                        }

                        PopLeave { id: leaveAnimation; target: button; scaleProperty: "popScale" }
                        following: dragged && !root.dragSettling
                        offsetY: !dragged ? root.shiftOf(index)
                                 : root.dragSettling ? (root.dragTo - root.dragFrom) * root.dragStep : root.dragDy
                        onDragStarted: (x, y) => root.startDrag(button, index, x, y)
                        onDragMoved: (x, y) => root.moveDrag(button, x, y)
                        onDragEnded: dropped => root.endDrag(dropped)
                        icon: Icons.forPlatform(modelData.icon)
                        tip: Actions.tip(modelData.name, Actions.keys["show_platform_" + (index + 1)] ?? [])
                        selected: Library.platformFilter === modelData.id
                        onClicked: Library.platformFilter = modelData.id
                        hasMenu: true
                        menuOpen: (platformMenu.item?.visible ?? false) && platformMenu.item.platform.id === modelData.id
                        onMenuRequested: root.openPlatformMenu(button, modelData)
                    }
                }
            }
        }

        Connections {
            target: Library
            function onPlatformFilterChanged() { root.showSelected() }
        }

        Column {
            id: bottomColumn
            Layout.alignment: Qt.AlignHCenter
            opacity: root.shown
            visible: root.bottomShown.length > 0 && opacity > 0
            enabled: !General.railHidden
            spacing: Theme.layout.spacing

            Repeater {
                id: bottomRepeater
                model: root.bottomShown

                RailButton {
                    id: bottomButton
                    required property string modelData
                    required property int index
                    readonly property var def: root.bottomDefs[modelData]
                    readonly property bool dragged: root.bottomDragging && root.bottomDragName === modelData
                    draggable: true
                    opacity: (dragged && !root.bottomSettling) || lifted ? Theme.rail.dragOpacity : 1
                    following: dragged && !root.bottomSettling
                    offsetY: !dragged ? root.bottomShiftOf(index)
                             : root.bottomSettling ? (root.bottomTo - root.bottomFrom) * root.bottomStep : root.bottomDy
                    onDragStarted: (x, y) => root.startBottomDrag(bottomButton, index, x, y)
                    onDragMoved: (x, y) => root.moveBottomDrag(bottomButton, x, y)
                    onDragEnded: dropped => root.endBottomDrag(dropped)
                    icon: def.icon
                    tip: def.tip
                    onClicked: def.run()
                }
            }
        }
    }
}
