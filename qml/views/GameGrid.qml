import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property var currentGame: ({})
    property string selectedId: ""
    readonly property bool allCollapsed: false

    signal launchRequested()
    signal message(string text)
    signal gameAction(string name, var game)
    signal gamesAction(string name, var ids)
    signal modManagerRequested(string id)
    signal stopRequested()
    signal emulatorMessage(string text, string gameId)
    signal editRequested()
    signal historyRequested()
    signal steamUndoRequested()
    signal customizeRequested()

    property var selection: ({})
    readonly property int selectionCount: Object.keys(selection).length
    property string anchorId

    property int coverWidth: General.gridCoverWidth
    readonly property real usable: grid.width - 2 * Theme.grid.margin + Theme.grid.gap - gridBar.reserve
    readonly property int columns: Math.max(1, Math.floor(usable / (coverWidth + Theme.grid.gap)))
    readonly property real cellW: usable / columns
    readonly property real coverW: cellW - Theme.grid.gap
    readonly property real coverH: coverW * 1.5
    readonly property real titleH: General.gridShowTitles ? Math.ceil(Theme.grid.titleSize * 1.4) + Theme.layout.spacingSmall : 0
    readonly property real cellH: coverH + titleH + Theme.grid.gap
    readonly property real pillHeight: Theme.iconButton.size + 2 * Theme.layout.spacingSmall
    property bool ready: false

    // The panel hides without changing the saved setting when there is no game to show.
    readonly property bool panelWanted: General.gridPanel && currentGame.id !== undefined
    property real slide: panelWanted ? 1 : 0
    // Hiding slides the panel out first and widens the grid after; showing narrows the grid first. The covers reflow in one step.
    property bool panelReserved: panelWanted
    onPanelWantedChanged: if (panelWanted) panelReserved = true
    onSlideChanged: if (slide === 0) panelReserved = false
    Behavior on slide { enabled: Motion.on && root.ready; MotionNumber { speed: "normal" } }

    Binding { target: Library; property: "flat"; value: true }

    function syncSelection() {
        let row = Library.rowOfGame(selectedId)
        if (row < 0)
            row = Library.nextGameRow(-1, 1)
        grid.currentIndex = row
        if (row < 0)
            currentGame = ({})
        else
            updateCurrent()
        if (selectionCount > 0)
            setSelection(Object.keys(selection).filter(id => Library.rowOfGame(id) >= 0))
    }

    function updateCurrent() {
        const id = Library.gameIdAt(grid.currentIndex)
        if (id === "")
            return
        selectedId = id
        currentGame = Library.gameAt(grid.currentIndex)
        Library.copyHeroesNear(grid.currentIndex)
        if (selectionCount === 0)
            anchorId = id
    }

    function selectedIds() {
        return selectionCount > 0 ? Object.keys(selection) : (selectedId !== "" ? [selectedId] : [])
    }

    function setSelection(ids) {
        if (ids.length < 2 || !ids.includes(selectedId)) {
            clearSelection()
            return
        }
        const set = {}
        for (const id of ids)
            set[id] = true
        selection = set
    }

    function clearSelection() {
        if (selectionCount > 0)
            selection = ({})
        anchorId = selectedId
    }

    function idsBetween(a, b) {
        const ids = []
        for (let r = Math.min(a, b); r <= Math.max(a, b); ++r) {
            const id = Library.gameIdAt(r)
            if (id !== "")
                ids.push(id)
        }
        return ids
    }

    function anchorRow() {
        const row = Library.rowOfGame(anchorId)
        return row >= 0 ? row : grid.currentIndex
    }

    function clickRow(row, modifiers) {
        const id = Library.gameIdAt(row)
        const ctrl = (modifiers & Qt.ControlModifier) !== 0
        const base = selectedIds()
        if ((modifiers & Qt.ShiftModifier) !== 0) {
            const from = anchorRow()
            const fromId = Library.gameIdAt(from)
            grid.currentIndex = row
            setSelection(idsBetween(from, row).concat(ctrl ? base : []))
            anchorId = fromId
        } else if (ctrl && base.includes(id)) {
            if (base.length === 1)
                return
            const rest = base.filter(other => other !== id)
            if (id === selectedId)
                grid.currentIndex = rest.map(other => Library.rowOfGame(other)).reduce((a, b) => Math.abs(b - row) < Math.abs(a - row) ? b : a)
            setSelection(rest)
            anchorId = id
        } else if (ctrl) {
            grid.currentIndex = row
            setSelection(base.concat([id]))
            anchorId = id
        } else {
            clearSelection()
            grid.currentIndex = row
            anchorId = id
        }
    }

    function extendTo(row) {
        if (row < 0 || row === grid.currentIndex)
            return
        const from = anchorRow()
        const fromId = Library.gameIdAt(from)
        grid.currentIndex = row
        setSelection(idsBetween(from, row))
        anchorId = fromId
    }

    function neighborOutside(ids) {
        for (const step of [1, -1]) {
            let row = Library.rowOfGame(selectedId)
            while ((row = Library.nextGameRow(row, step)) >= 0) {
                if (!ids.includes(Library.gameIdAt(row)))
                    return Library.gameIdAt(row)
            }
        }
        return ""
    }

    function neighborOf(id) {
        const row = Library.rowOfGame(id)
        let next = Library.gameIdAt(Library.nextGameRow(row, 1))
        if (next === "" || next === id)
            next = Library.gameIdAt(Library.nextGameRow(row, -1))
        return next === id ? "" : next
    }

    // Up and Down keep the column; a shorter last row takes the last cover.
    function rowBy(delta) {
        const count = grid.count
        const current = grid.currentIndex
        if (count === 0)
            return -1
        const target = current + delta
        if (target >= 0 && target < count)
            return target
        if (delta > 0 && Math.floor(current / columns) < Math.floor((count - 1) / columns))
            return count - 1
        return current
    }

    function pageSize() {
        return Math.max(1, Math.floor(grid.height / cellH)) * columns
    }

    function select(row) {
        if (row >= 0)
            grid.currentIndex = row
    }

    readonly property var navigationActions: ["previous_game", "next_game", "page_up", "page_down", "first_game", "last_game"]

    function run(name) {
        if (navigationActions.includes(name)) {
            focusList()
            clearSelection()
        }
        switch (name) {
        case "extend_previous": focusList(); extendTo(rowBy(-columns)); break
        case "extend_next": focusList(); extendTo(rowBy(columns)); break
        case "select_all":
            focusList()
            setSelection(idsBetween(0, grid.count - 1))
            break
        case "clear_selection": clearSelection(); break
        case "previous_game": select(rowBy(-columns)); break
        case "next_game": select(rowBy(columns)); break
        case "page_up": select(rowBy(-pageSize())); break
        case "page_down": select(rowBy(pageSize())); break
        case "first_game": select(Library.nextGameRow(-1, 1)); break
        case "last_game": select(Library.nextGameRow(grid.count, -1)); break
        case "focus_search": searchRow.focusSearch(); break
        case "open_filters": searchRow.openFilters(); break
        case "open_collections": searchRow.openCollections(); break
        case "toggle_favorites_filter": Library.favoritesOnly = !Library.favoritesOnly; break
        case "clear_filters":
            Library.clearFilters()
            if (General.railHidden)
                Library.platformFilter = ""
            break
        case "cycle_sort": {
            const orders = ["title", "last_played", "play_time"]
            Library.sort = orders[(orders.indexOf(Library.sort) + 1) % orders.length]
            message(sortNames[Library.sort])
            break
        }
        case "grid_bigger": resize(1); break
        case "grid_smaller": resize(-1); break
        }
    }

    readonly property var sortNames: ({
        title: qsTr("Sorted by title"),
        last_played: qsTr("Sorted by last played"),
        play_time: qsTr("Sorted by play time"),
    })

    function resize(direction) {
        const step = Math.max(1, Math.round((Theme.grid.maxCoverWidth - Theme.grid.minCoverWidth) / 12))
        setCoverWidth(coverWidth + direction * step)
        saveTimer.restart()
    }

    function setCoverWidth(width) {
        coverWidth = Math.max(Theme.grid.minCoverWidth, Math.min(Theme.grid.maxCoverWidth, Math.round(width)))
    }

    // The view keeps the current game on screen while the cells change size under it.
    onCoverWidthChanged: Qt.callLater(() => grid.positionViewAtIndex(grid.currentIndex, GridView.Contain))

    Timer {
        id: saveTimer
        interval: 400
        onTriggered: {
            const error = General.setGridCoverWidth(root.coverWidth)
            if (error !== "")
                root.message(error)
        }
    }

    function focusList() { grid.forceActiveFocus() }

    function selectGame(id) {
        selectedId = id
        syncSelection()
        grid.positionViewAtIndex(grid.currentIndex, GridView.Contain)
    }

    function revealGame(id) {
        if (Library.rowOfGame(id) < 0)
            Library.installedOnly = false
        if (Library.rowOfGame(id) < 0) {
            Library.clearFilters()
            Library.installedOnly = false
            Library.platformFilter = ""
        }
        if (Library.rowOfGame(id) < 0 && Library.collectionId !== "")
            Library.closeCollection()
        selectGame(id)
    }

    function selectGames(ids) {
        selectGame(ids[0])
        for (const id of ids.slice(1))
            clickRow(Library.rowOfGame(id), Qt.ControlModifier)
    }

    function openMenuSubmenu(which) {
        gameMenu.item.openSubmenu(which)
    }

    function scrollThrough(pixelsPerSecond) {
        const distance = grid.contentHeight - grid.height
        if (distance <= 0)
            return
        const animation = Qt.createQmlObject(
            "import QtQuick; NumberAnimation { property: \"contentY\"; easing.type: Easing.Linear }", grid)
        animation.target = grid
        animation.from = grid.originY
        animation.to = grid.originY + distance
        animation.duration = Math.round(distance / pixelsPerSecond * 1000)
        animation.start()
    }

    property int menuRow: -1

    function prepareMenu(row) {
        const game = Library.gameAt(row)
        if (game.id === undefined)
            return null
        gameMenu.active = true
        gameMenu.item.game = game
        gameMenu.item.games = selection[game.id] === true
            ? Object.keys(selection).map(id => Library.gameAt(Library.rowOfGame(id))) : []
        menuRow = row
        return gameMenu.item
    }

    function openMenu(row, x, y) {
        const menu = prepareMenu(row)
        if (menu === null)
            return
        gameMenu.button = null
        let below
        if (x === undefined) {
            grid.positionViewAtIndex(row, GridView.Contain)
            const item = grid.itemAtIndex(row)
            if (item === null)
                return
            gameMenu.under = item
            gameMenu.underTop = 0
            gameMenu.underBottom = root.coverH
            gameMenu.underRight = NaN
            below = root.coverH
            menu.popup(item, 8, below)
        } else {
            below = y
            menu.popup(grid, x, y)
        }
        // Near the window's bottom the menu flips above the point it was asked to open at, but only
        // once it has laid out its entries, after popup() returns.
        menu.popDirection = Qt.binding(() => menu.y < below ? "up" : "down")
    }

    // Right-aligned under the item, as a menu dropping from a button.
    function openMenuBelow(row, item) {
        // The press on the button closes the menu as a press outside it, before the click that would open it again.
        if (gameMenu.closedBy === item && Date.now() - gameMenu.closedAt < 1000) {
            gameMenu.closedBy = null
            return
        }
        if (gameMenu.item?.opened && gameMenu.button === item) {
            gameMenu.item.close()
            return
        }
        const menu = prepareMenu(row)
        if (menu === null)
            return
        gameMenu.button = item
        gameMenu.under = item.parent
        gameMenu.underTop = item.y - Theme.menu.buttonGap
        gameMenu.underBottom = item.y + item.height + Theme.menu.buttonGap
        gameMenu.underRight = item.x + item.width
        menu.popup(item.parent, item.x + item.width - menu.width, gameMenu.underBottom)
        menu.popDirection = Qt.binding(() => menu.y < gameMenu.underBottom ? "above" : "below")
    }

    Loader {
        id: gameMenu
        property Item under: null
        property real underTop: 0
        property real underBottom: 0
        property real underRight: NaN
        property Item button: null
        property Item closedBy: null
        property real closedAt: 0
        active: false
        source: "GameMenu.qml"
    }
    Connections {
        target: gameMenu.item
        function onChosen(name) {
            if (gameMenu.item.multi)
                root.gamesAction(name, gameMenu.item.games.map(g => g.id))
            else
                root.gameAction(name, gameMenu.item.game)
        }
        function onFailed(text) { root.message(text) }
        function onManagerChosen(id) { root.modManagerRequested(id) }
        function onNewTagRequested(ids) { root.askNewTag(ids) }
        function onNewCollectionRequested(ids) { searchRow.askCollection("new", "", "", ids) }
        function onOpened() { root.placeMenu() }
        function onHeightChanged() { root.placeMenu() }
        function onWidthChanged() { root.placeMenu() }
        function onAboutToHide() {
            gameMenu.closedBy = gameMenu.button?.hovered ? gameMenu.button : null
            gameMenu.closedAt = Date.now()
        }
        function onClosed() {
            root.menuRow = -1
            gameMenu.under = null
            gameMenu.button = null
        }
    }
    // Above the cover (or button) when there's no room below, so it doesn't cover it.
    function placeMenu() {
        const menu = gameMenu.item
        const under = gameMenu.under
        if (!menu.visible || under === null)
            return
        menu.y = under.mapToItem(null, 0, gameMenu.underBottom).y + menu.height > root.Window.height
            ? gameMenu.underTop - menu.height : gameMenu.underBottom
        if (!isNaN(gameMenu.underRight))
            menu.x = gameMenu.underRight - menu.width
    }

    function askNewTag(ids) {
        newTag.active = true
        newTag.item.ids = ids
        newTag.item.open()
    }
    Loader {
        id: newTag
        active: false
        source: "NewTagPrompt.qml"
    }
    Connections {
        target: newTag.item
        function onMessage(text) { root.message(text) }
        function onClosed() { root.focusList() }
    }

    Component.onCompleted: {
        syncSelection()
        grid.forceActiveFocus()
        Qt.callLater(() => { ready = true })
    }

    property real contentYBeforeReset: 0

    Connections {
        target: Library
        function onModelAboutToBeReset() { root.contentYBeforeReset = grid.contentY }
        function onModelReset() { Qt.callLater(root.syncSelection) }
        function onRowsInserted() { Qt.callLater(root.syncSelection) }
        function onRowsRemoved() { Qt.callLater(root.syncSelection) }
        function onRowsMoved() { Qt.callLater(root.syncSelection) }
        function onReloaded() {
            grid.contentY = Math.min(root.contentYBeforeReset,
                                     Math.max(grid.originY, grid.contentHeight - grid.height + grid.originY))
        }
        function onDataChanged(topLeft, bottomRight) {
            const row = Library.rowOfGame(root.selectedId)
            if (row >= 0 && row !== grid.currentIndex) {
                grid.currentIndex = row
                return
            }
            if (grid.currentIndex >= topLeft.row && grid.currentIndex <= bottomRight.row)
                root.updateCurrent()
        }
        function onSearchChanged() { root.clearSelection() }
        function onPlatformFilterChanged() { root.clearSelection() }
        function onFiltersChanged() { root.clearSelection() }
    }

    LibrarySearchRow {
        id: searchRow
        x: Theme.list.padding
        y: Theme.layout.margin
        width: Theme.list.width - 2 * Theme.list.padding
        ready: root.ready
        onMessage: text => root.message(text)
        onFocusRequested: root.focusList()
    }

    Label {
        anchors.centerIn: parent
        visible: grid.count === 0 && (Library.loading || Library.gameCount === 0)
        opacity: visible ? 1 : 0
        Behavior on opacity { enabled: Motion.fades && root.ready; MotionNumber { fade: true; speed: "normal" } }
        text: Library.loading ? qsTr("Loading library…") : qsTr("No games yet")
        color: Theme.palette.textMuted
    }

    GridView {
        id: grid

        x: General.railHidden ? -Theme.rail.width : 0
        y: searchRow.y + searchRow.height + Theme.layout.spacing
        width: parent.width - x - (root.panelReserved ? Theme.grid.panelWidth + Theme.grid.panelMargin + Theme.grid.margin : 0)
        height: parent.height - y - Theme.grid.panelMargin
        clip: true
        model: Library
        reuseItems: true
        keyNavigationEnabled: false
        highlightMoveDuration: 0
        boundsBehavior: Flickable.StopAtBounds
        cellWidth: root.cellW
        cellHeight: root.cellH
        leftMargin: Theme.grid.margin
        rightMargin: Theme.grid.margin - Theme.grid.gap + gridBar.reserve
        topMargin: Theme.grid.margin
        bottomMargin: root.pillHeight + 2 * Theme.grid.margin
        cacheBuffer: root.cellH

        ScrollBar.vertical: SettingsScrollBar {
            id: gridBar
            topPadding: Theme.grid.margin
            thickness: Theme.gameEditor.bulkScrollbarWidth
            trackless: true
            handleColor: Theme.list.scrollbarHandle
            handleHoverColor: Theme.list.scrollbarHandleHover
        }

        // Instant: keys repeat too often to animate a scroll.
        onCurrentIndexChanged: {
            root.updateCurrent()
            positionViewAtIndex(currentIndex, GridView.Contain)
        }

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: event => root.resize(event.angleDelta.y > 0 ? 1 : -1)
        }

        Keys.onPressed: event => {
            const shift = (event.modifiers & Qt.ShiftModifier) !== 0
            if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                const step = event.key === Qt.Key_Left ? -1 : 1
                const row = grid.currentIndex + step
                if (row >= 0 && row < grid.count) {
                    if (shift)
                        root.extendTo(row)
                    else {
                        root.clearSelection()
                        grid.currentIndex = row
                    }
                }
                event.accepted = true
            } else if (event.key === Qt.Key_Backspace) {
                if (searchRow.field.text !== "") {
                    searchRow.field.forceActiveFocus()
                    searchRow.field.remove(searchRow.field.length - 1, searchRow.field.length)
                }
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                if (root.selectionCount > 0)
                    root.clearSelection()
                else
                    searchRow.field.clear()
                event.accepted = true
            } else if (event.text.length === 1 && event.text > " " && event.text !== "\x7f"
                       && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
                searchRow.field.forceActiveFocus()
                searchRow.field.insert(searchRow.field.length, event.text)
                event.accepted = true
            }
        }

        delegate: Item {
            id: cell

            required property int index
            required property string gameId
            required property string title
            required property bool installed
            required property bool hidden
            required property bool updating
            required property bool favorite
            required property real hue
            required property url coverUrl

            readonly property bool running: GameRunner.running.includes(gameId)
            readonly property bool current: GridView.isCurrentItem
            readonly property bool selected: root.selection[gameId] === true
            readonly property bool hovered: mouse.containsMouse || root.menuRow === index

            width: GridView.view.cellWidth
            height: GridView.view.cellHeight

            Item {
                id: card
                width: root.coverW
                height: root.coverH + root.titleH
                scale: mouse.pressed ? Theme.motion.pressScale : 1
                Behavior on scale { enabled: Motion.on; MotionNumber {} }

                Item {
                    id: face
                    width: parent.width
                    height: root.coverH
                    opacity: cell.installed && !cell.hidden ? 1 : Theme.list.uninstalledOpacity

                    Item {
                        id: art
                        anchors.fill: parent
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskSource: artMask
                            maskThresholdMin: 0.5
                            maskSpreadAtMin: 1.0
                        }

                        ArtPlaceholder {
                            anchors.fill: parent
                            hue: cell.hue
                            visible: image.status !== Image.Ready

                            Label {
                                anchors.fill: parent
                                anchors.margins: Theme.layout.spacing
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                                text: cell.title
                                color: Theme.palette.text
                                font.pixelSize: Theme.grid.titleSize
                                font.weight: Theme.font.strongWeight
                            }
                        }

                        FadeImage {
                            id: image
                            anchors.fill: parent
                            source: cell.coverUrl
                            sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio / 32) * 32,
                                                Math.ceil(height * Screen.devicePixelRatio / 32) * 32)
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                            cache: true
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: Theme.grid.hoverOverlay
                            opacity: cell.hovered ? 1 : 0
                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                        }
                    }

                    Rectangle {
                        id: artMask
                        anchors.fill: parent
                        radius: Theme.grid.radius
                        visible: false
                        layer.enabled: true
                    }
                }

                Rectangle {
                    visible: cell.current || cell.selected
                    anchors.fill: face
                    anchors.margins: -(Theme.grid.ringGap + Theme.grid.ringWidth)
                    radius: Theme.grid.radius + Theme.grid.ringGap + Theme.grid.ringWidth
                    color: "transparent"
                    border.width: Theme.grid.ringWidth
                    border.color: Theme.grid.ringColor
                    opacity: cell.current ? 1 : Theme.grid.selectedOpacity
                }

                Row {
                    anchors.top: face.top
                    anchors.right: face.right
                    anchors.margins: Theme.layout.spacingSmall
                    spacing: Theme.layout.spacingSmall
                    layoutDirection: Qt.RightToLeft

                    Badge {
                        visible: cell.favorite
                        iconPath: Icons.star
                    }
                    Badge {
                        visible: cell.updating
                        iconPath: Icons.arrowDown
                    }
                    Badge {
                        visible: cell.running
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width / 3
                            height: width
                            radius: width / 2
                            color: Theme.list.runningColor
                        }
                    }
                }

                Label {
                    visible: General.gridShowTitles
                    anchors.top: face.bottom
                    anchors.topMargin: Theme.layout.spacingSmall
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: cell.title
                    color: Theme.list.text
                    font.pixelSize: Theme.grid.titleSize
                    opacity: face.opacity
                }
            }

            MouseArea {
                id: mouse
                x: card.x
                width: root.coverW
                height: root.coverH + root.titleH
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                ToolTip.visible: containsMouse && !General.gridShowTitles
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: cell.title

                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton) {
                        const p = mapToItem(grid, mouse.x, mouse.y)
                        root.openMenu(cell.index, p.x, p.y)
                        return
                    }
                    root.clickRow(cell.index, mouse.modifiers)
                    grid.forceActiveFocus()
                }
                onDoubleClicked: mouse => {
                    if (mouse.button === Qt.LeftButton && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)))
                        root.launchRequested()
                }
            }
        }
    }

    component Badge: Rectangle {
        property string iconPath
        default property alias content: slot.data

        width: Theme.grid.badgeSize
        height: width
        radius: width / 2
        color: Theme.grid.badgeBackground

        Icon {
            anchors.centerIn: parent
            visible: parent.iconPath !== ""
            path: parent.iconPath
            size: parent.width * 0.6
            color: Theme.grid.badgeIconColor
        }
        Item {
            id: slot
            anchors.fill: parent
        }
    }

    PopoverBackground {
        id: sizePill
        visible: root.selectionCount === 0
        anchors.horizontalCenter: grid.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.grid.margin
        width: pillRow.implicitWidth + 2 * Theme.layout.spacing
        height: root.pillHeight
        radius: Theme.radius.pill

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: Theme.layout.spacingSmall

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                plain: true
                icon: Icons.minimize
                tip: Actions.tip(qsTr("Smaller covers"), Actions.keys.grid_smaller)
                onClicked: { root.resize(-1); root.focusList() }
            }
            SettingsSlider {
                id: sizeSlider
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.grid.sliderWidth
                from: Theme.grid.minCoverWidth
                to: Theme.grid.maxCoverWidth
                stepSize: 1
                value: root.coverWidth
                onMoved: root.setCoverWidth(value)
                onPressedChanged: if (!pressed) saveTimer.restart()
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                plain: true
                icon: Icons.add
                tip: Actions.tip(qsTr("Bigger covers"), Actions.keys.grid_bigger)
                onClicked: { root.resize(1); root.focusList() }
            }
        }
    }

    Rectangle {
        visible: root.selectionCount > 0
        anchors.horizontalCenter: grid.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.grid.margin
        width: sizePill.width
        height: root.pillHeight
        radius: Theme.radius.pill
        color: Theme.popover.background
        border.color: Theme.popover.border
        border.width: Theme.popover.borderWidth

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 2 * Theme.layout.spacing
            anchors.rightMargin: 2 * Theme.layout.spacing
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("%1 selected").arg(root.selectionCount)
                color: Theme.list.selectionBarText
                elide: Text.ElideRight
            }
            Icon {
                Layout.rightMargin: Theme.layout.spacingSmall
                path: Icons.close
                size: Theme.list.sortValueFontSize + 2
                color: Theme.list.selectionBarText
                opacity: clearMouse.containsMouse ? 1 : 0.7

                MouseArea {
                    id: clearMouse
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.clearSelection()

                    ToolTip.visible: containsMouse
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: qsTr("Clear the selection")
                }
            }
        }
    }

    GridPanel {
        id: panel
        visible: root.slide > 0
        x: root.width - root.slide * (width + Theme.grid.panelMargin)
        y: grid.y + Theme.grid.margin
        height: root.height - y - Theme.grid.panelMargin
        game: root.currentGame
        onLaunchRequested: root.launchRequested()
        onStopRequested: root.stopRequested()
        onMessage: text => root.message(text)
        onEmulatorMessage: (text, gameId) => root.emulatorMessage(text, gameId)
        onEditRequested: root.editRequested()
        onMenuRequested: button => root.openMenuBelow(Library.rowOfGame(root.currentGame.id), button)
        onHistoryRequested: root.historyRequested()
        onSteamUndoRequested: root.steamUndoRequested()
        onCustomizeRequested: root.customizeRequested()
    }

    function openInstallDialog() {
        panel.openInstallDialog()
    }

    function openPanelMenu() {
        panel.openMenu()
    }

    function pageMenuForScreenshot(place) {
        panel.pageMenuForScreenshot(place)
    }
}
