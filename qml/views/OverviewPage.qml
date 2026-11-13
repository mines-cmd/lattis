import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
FocusScope {
    id: root

    property real dragReserve: 0
    property var overview: ({})
    property bool achievementsRead: false
    // The app ids ProtonDB is still to be asked about, one at a time at the pace ProtonDb keeps.
    property var protonQueue: []
    property bool protonAsking: false
    readonly property bool loaded: overview.games !== undefined
    // Replacing `overview` remakes every card, ~40 ms on the main thread, so the results are kept between
    // opens and a refresh that shows nothing new is dropped; one that does waits for the rise to end.
    property string overviewKey: ""
    property var pendingOverview: null
    property bool revealed: false
    readonly property bool revealedCards: loaded && revealed
    property real barsGrow: 1

    // Three columns of cards, two below compactWidth, one below narrowWidth.
    readonly property int columns: Theme.overview.compactWidth > 0 && width < Theme.overview.compactWidth
        ? (Theme.overview.narrowWidth > 0 && width < Theme.overview.narrowWidth ? 1 : 2) : 3
    readonly property real sideMargin: 2 * Theme.layout.margin
    readonly property real contentWidth: Math.min(Theme.overview.maxWidth, width - 2 * sideMargin)
    readonly property var continuePlaying: overview.continuePlaying ?? []

    onLoadedChanged: if (!loaded) stacks.reset()

    function cardShown(name: string): bool {
        return !General.overviewHidden.includes(name)
    }

    // The full-width cards sit in rows 0-1 above the columns (row 2) and 3-4 below them.
    function rowOf(name: string): int {
        const above = General.overviewAbove.indexOf(name)
        return above >= 0 ? above : 3 + General.overviewBelow.indexOf(name)
    }

    function rowVisible(name: string): bool {
        return cardShown(name) && (name !== "continue_playing" || continuePlaying.length > 0)
    }

    // The entry next to `name` in `order` that is on screen (a hidden or empty card in between is skipped), or "".
    function neighbour(order, name: string, step: int, isShown): string {
        for (let i = order.indexOf(name) + step; i >= 0 && i < order.length; i += step)
            if (isShown(order[i]))
                return order[i]
        return ""
    }

    function swapped(order, a: string, b: string) {
        const out = order.slice()
        out[order.indexOf(a)] = b
        out[order.indexOf(b)] = a
        return out
    }

    function report(error: string) {
        if (error !== "")
            message(error)
    }

    // The first change fixes the columns to where the cards are on screen now, so nothing else moves.
    function fixColumns(): var {
        const lists = stacks.currentLists()
        if (General.overviewAutomatic)
            report(General.setOverviewColumns(lists.left, lists.right))
        return lists
    }

    function openCardMenu(name: string, item: Item, position: point) {
        cardMenu.active = true
        const menu = cardMenu.item
        const isRow = name === "tiles" || name === "continue_playing"
        menu.card = name
        menu.canLeft = menu.canRight = menu.canBelow = menu.canAbove = false
        if (isRow) {
            const order = General.overviewAbove.includes(name) ? General.overviewAbove : General.overviewBelow
            menu.canUp = neighbour(order, name, -1, rowVisible) !== ""
            menu.canDown = neighbour(order, name, 1, rowVisible) !== ""
            menu.canBelow = General.overviewAbove.includes(name)
            menu.canAbove = !menu.canBelow
        } else {
            const lists = stacks.currentLists()
            const order = stacks.count === 1 ? lists.left.concat(lists.right)
                : lists.left.includes(name) ? lists.left : lists.right
            menu.canUp = neighbour(order, name, -1, stacks.isShown) !== ""
            menu.canDown = neighbour(order, name, 1, stacks.isShown) !== ""
            if (stacks.count === 2) {
                menu.canLeft = !lists.left.includes(name)
                menu.canRight = !menu.canLeft
            }
        }
        menu.popup(item, position.x, position.y)
    }

    function hideCard(name: string) {
        if (name !== "tiles" && name !== "continue_playing")
            fixColumns()
        report(General.setOverviewHidden(General.overviewHidden.concat(name)))
    }

    function moveCard(name: string, step: int) {
        if (name === "tiles" || name === "continue_playing") {
            const above = General.overviewAbove.includes(name)
            const order = above ? General.overviewAbove : General.overviewBelow
            const other = neighbour(order, name, step, rowVisible)
            if (other === "")
                return
            const next = swapped(order, name, other)
            report(General.setOverviewRows(above ? next : General.overviewAbove, above ? General.overviewBelow : next))
            return
        }
        const lists = fixColumns()
        const joined = stacks.count === 1
        const order = joined ? lists.left.concat(lists.right) : lists.left.includes(name) ? lists.left : lists.right
        const other = neighbour(order, name, step, stacks.isShown)
        if (other === "")
            return
        const next = swapped(order, name, other)
        if (joined)
            report(General.setOverviewColumns(next.slice(0, lists.left.length), next.slice(lists.left.length)))
        else if (order === lists.left)
            report(General.setOverviewColumns(next, lists.right))
        else
            report(General.setOverviewColumns(lists.left, next))
    }

    function moveToColumn(name: string, side: int) {
        const lists = fixColumns()
        const from = side === 0 ? lists.right : lists.left
        const to = side === 0 ? lists.left : lists.right
        from.splice(from.indexOf(name), 1)
        to.splice(stacks.slotIn(to, name), 0, name)
        report(General.setOverviewColumns(lists.left, lists.right))
    }

    function moveRowCard(name: string, above: bool) {
        const rows = [General.overviewAbove.filter(n => n !== name), General.overviewBelow.filter(n => n !== name)]
        // Next to the columns, so the card stays where it was in relation to them.
        if (above)
            rows[0].push(name)
        else
            rows[1].unshift(name)
        report(General.setOverviewRows(rows[0], rows[1]))
    }

    signal message(string text)
    signal attentionRequested(string kind)
    signal closeRequested()
    signal filterRequested(string platform, var statuses, var tags)
    signal yearRequested(int year)
    signal allBySizeRequested()
    signal gameRequested(string id)
    signal playRequested(string id)
    signal statsRequested()

    function warmUp() {
        if (!loaded)
            Library.overview()
    }

    function start() {
        stopAskingProtonDb()
        Library.overview()
        revealed = false
        revealed = true
        if (loaded)
            regrowIn.restart()
        sinkOut.stop()
        riseIn.restart()
        forceActiveFocus()
    }

    // Free space moves with every write to the drive; only what the Storage card prints counts.
    function keyOf(result): string {
        return JSON.stringify(result, (key, value) => key === "free" ? Library.sizeText(value) : value)
    }

    function applyOverview(result) {
        pendingOverview = null
        const key = keyOf(result)
        if (key === overviewKey)
            return
        overviewKey = key
        overview = result
    }

    function stopAskingProtonDb() {
        protonQueue = []
        protonAsking = false
        askWatchdog.stop()
        protonRefresh.stop()
    }

    function askProtonDb() {
        while (protonQueue.length > 0) {
            const appId = protonQueue.shift()
            if (ProtonDb.request(appId)) {
                protonAsking = true
                askWatchdog.restart()
                return
            }
        }
        protonAsking = false
        askWatchdog.stop()
    }

    function leave() {
        stopAskingProtonDb()
        riseIn.stop()
        sinkOut.restart()
    }

    function hoursText(minutes: int): string {
        return minutes <= 0 ? qsTr("0 min") : Library.shortPlayTime(minutes)
    }

    function daysText(minutes: int): string {
        return minutes < 1440 ? qsTr("Less than a day of your life") : qsTr("%1 days of your life").arg((minutes / 1440).toFixed(1))
    }

    function openTile(target: string) {
        if (target === "stats")
            statsRequested()
        else if (target === "steam")
            filterRequested("steam", [], [])
        else
            filterRequested("", target === "backlog" ? ["not_started"] : [], [])
    }

    // Steam is the only source with achievements, so the tile says it covers Steam games and not the library.
    function achievementsTile(o): var {
        const a = o.achievements
        const tile = { label: qsTr("Achievements"), icon: Icons.medal, color: Theme.palette.text, target: "steam" }
        if (a === undefined)
            return Object.assign(tile, { value: "…", sub: qsTr("Reading Steam games") })
        if (a.total <= 0)
            return Object.assign(tile, { value: "–", sub: qsTr("No achievements in Steam games") })
        return Object.assign(tile, { value: qsTr("%1%").arg(Math.floor(100 * a.unlocked / a.total)),
                                     sub: qsTr("%n perfect · Steam games", "", a.perfect) })
    }

    readonly property var tiles: {
        const o = overview
        if (!loaded)
            return []
        return [
            { label: qsTr("Games"), value: String(o.games), icon: Icons.stackLine, color: Theme.palette.text,
              sub: qsTr("%1 installed on %n platform(s)", "", o.platforms).arg(o.installed), target: "all" },
            { label: qsTr("Hours played"), value: hoursText(o.minutes), icon: Icons.timeLine, color: Theme.overview.tilePrimaryValue,
              sub: daysText(o.minutes), target: "stats" },
            { label: qsTr("Backlog"), value: String(o.backlog), icon: Icons.calendarCheckLine, color: Theme.palette.text,
              sub: o.games > 0 ? qsTr("%1% of the library").arg(Math.round(100 * o.backlog / o.games)) : qsTr("Nothing in the library"), target: "backlog" },
            achievementsTile(o)
        ]
    }

    Keys.onEscapePressed: root.closeRequested()
    Keys.onPressed: event => {
        const page = flick.height - Theme.layout.margin
        const line = Theme.overview.sectionSpacing * 4
        const steps = { [Qt.Key_Down]: line, [Qt.Key_Up]: -line, [Qt.Key_PageDown]: page, [Qt.Key_PageUp]: -page }
        const top = Math.max(0, flick.contentHeight - flick.height)
        if (event.key === Qt.Key_Home)
            flick.contentY = 0
        else if (event.key === Qt.Key_End)
            flick.contentY = top
        else if (event.key in steps)
            flick.contentY = Math.max(0, Math.min(top, flick.contentY + steps[event.key]))
        else
            return
        event.accepted = true
    }

    OverviewPlatformInfo {
        id: platformInfo
        platformList: root.overview.platformList ?? []
    }

    Connections {
        target: Library
        function onOverviewReady(result) {
            if (root.loaded && riseIn.running)
                root.pendingOverview = result
            else
                root.applyOverview(result)
            root.protonQueue = result.protonDbPending ?? []
            if (!askWatchdog.running && root.visible)
                root.askProtonDb()
            // The pass can end before the first answer lands, which then has no achievements.
            if (root.achievementsRead && result.achievements === undefined)
                Library.overview()
        }
        function onOverviewAchievementsRead() {
            root.achievementsRead = true
            // Also while hidden, so the results kept for the next open have them.
            if (root.loaded)
                Library.overview()
        }
        function onGamesChanged() {
            if (root.visible && root.loaded)
                Library.overview()
        }
    }

    Connections {
        target: GameSaves
        function onBackedUpAll() {
            if (root.loaded)
                Library.overview()
        }
    }

    // The singleton is only built when the page asks, and only listened to while it does.
    Connections {
        target: root.protonAsking ? ProtonDb : null
        function onChanged() {
            if (!protonRefresh.running)
                protonRefresh.start()
            root.askProtonDb()
        }
    }

    // Another part of the app can take the one request ProtonDb holds, which leaves no answer to wait for.
    Timer {
        id: askWatchdog
        interval: 15000
        onTriggered: root.askProtonDb()
    }

    // The card fills in over a minute or two; it is worked out again now and then, not for each answer.
    Timer {
        id: protonRefresh
        interval: 8000
        onTriggered: if (root.visible && root.loaded) Library.overview()
    }

    MotionNumber {
        id: riseIn
        target: rise
        property: "y"
        from: Theme.overview.pageOffset
        to: 0
        speed: "slow"
        onStopped: if (root.pendingOverview && rise.y === 0) root.applyOverview(root.pendingOverview)
    }

    MotionNumber {
        id: regrowIn
        target: root
        property: "barsGrow"
        from: 0
        to: 1
        speed: "slow"
    }

    // Not riseIn reversed: that saves its movement for the end, when the page has all but faded.
    MotionNumber {
        id: sinkOut
        target: rise
        property: "y"
        to: Theme.overview.pageOffset
        speed: "normal"
    }

    Surface {
        anchors.fill: parent
        fill: Theme.palette.background
        fillEnd: Theme.palette.backgroundEnd
        direction: Theme.palette.gradientDirection
    }

    Item {
        width: parent.width - root.dragReserve
        height: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing

        DragHandler {
            target: null
            enabled: !(root.Window.window?.toolbarHovered ?? false)
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
    }

    ChromeButton {
        x: Theme.layout.margin
        y: Theme.layout.margin
        icon: Icons.arrowUp
        iconRotation: -90
        label: qsTr("Back to games")
        tip: qsTr("Back to games (Esc)")
        onClicked: root.closeRequested()
    }

    Flickable {
        id: flick

        y: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing
        width: parent.width
        height: parent.height - y
        clip: true
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * root.sideMargin
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            transform: Translate { id: rise }

            x: (flick.width - width) / 2
            width: root.contentWidth
            spacing: Theme.overview.sectionSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: 2 * Theme.overview.sectionSpacing

                Label {
                    text: qsTr("Library overview")
                    color: Theme.palette.text
                    font.pixelSize: Theme.overview.titleSize
                    font.weight: Theme.font.emphasisWeight
                }
                Item { Layout.fillWidth: true }
                ChromeButton {
                    icon: Icons.barChartFill
                    label: qsTr("Play statistics")
                    tip: qsTr("Play statistics")
                    onClicked: root.statsRequested()
                }
            }

            GridLayout {
                visible: root.loaded
                Layout.fillWidth: true
                columns: 1
                rowSpacing: Theme.overview.sectionSpacing

                GridLayout {
                    visible: root.cardShown("tiles")
                    Layout.row: root.rowOf("tiles")
                    Layout.fillWidth: true
                    columns: root.columns === 3 ? Math.max(1, root.tiles.length) : root.columns === 2 ? 2 : 1
                    columnSpacing: Theme.overview.sectionSpacing
                    rowSpacing: Theme.overview.sectionSpacing
                    uniformCellWidths: true

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: point => root.openCardMenu("tiles", parent, point.position)
                    }

                    Repeater {
                        // A count, not the array: a new array would rebuild the tiles and fade them in again.
                        model: root.tiles.length

                        OverviewReveal {
                            id: tileReveal
                            shown: root.revealedCards
                            required property int index
                            readonly property var modelData: root.tiles[index]
                            Layout.fillWidth: true
                            order: index

                            OverviewCard {
                                id: tileCard
                                Layout.fillWidth: true
                                horizontalPadding: Theme.overview.tilePaddingX
                                watermark: tileReveal.modelData.icon
                                scale: tap.pressed ? Theme.motion.pressScale : 1

                                Behavior on scale { enabled: Motion.on; MotionNumber {} }

                                TapHandler {
                                    id: tap
                                    onTapped: root.openTile(tileReveal.modelData.target)
                                }
                                HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }

                                Rectangle {
                                    parent: tileCard
                                    anchors.fill: parent
                                    radius: tileCard.radius
                                    color: Theme.overview.tileHoverTint
                                    opacity: tileHover.hovered ? 1 : 0

                                    Behavior on opacity { MotionNumber { fade: true } }
                                }

                                CardLabel {
                                    text: tileReveal.modelData.label
                                    color: Theme.overview.textMuted
                                }
                                Label {
                                    text: tileReveal.modelData.value
                                    color: tileReveal.modelData.color
                                    font.pixelSize: Theme.overview.tileValueSize
                                    font.weight: Theme.font.strongWeight
                                }
                                OverviewQuiet {
                                    Layout.fillWidth: true
                                    text: tileReveal.modelData.sub
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                OverviewReveal {
                    id: continueReveal
                    shown: root.revealedCards
                    name: "continue_playing"
                    Layout.row: root.rowOf("continue_playing")
                    Layout.fillWidth: true
                    visible: root.continuePlaying.length > 0 && root.cardShown("continue_playing")
                    order: root.tiles.length
                    onMenuRequested: position => root.openCardMenu("continue_playing", continueReveal, position)

                    OverviewCard {
                        Layout.fillWidth: true

                        OverviewCardTitle { text: qsTr("Continue playing") }

                        Flickable {
                            id: coverStrip
                            Layout.fillWidth: true
                            Layout.preferredHeight: coverRow.implicitHeight
                            contentWidth: coverRow.implicitWidth
                            contentHeight: coverRow.implicitHeight
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.HorizontalFlick
                            ScrollBar.horizontal: ScrollBar { visible: coverStrip.contentWidth > coverStrip.width }

                            // The covers share the card's width, so a few games don't leave it mostly empty.
                            readonly property real coverWidth: {
                                const count = Math.max(1, root.continuePlaying.length)
                                const share = (width - (count - 1) * Theme.overview.coverSpacing) / count
                                return Math.max(Theme.overview.coverWidth, Math.min(Theme.overview.coverMaxWidth, share))
                            }

                            Row {
                                id: coverRow
                                spacing: Theme.overview.coverSpacing

                                Repeater {
                                    model: root.continuePlaying

                                    Item {
                                        id: entry
                                        required property var modelData
                                        readonly property bool hovered: hover.hovered

                                        width: coverStrip.coverWidth
                                        implicitHeight: cover.height + caption.implicitHeight + Theme.layout.spacing

                                        ArtThumb {
                                            id: cover
                                            width: parent.width
                                            height: Math.round(width * Theme.overview.coverRatio)
                                            cornerRadius: Theme.overview.coverRadius
                                            color: Theme.overview.coverPlaceholder
                                            iconWhenEmpty: true
                                            source: entry.modelData.coverUrl
                                            zoom: entry.hovered ? Theme.overview.coverHoverZoom : 1
                                            Behavior on zoom { enabled: Motion.on; MotionNumber {} }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.gameRequested(entry.modelData.id)
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: cover.radius
                                                gradient: Gradient {
                                                    GradientStop { position: 0; color: "transparent" }
                                                    GradientStop { position: 1; color: Theme.overview.coverHoverOverlay }
                                                }
                                                opacity: entry.hovered ? 1 : 0
                                                visible: opacity > 0
                                                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                                                Rectangle {
                                                    id: playButton
                                                    anchors.right: parent.right
                                                    anchors.bottom: parent.bottom
                                                    anchors.margins: Theme.overview.coverPlayInset
                                                    width: Theme.overview.coverPlaySize
                                                    height: width
                                                    radius: width / 2
                                                    color: playMouse.containsMouse ? Theme.overview.coverPlayHoverBackground
                                                                                   : Theme.overview.coverPlayBackground
                                                    scale: playMouse.pressed ? Theme.motion.pressScale : 1
                                                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                                                    Behavior on scale { enabled: Motion.on; MotionNumber {} }

                                                    Icon {
                                                        anchors.centerIn: parent
                                                        size: Theme.overview.coverPlayIconSize
                                                        color: Theme.overview.coverPlayIcon
                                                        path: Icons.playFill
                                                    }
                                                    MouseArea {
                                                        id: playMouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: root.playRequested(entry.modelData.id)
                                                    }
                                                    Accessible.role: Accessible.Button
                                                    Accessible.name: qsTr("Play")
                                                }
                                            }
                                        }

                                        HoverHandler { id: hover }

                                        Column {
                                            id: caption
                                            anchors.top: cover.bottom
                                            anchors.topMargin: Theme.layout.spacing
                                            width: parent.width

                                            Label {
                                                width: parent.width
                                                text: entry.modelData.title
                                                color: Theme.palette.text
                                                font.weight: Theme.font.emphasisWeight
                                                elide: Text.ElideRight
                                            }
                                            OverviewQuiet {
                                                width: parent.width
                                                text: entry.modelData.lastPlayedText !== "" ? entry.modelData.lastPlayedText
                                                                                            : qsTr("Not played yet")
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Cards differ in height and come and go with the library, so no fixed pairing or split keeps
                // both columns level. Each card, in this order, goes in the shorter column. A card that has
                // been on screen keeps its column, so a refresh doesn't move cards around while they are read;
                // the ones below the view are dealt again, since cards above them grow as answers arrive
                // (Compatibility as ProtonDB answers) and the first deal no longer levels the columns.
                Item {
                    id: stacks

                    property real stackHeight: 0
                    readonly property int count: root.columns === 1 ? 1 : 2
                    readonly property real gap: Theme.overview.sectionSpacing
                    readonly property real columnWidth: (width - (count - 1) * gap) / count

                    Layout.row: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: stackHeight

                    function cardNamed(name) {
                        for (let i = 0; i < children.length; ++i)
                            if (children[i].name === name)
                                return children[i]
                        return null
                    }

                    function isShown(name) {
                        return cardNamed(name)?.visible ?? false
                    }

                    // Each column's cards top to bottom, the ones not on screen too. Automatic dealing is read off
                    // the screen; a single column has no right side, so it all goes left.
                    function currentLists() {
                        if (!General.overviewAutomatic)
                            return { left: General.overviewLeft.slice(), right: General.overviewRight.slice() }
                        const shown = []
                        for (let i = 0; i < children.length; ++i)
                            if (children[i].visible && children[i].column >= 0)
                                shown.push(children[i])
                        shown.sort((a, b) => a.y - b.y)
                        const lists = { left: [], right: [] }
                        for (const card of shown)
                            (count === 1 || card.column === 0 ? lists.left : lists.right).push(card.name)
                        return lists
                    }

                    // Where a card moved into `names` goes so it stays level with where it was.
                    function slotIn(names, name) {
                        const y = cardNamed(name).y
                        for (let i = 0; i < names.length; ++i) {
                            const other = cardNamed(names[i])
                            if (other?.visible && other.y > y)
                                return i
                        }
                        return names.length
                    }

                    function reset() {
                        for (let i = 0; i < children.length; ++i) {
                            children[i].column = -1
                            children[i].seen = false
                        }
                        Qt.callLater(relayout)
                    }

                    function markSeen() {
                        if (!root.revealedCards)
                            return
                        const viewBottom = mapFromItem(flick, 0, flick.height).y
                        for (let i = 0; i < children.length; ++i) {
                            const card = children[i]
                            if (card.visible && card.column >= 0 && card.y < viewBottom)
                                card.seen = true
                        }
                    }

                    function relayout() {
                        markSeen()
                        const bottoms = new Array(count).fill(0)
                        const last = new Array(count).fill(null)
                        const kept = []
                        const dealt = []
                        const fixed = !General.overviewAutomatic
                        const byName = {}
                        for (let i = 0; i < children.length; ++i) {
                            const card = children[i]
                            if (!card.visible)
                                continue
                            byName[card.name] = card
                            if (!fixed && card.seen && card.column >= 0 && card.column < count)
                                kept.push(card)
                            else if (!fixed)
                                dealt.push(card)
                        }
                        if (fixed) {
                            // Left column first, so a single column reads the left one then the right one.
                            for (const [side, names] of [[0, General.overviewLeft], [1, General.overviewRight]]) {
                                for (const name of names) {
                                    const card = byName[name]
                                    if (!card)
                                        continue
                                    card.column = count === 1 ? 0 : side
                                    kept.push(card)
                                    delete byName[name]
                                }
                            }
                            dealt.push(...Object.values(byName))
                        }
                        // Seen cards go first: an unseen card dealt above one would push it down in view.
                        for (const card of kept.concat(dealt)) {
                            if (dealt.includes(card))
                                card.column = bottoms.indexOf(Math.min(...bottoms))
                            card.x = card.column * (columnWidth + gap)
                            card.y = bottoms[card.column]
                            card.width = columnWidth
                            card.height = card.implicitHeight
                            card.stretch = false
                            bottoms[card.column] += card.implicitHeight + gap
                            last[card.column] = card
                        }
                        stackHeight = Math.max(0, Math.max(...bottoms) - gap)
                        if (count === 2) {
                            const shorter = bottoms[0] < bottoms[1] ? 0 : 1
                            const difference = Math.abs(bottoms[0] - bottoms[1])
                            if (difference > 0 && difference <= Theme.overview.columnStretch && last[shorter]) {
                                last[shorter].height += difference
                                last[shorter].stretch = true
                            }
                        }
                    }

                    onCountChanged: reset()
                    onWidthChanged: relayout()

                    Connections {
                        target: flick
                        function onContentYChanged() { stacks.markSeen() }
                    }
                    Connections {
                        target: root
                        function onRevealedCardsChanged() { stacks.markSeen() }
                    }
                    Connections {
                        target: General
                        function onOverviewChanged() { Qt.callLater(stacks.relayout) }
                    }
                    Component.onCompleted: {
                        for (let i = 0; i < children.length; ++i) {
                            const card = children[i]
                            card.implicitHeightChanged.connect(() => Qt.callLater(relayout))
                            card.visibleChanged.connect(() => Qt.callLater(relayout))
                            card.menuRequested.connect(position => root.openCardMenu(card.name, card, position))
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "storage"
                        visible: storage.hasContent && root.cardShown("storage")
                        order: root.tiles.length + 1

                        OverviewStorage {
                            regrow: root.barsGrow
                            id: storage
                            Layout.fillWidth: true
                            overview: root.overview
                            info: platformInfo
                            stacked: true
                            onGameRequested: id => root.gameRequested(id)
                            onAllBySizeRequested: root.allBySizeRequested()
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "platforms"
                        visible: ((root.overview.platformCounts ?? []).length > 0) && root.cardShown("platforms")
                        order: root.tiles.length + 2

                        OverviewPlatforms {
                            regrow: root.barsGrow
                            Layout.fillWidth: true
                            overview: root.overview
                            info: platformInfo
                            onFilterRequested: platform => root.filterRequested(platform, [], [])
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "statuses"
                        visible: ((root.overview.statusCounts ?? []).length > 0) && root.cardShown("statuses")
                        order: root.tiles.length + 3

                        OverviewStatuses {
                            regrow: root.barsGrow
                            Layout.fillWidth: true
                            overview: root.overview
                            onFilterRequested: status => root.filterRequested("", [status], [])
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "attention"
                        visible: ((root.overview.attention ?? []).length > 0) && root.cardShown("attention")
                        order: root.tiles.length + 4

                        OverviewAttention {
                            Layout.fillWidth: true
                            overview: root.overview
                            onActionRequested: kind => root.attentionRequested(kind)
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "backlog"
                        visible: ((root.overview.backlogPicks ?? []).length > 0) && root.cardShown("backlog")
                        order: root.tiles.length + 5

                        OverviewBacklog {
                            Layout.fillWidth: true
                            overview: root.overview
                            info: platformInfo
                            onGameRequested: id => root.gameRequested(id)
                            onPlayRequested: id => root.playRequested(id)
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "nearly_done"
                        visible: ((root.overview.nearlyDone ?? []).length > 0) && root.cardShown("nearly_done")
                        order: root.tiles.length + 6

                        OverviewNearlyDone {
                            regrow: root.barsGrow
                            Layout.fillWidth: true
                            overview: root.overview
                            onGameRequested: id => root.gameRequested(id)
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "tags"
                        visible: ((root.overview.topTags ?? []).length > 0) && root.cardShown("tags")
                        order: root.tiles.length + 7

                        OverviewTags {
                            regrow: root.barsGrow
                            Layout.fillWidth: true
                            overview: root.overview
                            onFilterRequested: tag => root.filterRequested("", [], [tag])
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "years"
                        visible: ((root.overview.years ?? []).length > 0) && root.cardShown("years")
                        order: root.tiles.length + 8

                        OverviewYears {
                            regrow: root.barsGrow
                            Layout.fillWidth: true
                            overview: root.overview
                            onYearRequested: year => root.yearRequested(year)
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "compatibility"
                        visible: compatibility.hasContent && root.cardShown("compatibility")
                        order: root.tiles.length + 9

                        OverviewCompatibility {
                            regrow: root.barsGrow
                            id: compatibility
                            readonly property bool hasContent: games > 0 || (stats.native ?? 0) + (stats.proton ?? 0) > 0
                            Layout.fillWidth: true
                            overview: root.overview
                            asking: root.protonAsking
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "saves"
                        visible: ((root.overview.saves?.played ?? 0) > 0 || (root.overview.saves?.archives ?? 0) > 0) && root.cardShown("saves")
                        order: root.tiles.length + 10

                        OverviewSaves {
                            regrow: root.barsGrow
                            Layout.fillWidth: true
                            overview: root.overview
                        }
                    }

                    OverviewReveal {
                        shown: root.revealedCards
                        name: "facts"
                        visible: root.cardShown("facts")
                        order: root.tiles.length + 11

                        OverviewFacts {
                            Layout.fillWidth: true
                            overview: root.overview
                            onGameRequested: id => root.gameRequested(id)
                            onSteamRequested: root.filterRequested("steam", [], [])
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: cardMenu
        active: false
        source: "OverviewCardMenu.qml"
    }
    Connections {
        target: cardMenu.item
        function onClosed() { root.forceActiveFocus() }
        function onHideRequested() { root.hideCard(cardMenu.item.card) }
        function onLeftRequested() { root.moveToColumn(cardMenu.item.card, 0) }
        function onRightRequested() { root.moveToColumn(cardMenu.item.card, 1) }
        function onUpRequested() { root.moveCard(cardMenu.item.card, -1) }
        function onDownRequested() { root.moveCard(cardMenu.item.card, 1) }
        function onBelowRequested() { root.moveRowCard(cardMenu.item.card, false) }
        function onAboveRequested() { root.moveRowCard(cardMenu.item.card, true) }
    }
}
