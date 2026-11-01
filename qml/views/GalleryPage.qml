import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
FocusScope {
    id: root

    property real dragReserve: 0

    // Every item found whose game the library shows, and the games' names and platforms by id.
    property var all: []
    property var games: ({})
    property bool loading: false
    // `all` filtered and sorted: what the grid shows and the viewer steps through.
    property var shown: []
    // The grid as rows: a group heading ({header, key, ...}) or a row of tiles ({items: [{item, n}]}).
    property var rows: []
    property var groupInfo: ({})
    property var rowOfItem: []
    property var headerOfRow: []
    property int focusIndex: 0
    property bool keyboardFocus: false
    property int dragWidth: -1
    // For screenshots: what to open once the items are in (see Main.qml's galleryForScreenshot()).
    property string autoOpen: ""

    // `all` sorted, before the filters.
    property var sorted: []
    // What is ticked in each section, as {key: true}; empty = no filter from that section.
    property var typeSel: ({})
    property var platformSel: ({})
    property var gameSel: ({})
    property var monthSel: ({})
    // How many items each entry would show given the other sections' choices: {section: {key: n}}.
    property var counts: ({ type: {}, platform: {}, game: {}, month: {} })
    // Every entry the sidebar lists, from all the items found, so a filter grays an entry out instead of removing it.
    property var platformList: []
    property var gameList: []
    property var yearList: []
    property bool listsStale: false
    property bool panelOpen: false

    readonly property string grouping: General.galleryGroup
    readonly property bool grouped: grouping !== "none"
    readonly property bool newestFirst: General.gallerySort === "newest"
    readonly property real sideMargin: 2 * Theme.layout.margin
    readonly property real sidebarX: Theme.layout.margin
    readonly property bool hasSidebar: loading || all.length > 0
    readonly property bool compact: hasSidebar && Theme.gallery.compactWidth > 0 && width < Theme.gallery.compactWidth
    readonly property real sidebarWidth: Math.min(Theme.gallery.sidebarWidth, width - sidebarX - sideMargin)
    readonly property real gridX: hasSidebar && !compact ? sidebarX + sidebarWidth + sideMargin : sideMargin
    readonly property real gridWidth: width - gridX - sideMargin
    readonly property bool filtersActive: Object.keys(typeSel).length + Object.keys(platformSel).length
                                          + Object.keys(gameSel).length + Object.keys(monthSel).length > 0
    readonly property real tileTarget: dragWidth > 0 ? dragWidth
                                       : General.galleryTileWidth > 0 ? General.galleryTileWidth : Theme.gallery.tileWidth
    readonly property real cardPad: grouped ? Theme.gallery.groupPadding : 0
    readonly property real innerWidth: gridWidth - 2 * cardPad
    readonly property int columns: Math.max(1, Math.floor((innerWidth + Theme.gallery.tileGap) / (tileTarget + Theme.gallery.tileGap)))
    readonly property real tileWidth: (innerWidth - (columns - 1) * Theme.gallery.tileGap) / columns
    readonly property real tileHeight: tileWidth * 9 / 16
    readonly property bool showNames: grouping !== "game"
    readonly property bool captions: General.galleryCaptions
    readonly property real captionHeight: captions ? Theme.layout.spacing / 2 + Theme.gallery.gameNameSize * 1.6 : 0
    readonly property real rowHeight: tileHeight + captionHeight + Theme.gallery.tileGap
    // Every row's top, and the total at the end: exact, unlike the ListView's estimate from the rows it has made.
    readonly property var rowTops: {
        const tops = [0]
        for (const r of rows)
            tops.push(tops[tops.length - 1] + rowSpan(r))
        return tops
    }
    readonly property real contentTotal: rowTops[rowTops.length - 1]
    readonly property bool viewerOpen: viewer.item !== null && viewer.item.opened

    signal closeRequested()
    signal gameRequested(string id)
    signal message(string text)
    signal started()

    function start(gameId = "") {
        all = []
        sorted = []
        shown = []
        rows = []
        typeSel = ({})
        platformSel = ({})
        gameSel = gameId === "" ? ({}) : ({ [Library.mediaKey(gameId)]: true })
        monthSel = ({})
        counts = { type: {}, platform: {}, game: {}, month: {} }
        platformList = []
        gameList = []
        yearList = []
        panelOpen = false
        started()
        focusIndex = 0
        keyboardFocus = false
        games = Library.mediaGames()
        loading = true
        Library.allMedia()
        sinkOut.stop()
        riseIn.restart()
        forceActiveFocus()
    }

    function leave() {
        riseIn.stop()
        sinkOut.restart()
    }

    MotionNumber {
        id: riseIn
        target: rise
        property: "y"
        from: Theme.gallery.pageOffset
        to: 0
        speed: "slow"
    }

    // Not riseIn reversed: that saves its movement for the end, when the page has all but faded,
    // so it can't be seen. The fade-out runs alongside it, over the same time.
    MotionNumber {
        id: sinkOut
        target: rise
        property: "y"
        to: Theme.gallery.pageOffset
        speed: "normal"
    }

    function take(batch: var) {
        const added = []
        for (const item of batch) {
            if (games[item.gameId] === undefined)
                continue
            item.type = item.kind === "video" ? "clip" : item.kind
            item.stamp = item.time.getTime()
            item.platform = games[item.gameId].platform
            item.month = monthKey(item)
            added.push(item)
        }
        if (added.length > 0) {
            all = all.concat(added)
            listsStale = true
            refresh.restart()
        }
    }

    // Batches come a few at a time while the page fills, so they are laid out together.
    Timer {
        id: refresh
        interval: 150
        onTriggered: root.rebuild()
    }

    function rebuild() {
        refresh.stop()
        const sign = newestFirst ? -1 : 1
        const list = all.slice()
        list.sort((a, b) => (a.stamp - b.stamp) * sign || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))
        sorted = list
        if (listsStale)
            buildLists()
        applyFilters()
    }

    function buildLists() {
        listsStale = false
        const platforms = {}
        const gameIds = {}
        const months = {}
        for (const item of all) {
            gameIds[item.gameId] = true
            const game = games[item.gameId]
            platforms[game.platform] = { id: game.platform, name: game.platformName, icon: game.platformIcon }
            months[item.month] = true
        }
        platformList = Object.values(platforms).sort((a, b) => a.name.localeCompare(b.name))
        gameList = Object.keys(gameIds).map(id => ({ id: id, title: games[id].title }))
                         .sort((a, b) => a.title.localeCompare(b.title))
        const byYear = {}
        for (const key of Object.keys(months).sort().reverse()) {
            const year = Math.floor(parseInt(key) / 100)
            byYear[year] = byYear[year] ?? { year: year, months: [] }
            const n = parseInt(key) % 100
            byYear[year].months.push({ key: key, name: Qt.locale().monthName(n - 1, Locale.LongFormat) })
        }
        yearList = Object.values(byYear).sort((a, b) => b.year - a.year)
    }

    // One pass: the items every section lets through, and for each section how many it would show if
    // only the other sections counted.
    function applyFilters() {
        const tally = { type: {}, platform: {}, game: {}, month: {} }
        const out = []
        const useType = Object.keys(typeSel).length > 0
        const usePlatform = Object.keys(platformSel).length > 0
        const useGame = Object.keys(gameSel).length > 0
        const useMonth = Object.keys(monthSel).length > 0
        for (const item of sorted) {
            const okType = !useType || typeSel[item.type] === true
            const okPlatform = !usePlatform || platformSel[item.platform] === true
            const okGame = !useGame || gameSel[item.gameId] === true
            const okMonth = !useMonth || monthSel[item.month] === true
            const failing = (okType ? 0 : 1) + (okPlatform ? 0 : 1) + (okGame ? 0 : 1) + (okMonth ? 0 : 1)
            if (failing === 0)
                out.push(item)
            if (failing === 0 || (failing === 1 && !okType))
                tally.type[item.type] = (tally.type[item.type] ?? 0) + 1
            if (failing === 0 || (failing === 1 && !okPlatform))
                tally.platform[item.platform] = (tally.platform[item.platform] ?? 0) + 1
            if (failing === 0 || (failing === 1 && !okGame))
                tally.game[item.gameId] = (tally.game[item.gameId] ?? 0) + 1
            if (failing === 0 || (failing === 1 && !okMonth))
                tally.month[item.month] = (tally.month[item.month] ?? 0) + 1
        }
        counts = tally
        shown = out
        relayout()
    }

    function selection(section: string): var {
        return section === "type" ? typeSel : section === "platform" ? platformSel : section === "game" ? gameSel : monthSel
    }

    function setSelection(section: string, value: var) {
        if (section === "type")
            typeSel = value
        else if (section === "platform")
            platformSel = value
        else if (section === "game")
            gameSel = value
        else
            monthSel = value
        applyFilters()
    }

    function toggle(section: string, key: string) {
        const next = Object.assign({}, selection(section))
        if (next[key])
            delete next[key]
        else
            next[key] = true
        setSelection(section, next)
    }

    function toggleYear(year: var) {
        const next = Object.assign({}, monthSel)
        const every = year.months.every(m => next[m.key])
        for (const m of year.months) {
            if (every)
                delete next[m.key]
            else
                next[m.key] = true
        }
        setSelection("month", next)
    }

    function clear(section: string) {
        setSelection(section, ({}))
    }

    function clearAll() {
        typeSel = ({})
        platformSel = ({})
        gameSel = ({})
        monthSel = ({})
        applyFilters()
    }

    function monthKey(item: var): string {
        const t = item.time
        return t.getFullYear() * 100 + t.getMonth() + 1 + ""
    }

    function monthLabel(key: string): string {
        const n = parseInt(key)
        return Qt.locale().monthName(n % 100 - 1, Locale.LongFormat) + " " + Math.floor(n / 100)
    }

    // The month heading already names the month, so a Month tile gives only the day.
    function whenText(item: var): string {
        const t = item.time
        const day = grouping === "month" ? t.toLocaleDateString(Qt.locale(), qsTr("ddd d"))
                  : grouping === "game" ? t.toLocaleDateString(Qt.locale(), qsTr("ddd d MMM yyyy"))
                  : t.toLocaleDateString(Qt.locale(), qsTr("d MMM yyyy"))
        // Without the system locale the app is en_US, whose 12 h clock isn't wanted.
        const time = General.systemLocale ? t.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                                          : t.toLocaleTimeString(Qt.locale(), "HH:mm")
        return day + "  ·  " + time
    }

    function relayout() {
        const keepKey = anchorKey()
        const info = {}
        const groups = []
        const index = {}
        if (!grouped) {
            groups.push({ key: "", items: [] })
        } else {
            for (let n = 0; n < shown.length; ++n) {
                const item = shown[n]
                const key = grouping === "month" ? monthKey(item) : item.gameId
                let g = index[key]
                if (g === undefined) {
                    g = { key: key, items: [] }
                    index[key] = g
                    groups.push(g)
                    const game = games[item.gameId]
                    info[key] = grouping === "month" ? { label: monthLabel(key), icon: "" }
                                                     : { label: game.title, icon: game.platformIcon }
                }
                g.items.push(n)
            }
            if (grouping === "game")
                groups.sort((a, b) => info[a.key].label.localeCompare(info[b.key].label))
        }
        if (!grouped)
            for (let n = 0; n < shown.length; ++n)
                groups[0].items.push(n)

        const out = []
        const rowOf = new Array(shown.length)
        const headerOf = []
        for (const g of groups) {
            let header = -1
            if (grouped) {
                header = out.length
                info[g.key].count = g.items.length
                out.push({ header: true, key: g.key })
                headerOf.push(header)
            }
            for (let at = 0; at < g.items.length; at += columns) {
                const entries = []
                for (const n of g.items.slice(at, at + columns)) {
                    entries.push({ item: shown[n], n: n })
                    rowOf[n] = out.length
                }
                headerOf.push(header)
                out.push({ header: false, key: g.key, items: entries, last: grouped && at + columns >= g.items.length })
            }
        }
        groupInfo = info
        rowOfItem = rowOf
        headerOfRow = headerOf
        rows = out
        focusIndex = Math.min(focusIndex, Math.max(0, shown.length - 1))
        if (keepKey !== "")
            Qt.callLater(() => scrollTo(indexOfKey(keepKey), true))
    }

    // The item at the top of the grid, to stay on it when the layout changes under it.
    function rowSpan(r: var): real {
        return r.header ? Theme.gallery.groupHeight
             : r.last ? rowHeight - Theme.gallery.tileGap + cardPad + Theme.gallery.groupSpacing
             : rowHeight
    }

    function exactY(): real {
        const i = list.indexAt(0, list.contentY + 1)
        const item = i >= 0 ? list.itemAtIndex(i) : null
        return item ? rowTops[i] + list.contentY - item.y : 0
    }

    function scrollToY(y: real) {
        if (rows.length === 0)
            return
        y = Math.max(0, Math.min(y, contentTotal - list.height))
        let lo = 0
        let hi = rows.length - 1
        while (lo < hi) {
            const mid = (lo + hi + 1) >> 1
            if (rowTops[mid] <= y)
                lo = mid
            else
                hi = mid - 1
        }
        list.positionViewAtIndex(lo, ListView.Beginning)
        const item = list.itemAtIndex(lo)
        if (item)
            list.contentY = item.y + y - rowTops[lo]
    }

    function anchorKey(): string {
        if (rows.length === 0 || list.contentY < 1)
            return ""
        let row = list.indexAt(0, list.contentY + 1)
        while (row >= 0 && rows[row].header)
            ++row
        return row >= 0 && row < rows.length ? keyOf(rows[row].items[0].item) : ""
    }

    function keyOf(item: var): string {
        return item.gameId + "\n" + item.id
    }

    function indexOfKey(key: string): int {
        return shown.findIndex(i => keyOf(i) === key)
    }

    function scrollTo(n: int, atTop: bool) {
        if (n < 0 || n >= rowOfItem.length)
            return
        const row = rowOfItem[n]
        list.positionViewAtIndex(row, atTop ? ListView.Beginning : ListView.Contain)
        if (grouped && list.itemAtIndex(row) !== null && list.itemAtIndex(row).y - list.contentY < Theme.gallery.groupHeight)
            list.contentY = list.itemAtIndex(row).y - Theme.gallery.groupHeight
    }

    function focusMove(n: int) {
        if (shown.length === 0)
            return
        keyboardFocus = true
        focusIndex = Math.max(0, Math.min(shown.length - 1, n))
        scrollTo(focusIndex, false)
    }

    function focusRow(by: int) {
        if (shown.length === 0)
            return
        let row = rowOfItem[focusIndex] + by
        while (row >= 0 && row < rows.length && rows[row].header)
            row += by
        if (row < 0 || row >= rows.length)
            return
        const col = focusIndex - rows[rowOfItem[focusIndex]].items[0].n
        const items = rows[row].items
        focusMove(items[Math.min(col, items.length - 1)].n)
    }

    function open(n: int) {
        if (n < 0 || n >= shown.length)
            return
        focusIndex = n
        viewer.active = true
        viewer.item.games = root.games
        viewer.item.show(shown[n], shown)
    }

    function openForScreenshot() {
        let what = ""
        let at
        for (const token of autoOpen.split(";")) {
            if (token.startsWith("filter:")) {
                const [section, keys] = token.slice(7).split("=")
                const next = {}
                for (const key of keys.split(","))
                    next[key] = true
                setSelection(section, next)
            } else if (token === "panel") {
                panelOpen = true
            } else {
                [what, at] = token.split("/")
            }
        }
        autoOpen = ""
        if (what === "viewer")
            open(at === undefined ? 0 : parseInt(at))
        else if (what === "menu" && shown.length > 0)
            openMenu(shown[0], list, Qt.point(list.width / 4, list.height / 4))
    }

    function openMenu(item: var, tile: Item, position: point) {
        itemMenu.active = true
        itemMenu.item.media = item
        itemMenu.item.gameId = item.gameId
        itemMenu.item.popup(tile, position.x, position.y)
    }

    onColumnsChanged: if (rows.length > 0) relayout()
    onGroupingChanged: if (all.length > 0) relayout()
    onNewestFirstChanged: if (all.length > 0) rebuild()

    // The sidebar's search field and entries keep their own keys.
    readonly property bool gridKeys: !filters.activeFocus

    Keys.onEscapePressed: {
        if (panelOpen) {
            panelOpen = false
            forceActiveFocus()
        } else {
            root.closeRequested()
        }
    }
    Keys.onLeftPressed: event => { if (event.accepted = root.gridKeys) root.focusMove(root.focusIndex - 1) }
    Keys.onRightPressed: event => { if (event.accepted = root.gridKeys) root.focusMove(root.focusIndex + 1) }
    Keys.onUpPressed: event => { if (event.accepted = root.gridKeys) root.focusRow(-1) }
    Keys.onDownPressed: event => { if (event.accepted = root.gridKeys) root.focusRow(1) }
    onCompactChanged: if (!compact) panelOpen = false
    Keys.onPressed: event => {
        if (!root.gridKeys)
            return
        if (event.key === Qt.Key_Home)
            root.focusMove(0)
        else if (event.key === Qt.Key_End)
            root.focusMove(root.shown.length - 1)
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            root.open(root.focusIndex)
        else if (event.key === Qt.Key_PageUp)
            root.focusMove(root.focusIndex - root.columns * Math.max(1, Math.floor(list.height / root.rowHeight) - 1))
        else if (event.key === Qt.Key_PageDown)
            root.focusMove(root.focusIndex + root.columns * Math.max(1, Math.floor(list.height / root.rowHeight) - 1))
        else
            return
        event.accepted = true
    }

    Connections {
        target: Library
        function onAllMediaReady(batch, done) {
            if (!root.loading)
                return
            root.take(batch)
            if (done) {
                root.loading = false
                root.rebuild()
                root.openForScreenshot()
            }
        }
    }

    Surface {
        anchors.fill: parent
        fill: Theme.palette.background
        fillEnd: Theme.palette.backgroundEnd
        direction: Theme.palette.gradientDirection
    }

    ChromeButton {
        x: root.sidebarX
        y: Theme.layout.margin
        icon: Icons.arrowUp
        iconRotation: -90
        label: qsTr("Back to games")
        tip: qsTr("Back to games (Esc)")
        onClicked: root.closeRequested()
    }

    Item {
        id: content
        transform: Translate { id: rise }

        y: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing
        width: parent.width
        height: parent.height - y

        RowLayout {
            id: header
            x: root.gridX
            width: root.gridWidth
            spacing: 2 * Theme.layout.spacing

            Label {
                text: qsTr("Gallery")
                color: Theme.palette.text
                font.pixelSize: Theme.gallery.titleSize
                font.weight: Theme.font.emphasisWeight
            }
            Label {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: Theme.gallery.titleSize / 5
                visible: root.shown.length > 0
                text: root.filtersActive
                      ? qsTr("%1 of %2").arg(Number(root.shown.length).toLocaleString(Qt.locale(), "f", 0))
                                        .arg(root.countText(root.all.length, qsTr("item"), qsTr("items")))
                      : root.countText(root.shown.length, qsTr("item"), qsTr("items"))
                color: Theme.gallery.countColor
            }
            Label {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: Theme.gallery.titleSize / 5
                visible: root.loading
                text: qsTr("Looking for more…")
                color: Theme.gallery.countColor
            }
            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: Theme.gallery.buttonSpacing

                ChromeButton {
                    id: filtersButton
                    implicitHeight: Theme.gallery.buttonHeight
                    radius: Theme.gallery.buttonRadius
                    visible: root.compact
                    icon: Icons.filter
                    label: qsTr("Filters")
                    onClicked: {
                        root.panelOpen = !root.panelOpen
                        if (root.panelOpen)
                            filters.forceActiveFocus()
                        else
                            root.forceActiveFocus()
                    }

                    Rectangle {
                        visible: root.filtersActive
                        x: parent.width - width * 0.75
                        y: -height * 0.25
                        width: Theme.gallery.filterDotSize
                        height: width
                        radius: width / 2
                        color: Theme.gallery.filterDotColor
                    }
                }
                ChromeButton {
                    implicitHeight: Theme.gallery.buttonHeight
                    radius: Theme.gallery.buttonRadius
                    icon: root.captions ? Icons.closedCaptioningFill : Icons.closedCaptioningLine
                    tip: root.captions ? qsTr("Hide captions") : qsTr("Show captions")
                    onClicked: root.message(General.setGalleryCaptions(!root.captions))
                    Accessible.name: tip
                }
                ChromeButton {
                    id: groupButton
                    implicitHeight: Theme.gallery.buttonHeight
                    radius: Theme.gallery.buttonRadius
                    icon: Icons.layoutMasonryLine
                    label: root.grouping === "month" ? qsTr("Group: Month") : root.grouping === "game" ? qsTr("Group: Game")
                                                                                                       : qsTr("Group: None")
                    onClicked: groupMenu.popup(parent, x, y + height + Theme.layout.spacing / 2)

                    SettingsMenu {
                        id: groupMenu
                        pops: true
                        popDirection: "down"
                        minWidth: groupButton.width
                        checkColumn: true

                        Repeater {
                            model: [{ id: "month", text: qsTr("Month") }, { id: "game", text: qsTr("Game") },
                                    { id: "none", text: qsTr("None") }]
                            SettingsMenuItem {
                                required property var modelData
                                text: modelData.text
                                checked: root.grouping === modelData.id
                                onTriggered: root.message(General.setGalleryGroup(modelData.id))
                            }
                        }
                    }
                }
                ChromeButton {
                    id: sortButton
                    implicitHeight: Theme.gallery.buttonHeight
                    radius: Theme.gallery.buttonRadius
                    icon: Icons.arrowDown
                    label: root.newestFirst ? qsTr("Newest first") : qsTr("Oldest first")
                    iconRotation: root.newestFirst ? 0 : 180
                    onClicked: sortMenu.popup(parent, x, y + height + Theme.layout.spacing / 2)

                    SettingsMenu {
                        id: sortMenu
                        pops: true
                        popDirection: "down"
                        minWidth: sortButton.width
                        checkColumn: true

                        SettingsMenuItem {
                            text: qsTr("Newest first")
                            checked: root.newestFirst
                            onTriggered: root.message(General.setGallerySort("newest"))
                        }
                        SettingsMenuItem {
                            text: qsTr("Oldest first")
                            checked: !root.newestFirst
                            onTriggered: root.message(General.setGallerySort("oldest"))
                        }
                    }
                }
                Surface {
                    id: sizePill
                    implicitHeight: Theme.gallery.buttonHeight
                    implicitWidth: sizeRow.implicitWidth + 2 * sizeRow.inset
                    radius: Theme.gallery.buttonRadius
                    fill: Theme.iconButton.background
                    fillEnd: Theme.iconButton.backgroundEnd
                    direction: Theme.iconButton.gradientDirection
                    border.color: Theme.iconButton.border
                    border.width: Theme.iconButton.borderWidth

                    function resize(width) {
                        root.dragWidth = Math.round(Math.max(sizeSlider.from, Math.min(sizeSlider.to, width)))
                        saveSize.restart()
                    }

                    // Inline components can't see the file's ids, hence the inset passed in.
                    component SizeStep: Rectangle {
                        id: step
                        property bool plus
                        property string tip
                        property real inset
                        signal clicked()
                        implicitWidth: Theme.gallery.buttonHeight - 2 * inset
                        implicitHeight: implicitWidth
                        radius: Math.max(0, Theme.gallery.buttonRadius - inset)
                        color: stepMouse.containsMouse ? Theme.iconButton.hoverBackground : "transparent"
                        scale: stepMouse.pressed ? Theme.motion.pressScale : 1

                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                        Behavior on scale { enabled: Motion.on; MotionNumber {} }

                        // Bars rather than an Icon path: scaled to this size, the path's 2-unit bar
                        // falls between pixel rows and blurs.
                        Item {
                            id: glyph
                            readonly property int length: Math.round((Theme.iconButton.iconSize - 4) * 14 / 24)
                            readonly property int thickness: Math.max(1, Math.round(length / 8))
                            x: Math.round((step.width - length) / 2)
                            y: Math.round((step.height - length) / 2)
                            width: length
                            height: length

                            Rectangle {
                                y: Math.round((glyph.length - glyph.thickness) / 2)
                                width: glyph.length
                                height: glyph.thickness
                                color: Theme.iconButton.icon
                            }
                            Rectangle {
                                visible: step.plus
                                x: Math.round((glyph.length - glyph.thickness) / 2)
                                width: glyph.thickness
                                height: glyph.length
                                color: Theme.iconButton.icon
                            }
                        }
                        MouseArea {
                            id: stepMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: step.clicked()
                        }
                        ToolTip.visible: stepMouse.containsMouse
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: step.tip
                        Accessible.role: Accessible.Button
                        Accessible.name: step.tip
                    }

                    Row {
                        id: sizeRow
                        readonly property real inset: Theme.layout.spacing / 2
                        anchors.centerIn: parent
                        spacing: Theme.layout.spacing / 2

                        SizeStep {
                            anchors.verticalCenter: parent.verticalCenter
                            inset: sizeRow.inset
                            tip: qsTr("Smaller tiles")
                            onClicked: sizePill.resize(root.tileTarget - Theme.gallery.sliderStep)
                        }
                        SettingsSlider {
                            id: sizeSlider
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.gallery.sliderWidth
                            handleSize: Theme.gallery.sliderHandleSize
                            trackHeight: Theme.gallery.sliderTrackHeight
                            fillColor: Theme.gallery.sliderFill
                            from: Theme.gallery.tileMinWidth
                            to: Math.max(Theme.gallery.tileMinWidth + 1, Theme.gallery.tileMaxWidth)
                            stepSize: 1
                            value: root.tileTarget
                            onMoved: sizePill.resize(value)
                            Accessible.name: qsTr("Tile size")
                        }
                        SizeStep {
                            anchors.verticalCenter: parent.verticalCenter
                            plus: true
                            inset: sizeRow.inset
                            tip: qsTr("Larger tiles")
                            onClicked: sizePill.resize(root.tileTarget + Theme.gallery.sliderStep)
                        }
                    }
                }
            }
        }

        // Writing settings.toml on every step of a drag would be wasteful.
        Timer {
            id: saveSize
            interval: 400
            onTriggered: {
                const width = root.dragWidth
                root.dragWidth = -1
                if (width > 0)
                    root.message(General.setGalleryTileWidth(width))
            }
        }

        ListView {
            id: list
            x: root.gridX
            y: header.height + Theme.layout.spacing
            width: root.gridWidth
            height: parent.height - y
            clip: true
            model: root.rows
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: Math.max(root.rowHeight * 2, height)

            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool isHeader: modelData.header
                readonly property bool isLast: modelData.last === true
                property int hotCount: 0

                z: hotCount > 0 ? 2 : 1
                width: list.width
                height: root.rowSpan(modelData)

                CardSlice {
                    visible: root.grouped
                    width: parent.width
                    height: row.isLast ? parent.height - Theme.gallery.groupSpacing : parent.height
                    first: row.isHeader
                    last: row.isLast
                }
                GroupHeading {
                    visible: row.isHeader
                    x: root.cardPad
                    width: parent.width - 2 * root.cardPad
                    height: parent.height
                    info: row.isHeader ? root.groupInfo[row.modelData.key] : null
                }

                Repeater {
                    model: row.isHeader ? [] : row.modelData.items

                    Item {
                        id: cell
                        required property var modelData
                        required property int index

                        readonly property bool hot: tile.hovered

                        x: root.cardPad + index * (root.tileWidth + Theme.gallery.tileGap)
                        width: root.tileWidth
                        height: root.rowHeight
                        z: hot ? 1 : 0
                        onHotChanged: row.hotCount += hot ? 1 : -1

                        MediaTile {
                            id: tile
                            width: root.tileWidth
                            height: root.tileHeight
                            media: cell.modelData.item
                            showTooltip: false
                            cornerRadius: Theme.gallery.tileRadius
                            shadowSize: Theme.gallery.tileShadowSize
                            shadowColor: Theme.gallery.tileShadowColor
                            hoverScale: Theme.gallery.tileHoverScale
                            borderColor: Theme.gallery.tileBorderColor
                            focusRing: root.keyboardFocus && root.activeFocus && root.focusIndex === cell.modelData.n
                            onClicked: {
                                root.keyboardFocus = false
                                root.forceActiveFocus()
                                root.open(cell.modelData.n)
                            }
                            onMenuRequested: position => root.openMenu(cell.modelData.item, tile, position)
                        }
                        Row {
                            visible: root.captions
                            y: root.tileHeight + Theme.layout.spacing / 2

                            Label {
                                visible: root.showNames
                                width: Math.max(0, Math.min(implicitWidth, cell.width - dateLabel.implicitWidth))
                                elide: Text.ElideRight
                                text: root.games[cell.modelData.item.gameId]?.title ?? ""
                                color: Theme.gallery.gameNameColor
                                font.pixelSize: Theme.gallery.gameNameSize
                            }
                            Label {
                                id: dateLabel
                                text: (root.showNames ? "  ·  " : "") + root.whenText(cell.modelData.item)
                                color: Theme.gallery.dateColor
                                font.pixelSize: Theme.gallery.gameNameSize
                            }
                        }
                    }
                }
            }
        }

        // In the right margin, clear of the cards. Not attached to the list: rows differ in height
        // and only those near the view exist, so the list's contentHeight is an estimate that
        // changes as rows are made, which made the handle jump and a drag jerk the grid. This bar
        // goes by the exact row tops instead.
        SettingsScrollBar {
            id: gridBar
            trackless: true
            thickness: Theme.gallery.scrollbarWidth
            orientation: Qt.Vertical
            x: list.x + list.width + (root.sideMargin - width) / 2
            y: list.y
            height: list.height
            size: root.contentTotal > 0 ? Math.min(1, list.height / root.contentTotal) : 1
            readonly property real total: root.contentTotal
            onTotalChanged: follow()

            function follow() {
                if (!pressed)
                    position = root.contentTotal > 0 ? root.exactY() / root.contentTotal : 0
            }

            onPressedChanged: follow()
            onPositionChanged: if (pressed) root.scrollToY(position * root.contentTotal)

            Connections {
                target: list
                function onContentYChanged() { gridBar.follow() }
            }
        }

        // The heading of the group at the top stays there while its tiles scroll under it; the
        // next group's card pushes it up out of the way.
        Item {
            id: sticky
            readonly property int topRow: root.grouped && root.rows.length > 0 ? list.indexAt(0, list.contentY + 1) : -1
            readonly property int headerRow: sticky.topRow >= 0 ? root.headerOfRow[sticky.topRow] : -1
            readonly property int nextHeader: {
                if (sticky.headerRow < 0)
                    return -1
                for (let r = sticky.topRow + 1; r < root.rows.length; ++r)
                    if (root.rows[r].header)
                        return r
                return -1
            }
            readonly property real push: {
                const next = sticky.nextHeader >= 0 ? list.itemAtIndex(sticky.nextHeader) : null
                return next === null ? 0 : Math.min(0, next.y - list.contentY - Theme.gallery.groupSpacing
                                                       - root.cardPad - height)
            }
            visible: sticky.headerRow >= 0
            x: list.x
            y: list.y
            z: 2
            width: list.width
            height: Theme.gallery.groupHeight
            clip: true

            Item {
                y: sticky.push
                width: parent.width
                height: parent.height

                Rectangle {
                    anchors.fill: parent
                    color: Theme.gallery.groupBackground
                }
                CardSlice {
                    anchors.fill: parent
                    first: true
                }
                GroupHeading {
                    x: root.cardPad
                    width: parent.width - 2 * root.cardPad
                    height: parent.height
                    info: sticky.headerRow >= 0 ? root.groupInfo[root.rows[sticky.headerRow].key] : null
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: list
            visible: !root.loading && root.shown.length === 0
            width: Math.min(list.width - 2 * Theme.layout.margin, 420)
            spacing: Theme.layout.spacing * 1.5

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.all.length === 0 ? qsTr("No clips or screenshots found") : qsTr("Nothing matches these filters")
                color: Theme.gallery.countColor
                font.pixelSize: Theme.font.size
            }
            Label {
                visible: root.all.length === 0
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Lattis looks for Steam's clips and screenshots, the media folders set in a game's Properties, and the screenshot folders of the emulators it knows, for the games in your library.")
                color: Theme.gallery.countColor
            }
            SettingsButton {
                visible: root.all.length > 0
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Clear all filters")
                onClicked: root.clearAll()
            }
        }

        // Closes the panel of a narrow window when the grid is clicked.
        MouseArea {
            anchors.fill: list
            z: 4
            enabled: root.panelOpen
            onClicked: {
                root.panelOpen = false
                root.forceActiveFocus()
            }
        }

        GalleryFilters {
            id: filters
            page: root
            panel: root.compact
            visible: root.hasSidebar && (!root.compact || opacity > 0)
            opacity: !root.compact || root.panelOpen ? 1 : 0
            z: 5
            x: root.sidebarX
            y: root.compact ? list.y : 0
            width: root.sidebarWidth
            height: parent.height - y - Theme.layout.margin
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
        }
    }

    function countText(n: int, one: string, many: string): string {
        return Number(n).toLocaleString(Qt.locale(), "f", 0) + " " + (n === 1 ? one : many)
    }

    component GroupHeading: Item {
        id: heading

        property var info: null

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.layout.spacing * 1.5

            Icon {
                visible: (heading.info?.icon ?? "") !== ""
                anchors.verticalCenter: parent.verticalCenter
                size: Theme.gallery.groupIconSize
                path: Icons.forPlatform(heading.info?.icon ?? "")
                color: Theme.gallery.groupColor
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: heading.info?.label ?? ""
                color: Theme.gallery.groupColor
                font.pixelSize: Theme.gallery.groupSize
                font.weight: Theme.font.emphasisWeight
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "·  " + (heading.info?.count ?? 0)
                color: Theme.gallery.groupCountColor
                font.pixelSize: Theme.gallery.groupSize - 2
            }
        }
    }

    // One row's band of its group's card: the card is drawn whole and clipped, so the rows join
    // without seams and only the first and last show its rounded edges.
    component CardSlice: Item {
        id: slice

        property bool first: false
        property bool last: false

        clip: true

        Rectangle {
            readonly property real hidden: Math.max(radius, border.width) + 1
            y: slice.first ? 0 : -hidden
            width: parent.width
            height: parent.height + (slice.first ? 0 : hidden) + (slice.last ? 0 : hidden)
            radius: Theme.gallery.groupRadius
            color: Theme.gallery.groupCardBackground
            border.color: Theme.gallery.groupCardBorder
            border.width: Theme.gallery.groupCardBorderWidth
            antialiasing: true
        }
    }

    Loader {
        id: viewer
        active: false
        source: "MediaViewer.qml"
    }
    Connections {
        target: viewer.item
        function onMessage(text) { root.message(text) }
        function onGameRequested(id) { root.gameRequested(id) }
        function onClosed() {
            root.scrollTo(root.focusIndex, false)
            root.forceActiveFocus()
        }
        function onCurrentChanged() {
            const n = root.indexOfKey(root.keyOf(viewer.item.current))
            if (n >= 0)
                root.focusIndex = n
        }
    }

    Loader {
        id: itemMenu
        active: false
        source: "ClipMenu.qml"
    }
    Connections {
        target: itemMenu.item
        function onMessage(text) { root.message(text) }
        function onGameRequested(id) { root.gameRequested(id) }
    }
}
