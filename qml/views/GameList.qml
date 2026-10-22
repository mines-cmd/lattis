import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    property var currentGame: ({})
    property string selectedId: ""
    property real reach: 0
    property real extraTop: 0
    Behavior on reach { enabled: Motion.on && root.ready; MotionNumber { speed: "normal" } }
    Behavior on extraTop { enabled: Motion.on && root.ready; MotionNumber { speed: "normal" } }

    signal launchRequested()
    signal message(string text)
    signal gameAction(string name, var game)
    signal gamesAction(string name, var ids)

    property var selection: ({})
    readonly property bool allCollapsed: list.count > 0 && currentGame.id === undefined
    readonly property int selectionCount: Object.keys(selection).length
    property string anchorId

    function syncSelection() {
        let row = Library.rowOfGame(selectedId)
        if (row < 0)
            row = Library.nextGameRow(-1, 1)
        list.currentIndex = row
        if (row < 0)
            currentGame = ({})
        else
            updateCurrent()
        if (selectionCount > 0)
            setSelection(Object.keys(selection).filter(id => Library.rowOfGame(id) >= 0))
    }

    // While previous_game / next_game repeat from a held key, currentGame (so the details) stays
    // put; it catches up on the key's release.
    property bool stepKeyDown: false
    property bool detailsHeld: false

    function stepGame(direction) {
        detailsHeld = stepKeyDown
        stepKeyDown = true
        const row = Library.nextGameRow(list.currentIndex, direction)
        scrollGlide.stop()
        // A held key repeats too fast for a glide to keep up: it jumps.
        if (detailsHeld || !ready || !Motion.on || row === list.currentIndex) {
            select(row)
            return
        }
        const from = list.contentY
        select(row)
        // The list follows the current row at the next layout; asking now gives where it will end.
        list.positionViewAtIndex(row, ListView.Contain)
        if (list.contentY !== from) {
            scrollGlide.from = from
            scrollGlide.to = list.contentY
            scrollGlide.start()
        }
    }

    function releaseStepKey() {
        stepKeyDown = false
        if (detailsHeld) {
            detailsHeld = false
            updateCurrent()
        }
    }

    function updateCurrent() {
        const id = Library.gameIdAt(list.currentIndex)
        if (id === "")
            return
        selectedId = id
        if (detailsHeld) {
            anchorId = id
            return
        }
        currentGame = Library.gameAt(list.currentIndex)
        Library.copyHeroesNear(list.currentIndex)
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
        return row >= 0 ? row : list.currentIndex
    }

    function clickRow(row, modifiers) {
        const id = Library.gameIdAt(row)
        const ctrl = (modifiers & Qt.ControlModifier) !== 0
        const base = selectedIds()
        if ((modifiers & Qt.ShiftModifier) !== 0) {
            const from = anchorRow()
            list.currentIndex = row
            setSelection(idsBetween(from, row).concat(ctrl ? base : []))
        } else if (ctrl && base.includes(id)) {
            if (base.length === 1)
                return
            const rest = base.filter(other => other !== id)
            if (id === selectedId)
                list.currentIndex = rest.map(other => Library.rowOfGame(other)).reduce((a, b) => Math.abs(b - row) < Math.abs(a - row) ? b : a)
            setSelection(rest)
            anchorId = id
        } else if (ctrl) {
            list.currentIndex = row
            setSelection(base.concat([id]))
            anchorId = id
        } else {
            clearSelection()
            list.currentIndex = row
            anchorId = id
        }
    }

    function extend(step) {
        const to = Library.nextGameRow(list.currentIndex, step)
        if (to < 0)
            return
        const from = anchorRow()
        list.currentIndex = to
        setSelection(idsBetween(from, to))
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

    readonly property var navigationActions: ["previous_game", "next_game", "page_up", "page_down", "first_game",
                                              "last_game", "previous_group", "next_group", "collapse_group", "expand_group"]

    function run(name) {
        if (navigationActions.includes(name)) {
            focusList()
            clearSelection()
        }
        switch (name) {
        case "extend_previous": focusList(); extend(-1); break
        case "extend_next": focusList(); extend(1); break
        case "select_all":
            focusList()
            setSelection(idsBetween(0, list.count - 1))
            break
        case "clear_selection": clearSelection(); break
        case "previous_game": stepGame(-1); break
        case "next_game": stepGame(1); break
        case "page_up": select(stepBy(-pageSize())); break
        case "page_down": select(stepBy(pageSize())); break
        case "first_game": select(Library.nextGameRow(-1, 1)); break
        case "last_game": select(Library.nextGameRow(list.count, -1)); break
        case "previous_group": select(Library.nextGroupRow(list.currentIndex, -1)); break
        case "next_group": select(Library.nextGroupRow(list.currentIndex, 1)); break
        case "collapse_group": select(Library.collapseGroupAt(list.currentIndex)); break
        case "expand_group": select(Library.expandGroupNear(list.currentIndex)); break
        case "focus_search":
            search.forceActiveFocus()
            search.selectAll()
            break
        case "open_filters": openFilters(); break
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
        }
    }

    readonly property var sortNames: ({
        title: qsTr("Sorted by title"),
        last_played: qsTr("Sorted by last played"),
        play_time: qsTr("Sorted by play time"),
    })

    function focusList() { list.forceActiveFocus() }

    function selectGame(id) {
        selectedId = id
        syncSelection()
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
        const distance = list.contentHeight - list.height
        if (distance <= 0)
            return
        const animation = Qt.createQmlObject(
            "import QtQuick; NumberAnimation { property: \"contentY\"; easing.type: Easing.Linear }", list)
        animation.target = list
        animation.from = list.originY
        animation.to = list.originY + distance
        animation.duration = Math.round(distance / pixelsPerSecond * 1000)
        animation.start()
    }

    function select(row) {
        if (row >= 0)
            list.currentIndex = row
    }

    function neighborOf(id) {
        const row = Library.rowOfGame(id)
        let next = Library.gameIdAt(Library.nextGameRow(row, 1))
        if (next === "" || next === id)
            next = Library.gameIdAt(Library.nextGameRow(row, -1))
        return next === id ? "" : next
    }

    property int menuRow: -1
    property Item hoveredRow: null

    function openMenu(row, x, y) {
        const game = Library.gameAt(row)
        if (game.id === undefined)
            return
        gameMenu.active = true
        gameMenu.item.game = game
        gameMenu.item.games = selection[game.id] === true
            ? Object.keys(selection).map(id => Library.gameAt(Library.rowOfGame(id))) : []
        menuRow = row
        const menu = gameMenu.item
        let below
        if (x === undefined) {
            list.positionViewAtIndex(row, ListView.Contain)
            const item = list.itemAtIndex(row)
            if (item === null)
                return
            gameMenu.underRow = item
            below = item.height
            menu.popup(item, 8, below)
        } else {
            below = y
            menu.popup(list, x, y)
        }
        // Near the window's bottom the menu flips above the point it was asked to open at, but only
        // once it has laid out its entries, after popup() returns.
        menu.popDirection = Qt.binding(() => menu.y < below ? "up" : "down")
    }

    Loader {
        id: gameMenu
        property Item underRow: null
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
        function onNewTagRequested(ids) { root.askNewTag(ids) }
        function onOpened() { root.placeMenu() }
        function onHeightChanged() { root.placeMenu() }
        function onClosed() {
            root.menuRow = -1
            gameMenu.underRow = null
        }
    }
    // Above the row when there's no room below, so it doesn't cover it. The menu's height is
    // only known once it's shown.
    function placeMenu() {
        const menu = gameMenu.item
        const row = gameMenu.underRow
        if (menu.visible && row !== null)
            menu.y = row.mapToItem(null, 0, row.height).y + menu.height > root.Window.height ? -menu.height : row.height
    }

    function pageSize() {
        return Math.max(1, Math.floor(list.height / Theme.list.rowHeight) - 1)
    }

    function stepBy(count) {
        let row = list.currentIndex
        for (let i = 0; i < Math.abs(count); ++i)
            row = Library.nextGameRow(row, count < 0 ? -1 : 1)
        return row
    }

    function openFilters() {
        filterPanel.active = true
        if (filterPanel.item.visible) {
            filterPanel.item.close()
            focusList()
        } else
            filterPanel.item.open()
    }

    readonly property var activeFilters: {
        const out = []
        const platform = General.railHidden && Library.platformFilter !== ""
            ? Library.gameCounts.find(p => p.id === Library.platformFilter) : undefined
        if (platform !== undefined)
            out.push({ key: "platform", text: platform.name, off: () => Library.platformFilter = "" })
        if (!Library.installedOnly)
            out.push({ key: "installed", text: qsTr("Not installed shown"), off: () => Library.installedOnly = true })
        if (Library.favoritesOnly)
            out.push({ key: "favorites", text: qsTr("Favorites"), off: () => Library.favoritesOnly = false })
        if (Library.showHidden)
            out.push({ key: "hidden", text: qsTr("Hidden shown"), off: () => Library.showHidden = false })
        for (const status of Library.statuses) {
            if (Library.statusFilter.includes(status.id))
                out.push({ key: "status:" + status.id, text: status.name,
                           off: () => Library.statusFilter = Library.statusFilter.filter(s => s !== status.id) })
        }
        const palette = Theme.tags.palette
        let tags = 0
        for (const name of Library.tagFilter) {
            const tag = Library.tags.find(t => t.name === name)
            if (tag === undefined)
                continue
            ++tags
            out.push({
                key: "tag:" + name,
                text: name,
                tagColor: palette.length > 0 ? palette[tag.color % palette.length] : Theme.filters.chipBackground,
                off: () => Library.tagFilter = Library.tagFilter.filter(t => t !== name)
            })
        }
        if (Library.tagMatchAll && tags >= 2)
            out.push({ key: "all-tags", text: qsTr("All tags"), off: () => Library.tagMatchAll = false })
        return out
    }

    // Without the functions, which a model can't hold: a chip finds its `off` by key.
    KeyedListModel {
        id: chipModel
        leaveSpeed: "fast"
        source: root.activeFilters.map(f => ({ key: f.key, text: f.text, tagColor: String(f.tagColor ?? "transparent") }))
    }

    property bool ready: false

    color: Theme.list.background

    Rectangle {
        x: -width
        width: Math.max(0, root.reach - Math.max(0, Theme.list.padding - Theme.list.railMargin))
        height: parent.height
        visible: width > 0
        color: Theme.list.background
    }

    // Outside the ListView, whose declared children scroll with its content.
    Rectangle {
        id: fadeMask
        visible: false
        layer.enabled: true
        width: list.width
        height: list.height
        gradient: Gradient {
            GradientStop { position: 0; color: list.fadeTop > 0 ? "transparent" : "white" }
            GradientStop { position: list.height > 0 ? list.fadeTop / list.height : 0; color: "white" }
            GradientStop { position: list.height > 0 ? 1 - list.fadeBottom / list.height : 1; color: "white" }
            GradientStop { position: 1; color: list.fadeBottom > 0 ? "transparent" : "white" }
        }
    }

    Component.onCompleted: {
        syncSelection()
        list.forceActiveFocus()
        Qt.callLater(() => { ready = true })
    }

    // Scroll position before a model reset, restored when the reset was a reload from disk
    // (a hand-edited or new game file) rather than a search or filter change.
    property real contentYBeforeReset: 0

    Connections {
        target: Library
        function onModelAboutToBeReset() { root.contentYBeforeReset = list.contentY }
        function onModelReset() { Qt.callLater(root.syncSelection) }
        function onRowsInserted() { Qt.callLater(root.syncSelection) }
        function onRowsRemoved() { Qt.callLater(root.syncSelection) }
        function onRowsMoved() { Qt.callLater(root.syncSelection) }
        function onReloaded() {
            list.contentY = Math.min(root.contentYBeforeReset,
                                     Math.max(list.originY, list.contentHeight - list.height + list.originY))
        }
        // Rows updated in place (the game list itself didn't change): refresh the details.
        // A game that moved (played, so it sorts first) keeps the selection: the view's index can still be
        // the old row, which now holds another game.
        function onDataChanged(topLeft, bottomRight) {
            const row = Library.rowOfGame(root.selectedId)
            if (row >= 0 && row !== list.currentIndex) {
                list.currentIndex = row
                return
            }
            if (list.currentIndex >= topLeft.row && list.currentIndex <= bottomRight.row)
                root.updateCurrent()
        }
        function onSearchChanged() { root.clearSelection() }
        function onPlatformFilterChanged() { root.clearSelection() }
        function onFiltersChanged() { root.clearSelection() }
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

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.list.padding
        anchors.rightMargin: Theme.list.padding
        anchors.topMargin: Theme.layout.margin
        spacing: Theme.layout.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            TextField {
                id: search
                Layout.fillWidth: true
                implicitHeight: Theme.search.height
                leftPadding: Theme.search.iconSize + 19
                placeholderText: Theme.search.placeholder
                placeholderTextColor: Theme.search.placeholderColor
                color: Theme.search.text
                selectionColor: Theme.palette.accent
                selectedTextColor: Theme.palette.onAccent
                background: Surface {
                    radius: Theme.search.radius
                    fill: Theme.search.background
                    fillEnd: Theme.search.backgroundEnd
                    direction: Theme.search.gradientDirection
                    border.color: search.activeFocus ? Theme.search.borderFocus : Theme.search.border
                    border.width: search.activeFocus ? Theme.search.borderWidthFocus : Theme.search.borderWidth

                    Behavior on border.color { enabled: Motion.fades; MotionColor {} }

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Theme.search.hoverTint
                        opacity: search.hovered ? 1 : 0
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                    }
                }

                onTextChanged: Library.search = text
                Keys.onEscapePressed: list.forceActiveFocus()
                HoverHandler { id: searchPointer }
                ContextMenu.menu: TextContextMenu { field: search; pointer: searchPointer }

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 11
                    size: Theme.search.iconSize
                    path: Icons.search
                    color: Theme.search.icon
                }
            }

            // The panel's parent: the button dips when pressed, and a popup follows its parent's
            // scale, so the panel would shift as the button springs back.
            Item {
                id: filterHost
                visible: Theme.search.showFilterButton
                implicitWidth: Theme.filterButton.size
                implicitHeight: Theme.filterButton.size

                IconButton {
                    anchors.fill: parent
                    icon: Icons.filter
                    iconSize: Theme.filterButton.iconSize
                    iconColor: Theme.filterButton.icon
                    radius: Theme.filterButton.radius
                    fill: Theme.filterButton.background
                    fillEnd: Theme.filterButton.backgroundEnd
                    hoverTint: Theme.filterButton.hoverTint
                    direction: Theme.filterButton.gradientDirection
                    border.color: Theme.filterButton.border
                    border.width: Theme.filterButton.borderWidth
                    badgeColor: Theme.filterButton.badge
                    badge: Library.filtersActive || (General.railHidden && Library.platformFilter !== "")
                    tip: Actions.tip(root.activeFilters.length > 0
                                     ? qsTr("Filters: %1").arg(root.activeFilters.map(f => f.text).join(", "))
                                     : qsTr("Filters"),
                                     Actions.keys.open_filters)
                    onClicked: root.openFilters()
                }

                Loader {
                    id: filterPanel
                    active: false
                    source: "FilterPanel.qml"
                    onLoaded: item.parent = filterHost
                }
                Binding {
                    target: filterPanel.item
                    property: "x"
                    value: filterHost.width + Theme.layout.spacing
                }
                Connections {
                    target: filterPanel.item
                    function onClosedByKey() { root.focusList() }
                }
            }
        }

        // The filters that are on; × turns one off, the chip opens the panel.
        // The row's space opens at once, as the first chip starts popping in. It closes in a second
        // step: the last chip pops out first, then the list eases up.
        Item {
            id: chipRow
            readonly property real wanted: Theme.search.showFilterChips && (root.activeFilters.length > 0 || chipModel.count > 0) ? 1 : 0
            property real space: wanted
            property real flowHeight: 0
            Binding on flowHeight {
                when: chipFlow.implicitHeight > 0
                value: chipFlow.implicitHeight
                restoreMode: Binding.RestoreNone
            }

            onWantedChanged: {
                closeAnimation.stop()
                if (wanted === 1 || !Motion.on || !root.ready)
                    space = wanted
                else
                    closeAnimation.start()
            }
            MotionNumber {
                id: closeAnimation
                target: chipRow
                property: "space"
                to: 0
                speed: "normal"
            }

            Layout.fillWidth: true
            // Always in the layout, so nothing shifts when it empties. A cell can't be smaller than
            // nothing, so the extra gap it adds when empty is taken back by the list's top margin.
            Layout.preferredHeight: (flowHeight + Theme.layout.spacing) * space

        Flow {
            id: chipFlow
            width: parent.width
            spacing: Theme.filters.chipSpacing

            add: Transition {
                enabled: Motion.on && root.ready && !chipModel.rebuilt
                MotionNumber { property: "opacity"; from: 0; to: 1 }
                MotionPop { property: "scale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
            }
            move: Transition {
                enabled: Motion.on && root.ready && !chipModel.rebuilt
                MotionNumber { properties: "x,y"; speed: "normal" }
            }

            Repeater {
                model: chipModel

                FilterChip {
                    id: chip
                    required property var model
                    readonly property bool leaving: model.leaving
                    text: model.text
                    tagColor: model.tagColor
                    enabled: !leaving
                    onClicked: root.openFilters()
                    onRemoved: root.activeFilters.find(f => f.key === model.key)?.off()

                    PopLeave { target: chip; leaving: chip.leaving; animated: Motion.on && root.ready }
                }
            }
        }
        }

        Label {
            visible: list.count === 0 && (Library.loading || Library.gameCount === 0)
            opacity: visible ? 1 : 0
            Behavior on opacity { enabled: Motion.fades && root.ready; MotionNumber { fade: true; speed: "normal" } }
            Layout.fillWidth: true
            Layout.leftMargin: -root.reach
            Layout.topMargin: Theme.layout.spacing + root.extraTop
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Library.loading ? qsTr("Loading library…") : qsTr("No games yet")
            color: Theme.palette.textMuted
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.leftMargin: -root.reach
            // Less the gap the chip row's cell adds even when empty.
            Layout.topMargin: root.extraTop - Theme.layout.spacing
            Layout.fillHeight: true
            clip: true
            model: Library
            reuseItems: true
            keyNavigationEnabled: false
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {
                id: scrollBar
                thickness: Theme.gameEditor.bulkScrollbarWidth
                trackless: true
                handleColor: Theme.list.scrollbarHandle
                handleHoverColor: Theme.list.scrollbarHandleHover
            }
            readonly property int rowWidth: width - scrollBar.thickness - Theme.layout.spacing / 2

            // Rows fade out over the last `edge_fade` pixels at an edge with more to scroll to, instead of being cut.
            // The fade grows from nothing as the list leaves its end, and the layer exists only while one is showing.
            readonly property real fadeTop: Math.min(Theme.list.edgeFade, Math.max(0, contentY - originY))
            readonly property real fadeBottom: Math.min(Theme.list.edgeFade, Math.max(0, contentHeight + originY - contentY - height))
            layer.enabled: fadeTop > 0 || fadeBottom > 0
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: fadeMask
                // Without a threshold and spread the mask cuts at any alpha above 0 instead of following it.
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1
            }

            onCurrentIndexChanged: root.updateCurrent()

            MotionNumber {
                id: scrollGlide
                target: list
                property: "contentY"
            }

            // A Shortcut takes only the press: the release still reaches the list (run() focuses
            // it), and repeats come as auto-repeat release and press pairs.
            Keys.onReleased: event => {
                if (!event.isAutoRepeat && root.stepKeyDown)
                    root.releaseStepKey()
            }
            onActiveFocusChanged: if (!activeFocus && root.stepKeyDown) root.releaseStepKey()

            // A game added or removed (not a search, filter or sort, which replace the whole
            // list): it fades and slides, and the rows after it move over.
            // A group collapsing or expanding folds instead: everything after its header moves as
            // one block, the group's rows sliding under the header or out from under it with the
            // rows below attached. The list's transitions stand down for it; each row follows
            // `foldShift`, including rows created while it runs, which a transition would miss.
            // The block moves at most the height left below the header, so a group of hundreds
            // folds as quickly as one of three.
            property bool folding: false
            property int foldStart: -1
            property real foldTop: 0
            property real foldDistance: 0
            property bool foldCollapse: false
            property real foldProgress: 1
            readonly property real foldShift: (foldCollapse ? 1 : -1) * (1 - foldProgress) * foldDistance
            signal foldEnded()

            NumberAnimation {
                id: foldAnimation
                target: list
                property: "foldProgress"
                from: 0
                to: 1
                // `fold_duration` is for a block as tall as the list; a shorter one takes less, or a
                // group of one game crawls. The square root and the `normal` floor keep short folds
                // from being a blink.
                duration: Motion.duration(Math.max(Math.min(Motion.speedMs("normal"), Theme.list.foldDuration),
                                                   Theme.list.foldDuration * Math.sqrt(Math.min(list.foldDistance / Math.max(list.height, 1), 1))))
                easing.type: Motion.easingType
                easing.bezierCurve: Motion.easingCurve
                onStopped: {
                    list.foldEnded()
                    list.folding = false
                }
            }

            Connections {
                target: Library
                function onGroupChangeChanged() {
                    const rows = Library.groupChange.rows ?? 0
                    if (rows === 0)
                        return
                    foldAnimation.stop()
                    if (!Motion.on || Theme.list.foldDuration <= 0)
                        return
                    const start = Library.groupChange.start
                    const header = list.itemAtIndex(start - 1)
                    const viewBottom = list.contentY + list.height
                    const top = Math.max(header ? header.y + header.height : list.contentY, list.contentY)
                    // Whole rows, so the one cut off at the bottom doesn't overlap the rows below.
                    const room = Math.ceil(Math.max(viewBottom - top, 0) / Theme.list.rowHeight) * Theme.list.rowHeight
                    const distance = Math.min(Math.abs(rows) * Theme.list.rowHeight, room)
                    if (distance <= 0)
                        return
                    list.foldStart = start
                    list.foldTop = top
                    list.foldDistance = distance
                    list.foldCollapse = rows < 0
                    list.foldProgress = 0
                    list.folding = true
                    foldAnimation.start()
                }
            }

            add: Transition {
                enabled: Motion.on && !list.folding
                ParallelAnimation {
                    MotionNumber { property: "opacity"; from: 0; to: 1; speed: "normal" }
                    MotionNumber { property: "x"; from: -Theme.list.changeSlide; to: 0; speed: "normal" }
                }
            }
            // A collapsing group's rows are held by a transition, not ListView.delayRemove: the
            // list keeps rows after a delayed row below it, which opened a gap the height of the group.
            remove: list.folding ? foldRemove : changeRemove
            Transition {
                id: foldRemove
                PauseAnimation { duration: foldAnimation.duration }
            }
            Transition {
                id: changeRemove
                enabled: Motion.on
                ParallelAnimation {
                    MotionNumber { property: "opacity"; to: 0; speed: "normal" }
                    MotionNumber { property: "x"; to: -Theme.list.changeSlide; speed: "normal" }
                }
            }
            displaced: Transition {
                enabled: Motion.on && !list.folding
                MotionNumber { property: "y"; speed: "normal" }
            }
            // A fade, not a slide: across a long list a slide reads as a glitch.
            move: Transition {
                enabled: Motion.on
                SequentialAnimation {
                    MotionNumber { property: "opacity"; to: 0 }
                    // Without a `to`, the view fills in the destination; a PropertyAction with
                    // ViewTransition.destination.y leaves the row at y 0.
                    NumberAnimation { property: "y"; duration: 0 }
                    MotionNumber { property: "opacity"; to: 1 }
                }
            }

            // One hover highlight for the whole list, so it can spring from row to row. A child of
            // the content, so it scrolls with the rows; delegates sit above it (z 1). The parent is
            // set: unlike a Flickable, a ListView leaves the items declared in it on the view itself.
            Rectangle {
                id: hoverHighlight
                parent: list.contentItem

                readonly property Item target: root.menuRow >= 0 ? list.itemAtIndex(root.menuRow) : root.hoveredRow
                property Item last: null
                onTargetChanged: if (target) last = target

                x: 0
                y: last ? last.y + 1 : 0
                width: list.rowWidth
                height: Theme.list.rowHeight - 2
                radius: Theme.list.rowRadius
                color: Theme.list.rowHover
                // Selected rows have their own color; overlays add up, so it hides under them.
                opacity: target && !target.current && !target.selected && !list.folding ? 1 : 0

                // Only once it shows: coming back after leaving the list, it appears in place.
                Behavior on y {
                    enabled: Motion.on && hoverHighlight.opacity > 0
                    MotionSpring { spring: Theme.list.hoverSpring; damping: Theme.list.hoverDamping }
                }
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            }

            footer: FilteredCount {
                width: ListView.view.rowWidth
                shown: Theme.list.filteredCountPosition === "end"
                listed: list.count > 0
            }

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Backspace) {
                    if (search.text !== "") {
                        search.forceActiveFocus()
                        search.remove(search.length - 1, search.length)
                    }
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    // The selection goes first: the search is one more Esc away.
                    if (root.selectionCount > 0)
                        root.clearSelection()
                    else
                        search.clear()
                    event.accepted = true
                } else if (event.text.length === 1 && event.text > " " && event.text !== "\x7f"
                           && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
                    // Printable text starts or continues a search. (Backspace and Escape carry
                    // control characters as text, hence the checks above and the "> space".)
                    search.forceActiveFocus()
                    search.insert(search.length, event.text)
                    event.accepted = true
                }
            }

            delegate: Item {
                id: row

                required property int index
                required property bool isHeader
                required property string gameId
                required property string title
                required property string group
                required property int count
                required property bool collapsed
                required property bool installed
                required property bool hidden
                required property bool updating
                required property real hue
                required property url iconUrl
                required property string sortValue

                // Off from when a row is pooled until its new data is in, so a reused row doesn't
                // fade from the previous game's color.
                property bool fading: false
                Component.onCompleted: Qt.callLater(() => { fading = true })
                // A removed row is pooled, not destroyed, after its remove transition: still showing
                // at its old place, and faded out and slid aside by that transition.
                ListView.onPooled: {
                    fading = false
                    visible = false
                    opacity = 1
                    x = 0
                    foldingOut = false
                    if (root.hoveredRow === row)
                        root.hoveredRow = null
                }
                ListView.onReused: {
                    visible = true
                    Qt.callLater(() => { fading = true })
                }

                property bool foldingOut: false
                readonly property bool folded: list.folding && (foldingOut || index >= list.foldStart)
                readonly property real foldY: !folded ? 0
                                              : foldingOut ? list.foldShift - list.foldDistance : list.foldShift
                transform: Translate { y: row.foldY }
                // Gone by the time its middle, where its text is, reaches the header's edge: the
                // header has no background to hide it behind, but the space under its title is
                // empty. A leaving row past the moving block (it was
                // off screen) never shows, or it would cover the rows below.
                Binding on opacity {
                    when: row.folded
                    value: row.foldingOut && row.y >= list.foldTop + list.foldDistance ? 0
                           : Math.max(0, Math.min(1, (row.y + row.foldY + row.height / 2 - list.foldTop) / (row.height / 2)))
                    restoreMode: Binding.RestoreBindingOrValue
                }
                ListView.onRemove: if (list.folding) foldingOut = true
                // Back at its old place until the list pools it.
                Connections {
                    target: list
                    enabled: row.foldingOut
                    function onFoldEnded() {
                        row.foldingOut = false
                        row.visible = false
                    }
                }

                readonly property bool current: ListView.isCurrentItem
                readonly property bool selected: !isHeader && root.selection[gameId] === true
                readonly property int headerGap: isHeader && index > 0 ? Theme.list.groupGap : 0

                width: ListView.view.rowWidth
                height: Theme.list.rowHeight + headerGap

                property color headerColor: isHeader && mouse.containsMouse ? Theme.list.headerHoverText
                                                                             : Theme.list.headerText
                Behavior on headerColor { enabled: Motion.fades && row.fading; MotionColor {} }

                RowLayout {
                    visible: row.isHeader
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 8
                    anchors.topMargin: row.headerGap
                    spacing: 6

                    Icon {
                        path: Icons.chevron
                        size: Theme.list.headerFontSize + 3
                        color: row.headerColor
                        rotation: row.collapsed ? 0 : 90

                        // Not for a reused row taking another group's header.
                        Behavior on rotation { enabled: Motion.on && row.fading; MotionNumber {} }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: Theme.list.headerUppercase ? row.title.toUpperCase() : row.title
                        color: row.headerColor
                        font.weight: Theme.font.strongWeight
                        font.pixelSize: Theme.list.headerFontSize
                        font.letterSpacing: Theme.list.headerUppercase ? 0.8 : 0
                    }
                    Label {
                        visible: Theme.list.showCounts
                        text: row.count
                        color: row.headerColor
                        font.pixelSize: Theme.list.headerFontSize
                    }
                }

                Rectangle {
                    visible: !row.isHeader
                    anchors.fill: parent
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    radius: Theme.list.rowRadius
                    color: row.current || row.selected ? Theme.list.rowSelected
                         : Qt.alpha(Theme.list.rowSelected, 0)

                    Behavior on color { enabled: Motion.fades && row.fading; MotionColor {} }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10
                        opacity: row.installed && !row.hidden ? 1 : Theme.list.uninstalledOpacity

                        GameIcon {
                            visible: Theme.list.showIcons
                            source: row.iconUrl
                            title: row.title
                            hue: row.hue
                        }
                        Label {
                            id: titleLabel
                            Layout.fillWidth: true
                            text: row.title
                            elide: Theme.list.titleElide === "middle" ? Text.ElideMiddle
                                 : Theme.list.titleElide === "left" ? Text.ElideLeft : Text.ElideRight
                            color: Theme.list.showUpdateIcon && row.updating ? Theme.list.updateIconColor : Theme.list.text
                            font.bold: row.current && Theme.list.selectedBold

                            ToolTip.visible: truncated && mouse.containsMouse
                            ToolTip.delay: Theme.tooltip.delay
                            ToolTip.text: row.title
                        }
                        Icon {
                            visible: Theme.list.showUpdateIcon && row.updating
                            path: Icons.arrowDown
                            size: Theme.list.sortValueFontSize + 2
                            color: Theme.list.updateIconColor
                        }
                        Icon {
                            visible: row.hidden
                            path: Icons.hidden
                            size: Theme.list.sortValueFontSize + 2
                            color: Theme.list.sortValueColor
                        }
                        Label {
                            visible: Theme.list.showSortValue && Library.showSortValue && row.sortValue !== ""
                            text: row.sortValue
                            color: Theme.list.sortValueColor
                            font.pixelSize: Theme.list.sortValueFontSize
                        }
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    // Folding rows pass under the header, which would lose the hover to them.
                    enabled: !row.folded
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onContainsMouseChanged: {
                        if (containsMouse && !row.isHeader)
                            root.hoveredRow = row
                        else if (root.hoveredRow === row)
                            root.hoveredRow = null
                    }
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton) {
                            if (!row.isHeader) {
                                const p = mapToItem(list, mouse.x, mouse.y)
                                root.openMenu(row.index, p.x, p.y)
                            }
                            return
                        }
                        if (row.isHeader)
                            Library.toggleGroup(row.group)
                        else
                            root.clickRow(row.index, mouse.modifiers)
                        list.forceActiveFocus()
                    }
                    onDoubleClicked: mouse => {
                        if (!row.isHeader && mouse.button === Qt.LeftButton
                            && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)))
                            root.launchRequested()
                    }
                }
            }
        }

        FilteredCount {
            Layout.fillWidth: true
            Layout.leftMargin: -root.reach
            Layout.preferredHeight: height
            Layout.bottomMargin: Theme.layout.margin
            shown: Theme.list.filteredCountPosition === "bottom" && root.selectionCount === 0
            listed: list.count > 0
        }

        Rectangle {
            visible: root.selectionCount > 0
            Layout.fillWidth: true
            Layout.leftMargin: -root.reach
            Layout.preferredHeight: Theme.list.rowHeight
            Layout.bottomMargin: Theme.layout.margin
            radius: Theme.list.rowRadius
            color: Theme.list.selectionBarBackground

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 selected").arg(root.selectionCount)
                    color: Theme.list.selectionBarText
                    elide: Text.ElideRight
                }
                Icon {
                    Layout.rightMargin: 4
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
    }

    component FilteredCount: Item {
        property bool shown
        property bool listed

        height: visible ? Theme.list.rowHeight : 0
        visible: shown && listed && Theme.list.showFilteredCount && Library.notInstalledHidden > 0

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            spacing: 6

            Label {
                text: qsTr("%n not installed", "", Library.notInstalledHidden)
                color: Theme.list.filteredCountColor
                font.pixelSize: Theme.list.sortValueFontSize
            }
            Label {
                text: "·"
                color: Theme.list.filteredCountColor
                font.pixelSize: Theme.list.sortValueFontSize
            }
            Label {
                text: qsTr("Show")
                color: Theme.list.linkColor
                font.pixelSize: Theme.list.sortValueFontSize
                font.underline: showMouse.containsMouse

                MouseArea {
                    id: showMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Library.installedOnly = false
                }
            }
        }
    }
}
