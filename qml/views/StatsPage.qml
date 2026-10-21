import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// See docs/code/qml.md.
FocusScope {
    id: root

    property real dragReserve: 0

    property string mode: "month"
    property int year: 0
    property int month: 0
    property int monthYear: 0
    property var stats: ({})
    // The last month and year that landed. Each view's own cards read these instead of `stats`, so
    // the view on its way out keeps its period while it crossfades with the other.
    property var monthStats: ({})
    property var yearStats: ({})
    readonly property bool yearMode: mode === "year"
    readonly property bool loaded: stats.year !== undefined
    readonly property bool hasHistory: loaded && stats.hasHistory
    readonly property bool yearShape: stats.columns !== undefined
    readonly property bool ready: hasHistory
    property bool yearUsed: false
    readonly property bool compact: Theme.stats.compactWidth > 0 && width < Theme.stats.compactWidth
    readonly property real sideMargin: 2 * Theme.layout.margin
    readonly property real contentWidth: Math.min(Theme.stats.maxWidth, width - 2 * sideMargin)
    readonly property var today: new Date()
    readonly property int shownYear: stats.year ?? year
    readonly property int shownMonthYear: monthStats.year ?? year
    readonly property int shownMonth: monthStats.month ?? month
    readonly property bool shownCurrentMonth: shownMonthYear === today.getFullYear() && shownMonth === today.getMonth() + 1
    property bool resetting: false
    readonly property bool growing: Motion.on && !resetting
    property bool filling: false
    readonly property bool fading: growing && !filling
    // False while a period lands that has nothing to move from (the first after opening, or the other
    // view's cards coming back), so columns, lines and rows land in place instead of sliding from the
    // empty layout.
    property bool settled: false
    readonly property bool shifting: growing && settled
    // 0 shows the month view's cards, 1 the year view's. In between both show, their bars scaled by
    // their view's presence as well as faded, so the outgoing bars sink as the others grow instead of
    // two charts standing over each other; fading one out before the other in would empty the card.
    property real yearness: yearShape ? 1 : 0
    Behavior on yearness { enabled: root.growing; MotionNumber { speed: "slow" } }
    readonly property real monthPresence: 1 - yearness
    readonly property real yearPresence: yearness

    // Hours by platform's rows; grows only, since a new count rebuilds every row.
    property int platformSlots: 8

    function keep(yearly: bool, result: var) {
        if (yearly)
            yearStats = result
        else
            monthStats = result
    }

    function land(result: var) {
        const yearly = result.columns !== undefined
        platformSlots = Math.max(platformSlots, (result.platforms ?? []).length)
        if (loaded && yearly !== yearShape) {
            resetting = true
            filling = true
            settled = false
            keep(yearly, {})
            resetting = false
            keep(yearly, result)
            filling = false
            settled = true
            stats = result
            return
        }
        filling = !loaded
        settled = loaded
        keep(yearly, result)
        stats = result
        settled = true
        filling = false
    }

    signal closeRequested()
    signal gameRequested(string id)

    function start() {
        resetting = true
        mode = "month"
        year = today.getFullYear()
        month = today.getMonth() + 1
        monthYear = year
        settled = false
        stats = {}
        monthStats = {}
        yearStats = {}
        resetting = false
        reload()
        sinkOut.stop()
        riseIn.restart()
        forceActiveFocus()
    }

    MotionNumber {
        id: riseIn
        target: rise
        property: "y"
        from: Theme.stats.pageOffset
        to: 0
        speed: "slow"
    }

    function leave() {
        riseIn.stop()
        sinkOut.restart()
    }

    // Not riseIn reversed: that saves its movement for the end, when the page has all but faded,
    // so it can't be seen. Main.qml holds the fade-out back until this has mostly moved.
    MotionNumber {
        id: sinkOut
        target: rise
        property: "y"
        to: Theme.stats.pageOffset
        speed: "slow"
    }

    function reload() {
        if (yearMode)
            Library.yearStats(year)
        else
            Library.monthStats(year, month)
    }

    function pick(y: int, m: int) {
        if (!yearMode && y === year && m === month)
            return
        mode = "month"
        year = y
        month = m
        monthYear = y
        reload()
    }

    function pickYear(y: int) {
        if (yearMode && y === year)
            return
        mode = "year"
        yearUsed = true
        year = y
        reload()
    }

    function setMode(next: string) {
        if (next === mode)
            return
        if (next === "year") {
            pickYear(year)
            return
        }
        // The month you left is only meaningful in its own year; in any other, the last month you could pick.
        if (year !== monthYear)
            month = year === today.getFullYear() ? today.getMonth() + 1 : 12
        pick(year, month)
    }

    property bool pickerPending: false
    onStatsChanged: {
        if (pickerPending && stats.hasHistory) {
            pickerPending = false
            openPicker()
        }
    }

    function openPicker() {
        pickerLoader.active = true
        const p = pickerLoader.item
        p.mode = mode
        p.allowYearView = true
        p.pickedYear = year
        p.pickedMonth = month
        p.firstYear = stats.firstYear
        p.firstMonth = stats.firstMonth
        p.todayYear = today.getFullYear()
        p.todayMonth = today.getMonth() + 1
        p.history = stats.historyMonths ?? []
        p.show()
    }

    function step(delta: int) {
        if (!(delta < 0 ? canPrevious : canNext))
            return
        if (yearMode) {
            pickYear(year + delta)
            return
        }
        const key = year * 12 + month - 1 + delta
        pick(Math.floor(key / 12), key % 12 + 1)
    }

    readonly property bool canPrevious: hasHistory && (yearMode ? year > stats.firstYear
        : year * 12 + month > stats.firstYear * 12 + stats.firstMonth)
    readonly property bool canNext: hasHistory && (yearMode ? year < today.getFullYear()
        : year * 12 + month < today.getFullYear() * 12 + today.getMonth() + 1)

    function spanText(firstMonth: int, firstDay: int, lastMonth: int, lastDay: int): string {
        return qsTr("%1 %2 – %3 %4").arg(firstDay).arg(shortMonth(firstMonth)).arg(lastDay).arg(shortMonth(lastMonth))
    }

    function streakText(): string {
        const first = new Date(shownYear, stats.streakFirstMonth - 1, stats.streakFirstDay)
        const last = new Date(shownYear, stats.streakFirstMonth - 1, stats.streakFirstDay + stats.streakDays - 1)
        return stats.streakDays === 1 ? qsTr("%1 %2").arg(first.getDate()).arg(shortMonth(first.getMonth() + 1))
            : spanText(first.getMonth() + 1, first.getDate(), last.getMonth() + 1, last.getDate())
    }

    function monthScaleOf(columns: var): var {
        let most = 60
        for (const c of columns)
            most = Math.max(most, c.minutes)
        const hours = most / 60
        let stepHours = [5, 10, 20, 25, 50, 100].find(s => Math.ceil(hours / s) <= 4)
        if (stepHours === undefined)
            stepHours = Math.ceil(hours / 4 / 100) * 100
        const top = Math.ceil(hours / stepHours) * stepHours
        const ticks = []
        for (let t = 0; t <= top; t += stepHours)
            ticks.push(t)
        return { top: top, ticks: ticks }
    }
    readonly property var monthScale: monthScaleOf(yearStats.columns ?? [])

    readonly property var yearStripItems: {
        if (!yearStats.hasHistory)
            return []
        return yearStats.years.map(y => ({
            year: y.year, month: 0, label: String(y.year), minutes: y.minutes, hasHistory: y.minutes > 0,
            selected: y.year === year, available: true, future: false,
            tip: qsTr("%1 · %2").arg(y.year + (y.year === today.getFullYear() ? qsTr(" so far") : ""))
                     .arg(y.minutes > 0 ? hoursText(y.minutes) : qsTr("no history"))
        }))
    }
    readonly property var monthStripItems: {
        if (!monthStats.hasHistory)
            return []
        const firstKey = monthStats.firstYear * 12 + monthStats.firstMonth
        return monthStats.strip.map(m => ({
            year: m.year, month: m.month, label: shortMonth(m.month), minutes: m.minutes, hasHistory: m.hasHistory,
            selected: m.year === year && m.month === month,
            available: m.pickable && m.year * 12 + m.month >= firstKey, future: !m.pickable,
            tip: qsTr("%1 %2 · %3").arg(longMonth(m.month)).arg(m.year)
                     .arg(m.hasHistory ? hoursText(m.minutes) : qsTr("no history"))
        }))
    }

    function shortMonth(m: int): string {
        return Qt.locale().monthName(m - 1, Locale.ShortFormat)
    }

    function longMonth(m: int): string {
        return Qt.locale().monthName(m - 1, Locale.LongFormat)
    }

    function hoursText(minutes: int): string {
        return minutes <= 0 ? qsTr("0 min") : Library.shortPlayTime(minutes)
    }

    function share(minutes: int): real {
        return stats.minutes > 0 ? minutes / stats.minutes : 0
    }

    function percentText(minutes: int): string {
        const percent = share(minutes) * 100
        return percent > 0 && percent < 0.5 ? qsTr("<1%") : qsTr("%1%").arg(Math.round(percent))
    }

    function lengthText(minutes: int): string {
        if (minutes < 60)
            return qsTr("%1 min").arg(minutes)
        const h = Math.floor(minutes / 60)
        const m = minutes % 60
        return m === 0 ? qsTr("%1 h").arg(h) : qsTr("%1 h %2 min").arg(h).arg(m)
    }

    function dateText(day: int): string {
        const wd = new Date(shownMonthYear, shownMonth - 1, day).getDay()
        return qsTr("%1 %2 %3").arg(Qt.locale().dayName(wd, Locale.ShortFormat)).arg(day).arg(shortMonth(shownMonth))
    }

    function rangeText(first: int, last: int): string {
        return first === last ? qsTr("%1 %2").arg(first).arg(shortMonth(shownMonth))
                              : qsTr("%1–%2 %3").arg(first).arg(last).arg(shortMonth(shownMonth))
    }

    readonly property var tiles: {
        if (!ready)
            return []
        const s = stats
        const primary = Theme.stats.tilePrimaryValue
        const platformCount = s.platforms.length
        const todayMonth = today.getMonth() + 1
        const currentYear = shownYear === today.getFullYear()
        const hoursSub = yearShape
            ? (currentYear ? qsTr("So far, %1").arg(spanText(1, 1, todayMonth, today.getDate())) : qsTr("In %1").arg(shownYear))
            : (shownCurrentMonth ? qsTr("So far, %1").arg(rangeText(1, today.getDate())) : qsTr("In %1").arg(longMonth(shownMonth)))
        const list = [
            { label: qsTr("Hours played"), amount: s.minutes, kind: "hours", color: primary, icon: Icons.timeLine, sub: hoursSub },
            { label: qsTr("Sessions"), amount: s.sessions, kind: "count", color: Theme.palette.text, icon: Icons.calendarCheckLine,
              sub: qsTr("On %n day(s)", "", s.activeDays) },
            { label: qsTr("Games played"), amount: s.games, kind: "count", color: Theme.palette.text, icon: Icons.stackLine,
              sub: qsTr("On %n platform(s)", "", platformCount) }
        ]
        const previousMonth = shownMonth === 1 ? 12 : shownMonth - 1
        if (yearShape) {
            if (s.previousHasHistory) {
                const diff = s.minutes - s.previousMinutes
                list.push({
                    label: qsTr("Change"),
                    amount: diff, kind: "change",
                    color: diff > 0 ? Theme.stats.deltaUp : diff < 0 ? Theme.stats.deltaDown : Theme.palette.text,
                    icon: Icons.barChartBoxLine,
                    sub: s.comparedThroughMonth > 0
                         ? qsTr("vs %1 %2").arg(spanText(1, 1, s.comparedThroughMonth, s.comparedThroughDay)).arg(shownYear - 1)
                         : qsTr("vs %1").arg(shownYear - 1)
                })
            } else {
                list.push({ label: qsTr("Change"), amount: 0, kind: "none", color: Theme.stats.textMuted, icon: Icons.barChartBoxLine,
                            sub: qsTr("No history before %1").arg(shownYear) })
            }
        } else if (s.previousHasHistory) {
            const diff = s.minutes - s.previousMinutes
            list.push({
                label: qsTr("Change"),
                amount: diff, kind: "change",
                color: diff > 0 ? Theme.stats.deltaUp : diff < 0 ? Theme.stats.deltaDown : Theme.palette.text,
                icon: Icons.barChartBoxLine,
                sub: s.comparedThrough > 0
                     ? qsTr("vs 1–%1 %2").arg(s.comparedThrough).arg(longMonth(previousMonth))
                     : qsTr("vs %1").arg(longMonth(previousMonth))
            })
        } else {
            list.push({ label: qsTr("Change"), amount: 0, kind: "none", color: Theme.stats.textMuted, icon: Icons.barChartBoxLine,
                        sub: qsTr("No history before %1").arg(longMonth(shownMonth)) })
        }
        return list
    }

    function tileValue(kind: string, amount: real): string {
        const n = Math.round(amount)
        if (kind === "none")
            return "—"
        if (kind === "hours")
            return hoursText(n)
        if (kind === "change")
            return n === 0 ? hoursText(0) : (n > 0 ? "+" : "−") + hoursText(Math.abs(n))
        return String(n)
    }

    readonly property var dayScale: {
        let most = 60
        for (const d of monthStats.days ?? [])
            most = Math.max(most, d.minutes)
        const hours = most / 60
        const stepHours = hours <= 4 ? 1 : hours <= 8 ? 2 : hours <= 16 ? 4 : 6
        const top = Math.ceil(hours / stepHours) * stepHours
        const ticks = []
        for (let t = 0; t <= top; t += stepHours)
            ticks.push(t)
        return { top: top, ticks: ticks }
    }

    Keys.onEscapePressed: root.closeRequested()
    Keys.onLeftPressed: root.step(-1)
    Keys.onRightPressed: root.step(1)

    Connections {
        target: Library
        function onMonthStatsReady(y, m, result) {
            if (!root.yearMode && y === root.year && m === root.month)
                root.land(result)
        }
        function onYearStatsReady(y, result) {
            if (root.yearMode && y === root.year)
                root.land(result)
        }
        function onHistoryChanged() {
            if (root.visible)
                root.reload()
        }
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
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
    }

    component CardFrame: Surface {
        radius: Theme.stats.cardRadius
        fill: Theme.stats.cardBackground
        fillEnd: Theme.stats.cardBackgroundEnd
        direction: Theme.layout.gradientDirection
        border.color: Theme.stats.cardBorder
        border.width: Theme.layout.borderWidth
    }

    component Card: CardFrame {
        id: card

        default property alias content: inner.data
        property real padding: Theme.stats.cardPadding
        property real horizontalPadding: padding
        property string watermark
        // Off for the cards that swap between the views, which sit on one frame that stays, since
        // two crossfading frames would let the background show through halfway.
        property bool framed: true

        implicitHeight: inner.implicitHeight + 2 * padding
        fill: framed ? Theme.stats.cardBackground : "transparent"
        fillEnd: framed ? Theme.stats.cardBackgroundEnd : "transparent"
        border.width: framed ? Theme.layout.borderWidth : 0

        Loader {
            anchors.fill: parent
            active: card.watermark !== "" && Theme.stats.tileIconSize > 0
            sourceComponent: Item {
                Item {
                    id: watermarkLayer
                    anchors.fill: parent
                    clip: true
                    visible: false
                    layer.enabled: true

                    Icon {
                        x: card.width + size * (Theme.stats.tileIconOverhang - 1)
                        y: card.height + size * (Theme.stats.tileIconOverhang - 1)
                        path: card.watermark
                        size: Theme.stats.tileIconSize
                        color: Theme.stats.tileIcon
                    }
                }

                // Plain clip would cut the icon square across the card's rounded corner.
                Rectangle {
                    id: watermarkMask
                    anchors.fill: parent
                    radius: card.radius
                    visible: false
                    layer.enabled: true
                }

                MultiEffect {
                    anchors.fill: parent
                    source: watermarkLayer
                    maskEnabled: true
                    maskSource: watermarkMask
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                    opacity: Theme.stats.tileIconOpacity
                }
            }
        }

        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.topMargin: parent.padding
            anchors.bottomMargin: parent.padding
            anchors.leftMargin: parent.horizontalPadding
            anchors.rightMargin: parent.horizontalPadding
            spacing: Theme.layout.spacing * 2
        }
    }

    component CardTitle: Label {
        color: Theme.palette.text
        font.pixelSize: Theme.stats.cardTitleSize
        font.weight: Theme.font.emphasisWeight
    }

    component Muted: Label {
        color: Theme.stats.textMuted
        font.pixelSize: Theme.font.smallSize
    }

    component Turns: Item {
        id: turns

        property var value
        property string keyField
        property var first: null
        property var second: null
        property bool firstShown: true
        property real firstOpacity: 1
        property real secondOpacity: 0
        property var lastKey

        Behavior on firstOpacity { enabled: root.fading; MotionNumber { fade: true; speed: "normal" } }
        Behavior on secondOpacity { enabled: root.fading; MotionNumber { fade: true; speed: "normal" } }

        function keyOf(v: var): var {
            return keyField !== "" && v ? v[keyField] : v
        }

        onValueChanged: {
            const key = keyOf(value)
            if (key === lastKey) {
                if (firstShown)
                    first = value
                else
                    second = value
                return
            }
            lastKey = key
            if (firstShown)
                second = value
            else
                first = value
            firstShown = !firstShown
            firstOpacity = firstShown ? 1 : 0
            secondOpacity = firstShown ? 0 : 1
        }
        Component.onCompleted: {
            first = value
            lastKey = keyOf(value)
        }
    }

    component FadeLabel: Item {
        id: fadeLabel

        property string text
        property color color: Theme.stats.textMuted
        property real pixelSize: Theme.font.smallSize
        property int weight: Font.Normal
        property int horizontalAlignment: Text.AlignLeft
        property int elide: Text.ElideNone
        property bool tabular: false

        implicitWidth: (textTurns.firstShown ? firstText : secondText).implicitWidth
        implicitHeight: firstText.implicitHeight

        Turns { id: textTurns; value: fadeLabel.text }

        Label {
            id: firstText
            width: parent.width
            text: textTurns.first ?? ""
            opacity: textTurns.firstOpacity
            visible: opacity > 0
            color: fadeLabel.color
            font.pixelSize: fadeLabel.pixelSize
            font.weight: fadeLabel.weight
            font.features: fadeLabel.tabular ? { "tnum": 1 } : {}
            horizontalAlignment: fadeLabel.horizontalAlignment
            elide: fadeLabel.elide
        }
        Label {
            id: secondText
            width: parent.width
            text: textTurns.second ?? ""
            opacity: textTurns.secondOpacity
            visible: opacity > 0
            color: fadeLabel.color
            font.pixelSize: fadeLabel.pixelSize
            font.weight: fadeLabel.weight
            font.features: fadeLabel.tabular ? { "tnum": 1 } : {}
            horizontalAlignment: fadeLabel.horizontalAlignment
            elide: fadeLabel.elide
        }
    }

    component ShareText: FadeLabel {
        pixelSize: Theme.font.size
        tabular: true
        // A longer old value spills left, away from the bar's end.
        horizontalAlignment: Text.AlignRight
    }

    component Share: RowLayout {
        property int minutes
        spacing: Theme.layout.spacing

        ShareText { text: root.hoursText(minutes) }
        ShareText { text: "·" }
        ShareText { text: root.percentText(minutes) }
    }

    component ShareBar: Rectangle {
        id: shareBar
        property int minutes
        Layout.fillWidth: true
        Layout.preferredHeight: 6
        radius: height / 2
        color: Theme.stats.track

        property real fraction: 0
        Component.onCompleted: fraction = Qt.binding(() => root.share(shareBar.minutes))
        Behavior on fraction { enabled: root.growing; MotionNumber { speed: "slow" } }

        Surface {
            height: parent.height
            width: parent.width * shareBar.fraction
            radius: height / 2
            fill: Theme.stats.bar
            fillEnd: Theme.stats.barEnd
            opacity: Theme.stats.barOpacity
        }
    }

    component GridLines: Repeater {
        id: gridLines
        required property var scale

        model: Math.max(6, scale.ticks.length)

        Rectangle {
            required property int index
            readonly property bool used: index < gridLines.scale.ticks.length
            property real at: used ? gridLines.scale.ticks[index] / gridLines.scale.top : 1
            Behavior on at { enabled: root.shifting; MotionNumber { speed: "slow" } }

            y: parent.height * (1 - at)
            width: parent.width
            height: 1
            color: Theme.palette.border
            opacity: used ? 1 : 0
            visible: opacity > 0

            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "normal" } }
        }
    }

    component TickLabels: Repeater {
        id: tickLabels
        required property var scale

        model: Math.max(6, scale.ticks.length)

        FadeLabel {
            required property int index
            readonly property bool used: index < tickLabels.scale.ticks.length
            property real at: used ? tickLabels.scale.ticks[index] / tickLabels.scale.top : 1
            Behavior on at { enabled: root.shifting; MotionNumber { speed: "slow" } }

            anchors.right: parent.right
            anchors.rightMargin: Theme.layout.spacing
            y: parent.height * (1 - at) - height / 2
            horizontalAlignment: Text.AlignRight
            text: used ? qsTr("%1 h").arg(tickLabels.scale.ticks[index]) : ""
        }
    }

    component ListSlot: Item {
        id: listSlot

        required property int index
        property var entry: null
        property var shown: ({})
        default property alias content: slotRow.data
        readonly property real gap: index > 0 ? Theme.layout.spacing * 2 : 0
        property real presence: 0

        onEntryChanged: if (entry !== null) shown = entry
        Component.onCompleted: {
            if (entry !== null)
                shown = entry
            presence = Qt.binding(() => listSlot.entry !== null ? 1 : 0)
        }
        Behavior on presence { enabled: root.shifting; MotionNumber { speed: "normal" } }

        Layout.fillWidth: true
        Layout.preferredHeight: (Theme.stats.rowHeight + gap) * presence
        visible: presence > 0
        clip: true
        opacity: presence

        Item {
            id: slotRow
            y: listSlot.gap
            width: parent.width
            height: Theme.stats.rowHeight
        }
    }

    component Strip: Row {
        id: strip

        property var items: []
        property bool yearly: false
        property real presence: 1

        opacity: presence
        visible: opacity > 0

        spacing: Theme.layout.spacing
        Accessible.role: Accessible.List
        Accessible.name: yearly ? qsTr("Years with history") : qsTr("Months of %1").arg(root.shownMonthYear)

        Repeater {
            // A count, so picking a month keeps the bars and their colors fade to the new pick.
            model: strip.items.length

            Item {
                id: stripMonth
                required property int index
                readonly property var modelData: strip.items[index]
                readonly property bool selected: modelData.selected
                readonly property real most: Math.max(60, ...strip.items.map(m => m.minutes))
                readonly property bool available: modelData.available
                property real share: 0

                Component.onCompleted: share = Qt.binding(() => stripMonth.modelData.hasHistory
                    ? Math.max(Theme.stats.barRadius / Theme.stats.stripHeight, stripMonth.modelData.minutes / stripMonth.most)
                    : 0)
                Behavior on share { enabled: root.growing; MotionNumber { speed: "slow" } }

                width: strip.yearly ? Theme.stats.stripYearWidth : 34
                height: Theme.stats.stripHeight + stripLabel.implicitHeight + Theme.layout.spacing

                Rectangle {
                    width: parent.width
                    height: Theme.stats.stripHeight
                    radius: Theme.stats.barRadius
                    color: stripMonth.available ? Theme.stats.track : "transparent"
                    opacity: stripMouse.containsMouse ? 0.8 : 1

                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                    Surface {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * stripMonth.share * strip.presence
                        radius: Theme.stats.barRadius
                        fill: stripMonth.selected ? Theme.stats.barActive : Theme.stats.bar
                        fillEnd: stripMonth.selected ? Theme.stats.barActiveEnd : Theme.stats.barEnd
                        opacity: stripMonth.selected || stripMouse.containsMouse ? 1 : Theme.stats.barOpacity

                        Behavior on fill { enabled: Motion.fades; MotionColor {} }
                        Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                    }
                }
                Label {
                    id: stripLabel
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: stripMonth.modelData.label
                    color: stripMonth.selected ? Theme.palette.text : Theme.stats.textMuted
                    opacity: stripMonth.modelData.future ? 0.3 : stripMonth.available ? 1 : 0.5
                    font.pixelSize: Theme.font.smallSize
                    font.weight: stripMonth.selected ? Theme.font.emphasisWeight : Font.Normal
                }

                MouseArea {
                    id: stripMouse
                    anchors.fill: parent
                    enabled: stripMonth.available
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: stripMonth.modelData.month === 0
                               ? root.pickYear(stripMonth.modelData.year)
                               : root.pick(stripMonth.modelData.year, stripMonth.modelData.month)
                }

                ToolTip.visible: stripMouse.containsMouse && stripMonth.available
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: stripMonth.modelData.tip
            }
        }
    }

    component CardLabel: CaptionLabel {
        color: Theme.stats.textMuted
        font.pixelSize: Theme.stats.labelSize
    }

    component Highlight: Card {
        id: highlight

        property string icon
        property string label
        property string value
        property string sub

        Layout.fillWidth: true
        padding: Theme.stats.cardPadding * 0.8

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.margin

            Rectangle {
                Layout.preferredWidth: Theme.layout.toolbarHeight
                Layout.preferredHeight: Theme.layout.toolbarHeight
                Layout.alignment: Qt.AlignVCenter
                radius: Theme.stats.cardRadius * 0.7
                color: Theme.stats.track

                Icon {
                    anchors.centerIn: parent
                    size: Theme.iconButton.iconSize
                    color: Theme.stats.barActive
                    path: highlight.icon
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                CardLabel {
                    text: highlight.label
                }
                FadeLabel {
                    Layout.fillWidth: true
                    text: highlight.value
                    color: Theme.palette.text
                    pixelSize: Theme.font.headingSize
                    weight: Theme.font.emphasisWeight
                    elide: Text.ElideRight
                }
                FadeLabel {
                    Layout.fillWidth: true
                    text: highlight.sub
                    elide: Text.ElideRight
                }
            }
        }
    }

    component ChromeButton: Surface {
        id: chrome

        property string icon
        property real iconRotation: 0
        property string label
        property string tip

        signal clicked()

        implicitHeight: Theme.layout.toolbarHeight
        implicitWidth: chromeRow.implicitWidth + 2 * (chrome.label === "" ? Theme.layout.spacing : Theme.layout.spacing * 1.5)
        radius: Theme.iconButton.radius
        opacity: enabled ? 1 : 0.35
        fill: chromeMouse.containsMouse && enabled ? Theme.iconButton.hoverBackground : Theme.iconButton.background
        fillEnd: chromeMouse.containsMouse && enabled ? Theme.iconButton.hoverBackgroundEnd : Theme.iconButton.backgroundEnd
        direction: Theme.iconButton.gradientDirection
        border.color: Theme.iconButton.border
        border.width: Theme.iconButton.borderWidth
        scale: chromeMouse.pressed ? Theme.motion.pressScale : 1

        Behavior on fill { enabled: Motion.fades; MotionColor {} }
        Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
        Behavior on scale { enabled: Motion.on; MotionNumber {} }

        Row {
            id: chromeRow
            anchors.centerIn: parent
            spacing: Theme.layout.spacing

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                size: Theme.iconButton.iconSize
                color: Theme.iconButton.icon
                path: chrome.icon
                rotation: chrome.iconRotation
            }
            Label {
                visible: chrome.label !== ""
                anchors.verticalCenter: parent.verticalCenter
                text: chrome.label
                color: Theme.iconButton.icon
            }
        }

        MouseArea {
            id: chromeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chrome.clicked()
        }

        ToolTip.visible: chrome.tip !== "" && chromeMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: chrome.tip
    }

    Component {
        id: monthsCardComponent

        Card {
            id: monthsCard
            framed: false

            readonly property var columns: root.yearStats.columns ?? []
            readonly property int shownYear: root.yearStats.year ?? root.year
            readonly property real axisWidth: 32
            readonly property real chartHeight: dayChartArea.implicitHeight - chart.y - Theme.layout.spacing - monthNames.implicitHeight
            readonly property real columnWidth: (chart.width - Theme.stats.barGap * 11) / 12
            readonly property int currentMonthIndex: monthsCard.shownYear === root.today.getFullYear() ? root.today.getMonth() : -1
            property int hovered: -1
            property int tipIndex: -1
            onHoveredChanged: if (hovered >= 0) tipIndex = hovered

            RowLayout {
                Layout.fillWidth: true

                CardTitle { text: qsTr("Hours by month") }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: 2 * Theme.layout.spacing

                    Row {
                        spacing: Theme.layout.spacing / 2
                        Rectangle {
                            width: 10; height: 10
                            radius: Theme.stats.barRadius
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.stats.bar
                            opacity: Theme.stats.barOpacity
                        }
                        Muted { text: qsTr("Played") }
                    }
                    Row {
                        spacing: Theme.layout.spacing / 2
                        Rectangle {
                            width: 10; height: 10
                            radius: Theme.stats.barRadius
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.stats.mergedBar
                            opacity: Theme.stats.mergedOpacity
                        }
                        Muted { text: qsTr("From Steam while Lattis was closed") }
                    }
                    Muted { text: qsTr("Click a month to open it") }
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: dayChartArea.implicitHeight

                Muted { id: hoursProbe; visible: false; text: "0" }

                Item {
                    width: monthsCard.axisWidth
                    y: chart.y
                    height: monthsCard.chartHeight

                    TickLabels { scale: root.monthScale }
                }

                Item {
                    id: chart
                    x: monthsCard.axisWidth
                    y: hoursProbe.implicitHeight + Theme.layout.spacing
                    width: parent.width - x
                    height: monthsCard.chartHeight

                    GridLines { scale: root.monthScale }

                    Repeater {
                        // A fixed count, so stepping years keeps the columns and their bars move to the new hours.
                        model: 12

                        Item {
                            id: monthColumn
                            required property int index
                            readonly property var modelData: monthsCard.columns[index]
                                ?? ({ month: index + 1, minutes: 0, mergedMinutes: 0, pickable: false })
                            readonly property bool isCurrent: index === monthsCard.currentMonthIndex
                            readonly property bool active: monthsCard.hovered === index
                            property real liveShare: (modelData.minutes - modelData.mergedMinutes) / 60 / root.monthScale.top
                            property real mergedShare: modelData.mergedMinutes / 60 / root.monthScale.top
                            readonly property real barHeight: chart.height * (liveShare + mergedShare) * root.yearPresence

                            Behavior on liveShare { enabled: root.growing; MotionNumber { speed: "slow" } }
                            Behavior on mergedShare { enabled: root.growing; MotionNumber { speed: "slow" } }

                            x: index * (monthsCard.columnWidth + Theme.stats.barGap)
                            width: monthsCard.columnWidth
                            height: chart.height

                            Rectangle {
                                anchors.fill: parent
                                radius: Theme.stats.barRadius
                                color: Theme.palette.hoverOverlay
                                opacity: monthColumn.active ? 1 : 0
                                visible: opacity > 0

                                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                            }

                            Surface {
                                anchors.bottom: liveSegment.top
                                width: parent.width
                                height: chart.height * monthColumn.mergedShare * root.yearPresence
                                radius: Theme.stats.barRadius
                                bottomLeftRadius: monthColumn.modelData.minutes > monthColumn.modelData.mergedMinutes ? 0 : radius
                                bottomRightRadius: bottomLeftRadius
                                fill: Theme.stats.mergedBar
                                opacity: Theme.stats.mergedOpacity
                            }
                            Surface {
                                id: liveSegment
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: chart.height * monthColumn.liveShare * root.yearPresence
                                radius: Theme.stats.barRadius
                                topLeftRadius: monthColumn.modelData.mergedMinutes > 0 ? 0 : radius
                                topRightRadius: topLeftRadius
                                fill: monthColumn.isCurrent ? Qt.lighter(Theme.stats.barActive, Theme.stats.lightBrightness)
                                    : monthColumn.active ? Theme.stats.barActive : Theme.stats.bar
                                fillEnd: monthColumn.isCurrent ? Qt.lighter(Theme.stats.barActiveEnd, Theme.stats.lightBrightness)
                                       : monthColumn.active ? Theme.stats.barActiveEnd : Theme.stats.barEnd
                                opacity: monthColumn.isCurrent || monthColumn.active ? 1 : Theme.stats.barOpacity

                                Behavior on fill { enabled: Motion.fades; MotionColor {} }
                                Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
                                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                            }

                            FadeLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: parent.height - monthColumn.barHeight - height - Theme.layout.spacing / 2
                                horizontalAlignment: Text.AlignHCenter
                                text: monthColumn.modelData.minutes > 0 ? root.hoursText(monthColumn.modelData.minutes) : ""
                                color: Theme.palette.text
                            }

                            HoverHandler {
                                enabled: monthColumn.modelData.pickable
                                cursorShape: Qt.PointingHandCursor
                                onHoveredChanged: {
                                    if (hovered)
                                        monthsCard.hovered = monthColumn.index
                                    else if (monthsCard.hovered === monthColumn.index)
                                        monthsCard.hovered = -1
                                }
                            }
                            TapHandler {
                                enabled: monthColumn.modelData.pickable
                                onTapped: root.pick(root.year, monthColumn.modelData.month)
                            }
                        }
                    }

                    Rectangle {
                        id: monthTip
                        readonly property var column: monthsCard.tipIndex >= 0 ? monthsCard.columns[monthsCard.tipIndex] ?? null : null
                        readonly property int more: column === null ? 0 : column.games - column.top.length

                        opacity: monthsCard.hovered >= 0 ? 1 : 0
                        visible: column !== null && opacity > 0
                        z: 10
                        width: monthTipColumn.implicitWidth + 2 * Theme.layout.margin * 0.75
                        height: monthTipColumn.implicitHeight + 2 * Theme.layout.margin * 0.75
                        x: column === null ? 0 : Math.max(0, Math.min(chart.width - width,
                               monthsCard.tipIndex * (monthsCard.columnWidth + Theme.stats.barGap) + monthsCard.columnWidth / 2 - width / 2))

                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                        Behavior on x { enabled: Motion.on && monthTip.visible; MotionSpring {} }
                        y: -height - Theme.layout.spacing
                        radius: Theme.stats.barRadius * 2
                        color: Theme.tooltip.background
                        border.color: Theme.tooltip.border
                        border.width: 1

                        ColumnLayout {
                            id: monthTipColumn
                            anchors.centerIn: parent
                            spacing: Theme.layout.spacing * 0.75

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 190
                                Label {
                                    Layout.fillWidth: true
                                    text: monthTip.column === null ? ""
                                          : qsTr("%1 %2").arg(root.longMonth(monthTip.column.month)).arg(monthsCard.shownYear)
                                            + (monthsCard.tipIndex === monthsCard.currentMonthIndex ? qsTr(" · So far") : "")
                                    color: Theme.tooltip.text
                                    font.weight: Theme.font.emphasisWeight
                                    font.pixelSize: Theme.font.headingSize
                                }
                                Label {
                                    text: monthTip.column === null ? "" : monthTip.column.minutes > 0
                                          ? root.hoursText(monthTip.column.minutes) : qsTr("Not played")
                                    color: Theme.stats.textMuted
                                    font.pixelSize: Theme.font.headingSize
                                }
                            }
                            Repeater {
                                model: monthTip.column === null ? [] : monthTip.column.top

                                RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.title ?? modelData.id
                                        color: Theme.tooltip.text
                                        font.pixelSize: Theme.font.smallSize
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: root.hoursText(modelData.minutes)
                                        color: Theme.stats.textMuted
                                        font.pixelSize: Theme.font.smallSize
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 1
                                color: Theme.tooltip.border
                            }
                            Muted {
                                text: {
                                    if (monthTip.column === null)
                                        return ""
                                    const open = qsTr("Click to open %1").arg(root.longMonth(monthTip.column.month))
                                    return monthTip.more > 0 ? qsTr("+%1 more · %2").arg(monthTip.more).arg(open) : open
                                }
                            }
                        }
                    }
                }

                Item {
                    id: monthNames
                    x: monthsCard.axisWidth
                    y: chart.y + monthsCard.chartHeight + Theme.layout.spacing
                    width: chart.width
                    implicitHeight: hoursProbe.implicitHeight

                    Repeater {
                        model: 12

                        Muted {
                            required property int index
                            readonly property bool pickable: monthsCard.columns[index]?.pickable ?? true
                            x: index * (monthsCard.columnWidth + Theme.stats.barGap)
                            width: monthsCard.columnWidth
                            horizontalAlignment: Text.AlignHCenter
                            text: root.shortMonth(index + 1)
                            color: index === monthsCard.currentMonthIndex ? Qt.lighter(Theme.stats.barActive, Theme.stats.lightBrightness)
                                                                          : Theme.stats.textMuted
                            font.weight: index === monthsCard.currentMonthIndex ? Theme.font.emphasisWeight : Font.Normal
                            opacity: pickable ? 1 : 0.4

                            Behavior on color { enabled: Motion.fades; MotionColor { speed: "normal" } }
                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "normal" } }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: weekdayCardComponent

        Card {
            id: weekdayCard
            framed: false

            readonly property var totals: root.yearStats.weekdays ?? []
            readonly property real most: Math.max(60, ...totals)

            CardTitle { text: qsTr("Hours by weekday") }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 220
                spacing: Theme.layout.spacing * 1.5

                Repeater {
                    model: 7

                    ColumnLayout {
                        id: weekdayColumn
                        required property int index
                        readonly property int modelData: weekdayCard.totals[index] ?? 0
                        // QML's Locale numbers the days from Sunday = 0, as does dayName.
                        readonly property int weekday: (Qt.locale().firstDayOfWeek + index) % 7
                        readonly property bool weekend: weekday === 0 || weekday === 6
                        property real share: modelData / weekdayCard.most

                        Behavior on share { enabled: root.growing; MotionNumber { speed: "slow" } }

                        Layout.preferredWidth: 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.layout.spacing

                        Item {
                            id: weekdayBars
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            FadeLabel {
                                id: weekdayTime
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: weekdayBar.top
                                anchors.bottomMargin: Theme.layout.spacing
                                horizontalAlignment: Text.AlignHCenter
                                text: weekdayColumn.modelData > 0 ? root.hoursText(weekdayColumn.modelData) : "—"
                                color: Theme.palette.text
                            }
                            Surface {
                                id: weekdayBar
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(44, parent.width)
                                height: (parent.height - weekdayTime.height - Theme.layout.spacing) * weekdayColumn.share * root.yearPresence
                                radius: Theme.stats.barRadius
                                fill: Theme.stats.bar
                                fillEnd: Theme.stats.barEnd
                                opacity: Theme.stats.barOpacity
                            }
                        }
                        Muted {
                            Layout.alignment: Qt.AlignHCenter
                            text: Qt.locale().dayName(weekdayColumn.weekday, Locale.ShortFormat)
                            color: weekdayColumn.weekend ? Theme.stats.textMuted : Qt.alpha(Theme.stats.textMuted, 0.6)
                        }
                    }
                }
            }
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
            spacing: Theme.stats.sectionSpacing

            GridLayout {
                Layout.fillWidth: true
                columns: root.compact ? 1 : 3
                columnSpacing: 2 * Theme.stats.sectionSpacing
                rowSpacing: Theme.stats.sectionSpacing

                RowLayout {
                    spacing: 2 * Theme.stats.sectionSpacing

                    Label {
                        text: qsTr("Play statistics")
                        color: Theme.palette.text
                        font.pixelSize: Theme.stats.titleSize
                        font.weight: Theme.font.emphasisWeight
                    }

                    RowLayout {
                        visible: root.hasHistory
                        spacing: Theme.layout.spacing

                        SegmentedToggle {
                            Layout.rightMargin: Theme.layout.spacing
                            accessibleName: qsTr("Period")
                            choices: [{ id: "month", text: qsTr("Month") }, { id: "year", text: qsTr("Year") }]
                            current: root.mode
                            onPicked: id => root.setMode(id)
                        }
                        ChromeButton {
                            icon: Icons.chevronRight
                            iconRotation: 180
                            enabled: root.canPrevious
                            tip: root.yearMode ? qsTr("Previous year") : qsTr("Previous month")
                            onClicked: root.step(-1)
                        }
                        Rectangle {
                            id: titleButton
                            property real shownWidth: Math.max(160, titleText.implicitWidth + titleIcon.size + 3 * Theme.layout.spacing)
                            Behavior on shownWidth { enabled: root.growing; MotionNumber { speed: "normal" } }

                            Layout.preferredWidth: shownWidth
                            implicitHeight: Theme.layout.toolbarHeight
                            radius: Theme.iconButton.radius
                            color: titleMouse.containsMouse ? Theme.iconButton.hoverBackground : Qt.alpha(Theme.iconButton.hoverBackground, 0)

                            Behavior on color { enabled: Motion.fades; MotionColor {} }

                            FadeLabel {
                                id: titleText
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.layout.spacing
                                anchors.right: titleIcon.left
                                anchors.rightMargin: Theme.layout.spacing
                                anchors.verticalCenter: parent.verticalCenter
                                // A longer old period would spill over the arrows while the button narrows.
                                clip: true
                                horizontalAlignment: Text.AlignHCenter
                                text: root.yearMode ? String(root.year) : qsTr("%1 %2").arg(root.longMonth(root.month)).arg(root.year)
                                color: Theme.palette.text
                                pixelSize: Theme.stats.monthSize
                                weight: Theme.font.emphasisWeight
                            }
                            Icon {
                                id: titleIcon
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.layout.spacing
                                anchors.verticalCenter: parent.verticalCenter
                                size: Theme.iconButton.iconSize
                                color: Theme.stats.textMuted
                                path: Icons.chevronRight
                                rotation: 90
                            }
                            MouseArea {
                                id: titleMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.openPicker()
                            }
                            Accessible.role: Accessible.Button
                            Accessible.name: root.yearMode ? qsTr("Pick a year") : qsTr("Pick a month")

                            Loader {
                                id: pickerLoader
                                active: false
                                source: "StatsPeriodPicker.qml"
                                onLoaded: {
                                    item.parent = titleButton
                                    item.y = titleButton.height + Theme.layout.spacing
                                }
                            }
                            Connections {
                                target: pickerLoader.item
                                function onMonthPicked(y, m) { root.pick(y, m) }
                                function onYearPicked(y) { root.pickYear(y) }
                                function onYearViewRequested(y) { root.pickYear(y) }
                            }
                        }
                        ChromeButton {
                            icon: Icons.chevronRight
                            enabled: root.canNext
                            tip: root.yearMode ? qsTr("Next year") : qsTr("Next month")
                            onClicked: root.step(1)
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    visible: !root.compact
                }

                Item {
                    visible: root.hasHistory
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    implicitWidth: Math.max(monthStrip.visible ? monthStrip.implicitWidth : 0,
                                            yearStrip.visible ? yearStrip.implicitWidth : 0)
                    implicitHeight: Math.max(monthStrip.implicitHeight, yearStrip.implicitHeight)

                    Strip {
                        id: monthStrip
                        anchors.right: parent.right
                        items: root.monthStripItems
                        presence: root.monthPresence
                    }
                    Strip {
                        id: yearStrip
                        anchors.right: parent.right
                        items: root.yearStripItems
                        yearly: true
                        presence: root.yearPresence
                    }
                }
            }

            Label {
                visible: root.loaded && !root.hasHistory
                Layout.fillWidth: true
                Layout.topMargin: 4 * Theme.stats.sectionSpacing
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Sessions appear here once a game is played.")
                color: Theme.stats.textMuted
            }

            ColumnLayout {
                visible: root.ready
                Layout.fillWidth: true
                spacing: Theme.stats.sectionSpacing

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 2 : 4
                    columnSpacing: Theme.stats.sectionSpacing
                    rowSpacing: Theme.stats.sectionSpacing
                    uniformCellWidths: true

                    Repeater {
                        // A fixed count, so a new period keeps the tiles and their numbers count to the new values.
                        model: 4

                        Card {
                            id: tile
                            required property int index
                            readonly property var modelData: root.tiles[index]
                                ?? ({ label: "", amount: 0, kind: "count", color: Theme.palette.text, icon: "", sub: "" })
                            property real amount: modelData.amount
                            Behavior on amount { enabled: root.growing; MotionNumber { speed: "slow" } }

                            Layout.fillWidth: true
                            horizontalPadding: Theme.stats.tilePaddingX
                            watermark: modelData.icon

                            CardLabel {
                                text: tile.modelData.label
                            }
                            Label {
                                text: root.tileValue(tile.modelData.kind, tile.amount)
                                color: tile.modelData.color
                                font.pixelSize: Theme.stats.tileValueSize
                                font.weight: Font.Bold

                                Behavior on color { enabled: Motion.fades; MotionColor { speed: "normal" } }
                            }
                            FadeLabel {
                                text: tile.modelData.sub
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    implicitHeight: dayCard.implicitHeight

                    CardFrame { anchors.fill: parent }

                    Card {
                        id: dayCard
                        framed: false
                        width: parent.width
                        opacity: root.monthPresence
                        visible: opacity > 0

                        readonly property var days: root.monthStats.days ?? []
                        readonly property real axisWidth: 32
                        property real dayCount: days.length
                        Behavior on dayCount { enabled: root.shifting; MotionNumber { speed: "slow" } }
                        readonly property real columnWidth: dayCount > 0
                            ? (chart.width - Theme.stats.barGap * (dayCount - 1)) / dayCount : 0
                        property int hovered: -1
                        property int tipIndex: -1
                        onHoveredChanged: if (hovered >= 0) tipIndex = hovered

                        RowLayout {
                            Layout.fillWidth: true

                            CardTitle { text: qsTr("Hours by day") }
                            Item { Layout.fillWidth: true }
                            Row {
                                spacing: 2 * Theme.layout.spacing

                                Row {
                                    spacing: Theme.layout.spacing / 2
                                    Rectangle {
                                        width: 10; height: 10
                                        radius: Theme.stats.barRadius
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.stats.bar
                                        opacity: Theme.stats.barOpacity
                                    }
                                    Muted { text: qsTr("Played") }
                                }
                                Row {
                                    spacing: Theme.layout.spacing / 2
                                    Rectangle {
                                        width: 10; height: 10
                                        radius: Theme.stats.barRadius
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.stats.mergedBar
                                        opacity: Theme.stats.mergedOpacity
                                    }
                                    Muted { text: qsTr("From Steam while Lattis was closed") }
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            id: dayChartArea
                            implicitHeight: Theme.stats.chartHeight + axis.height + weekLines.height + 2 * Theme.layout.spacing

                            Item {
                                id: yAxis
                                width: dayCard.axisWidth
                                height: Theme.stats.chartHeight

                                TickLabels { scale: root.dayScale }
                            }

                            Item {
                                id: chart
                                x: dayCard.axisWidth
                                width: parent.width - x
                                height: Theme.stats.chartHeight

                                GridLines { scale: root.dayScale }

                                Repeater {
                                    // A fixed count, so stepping months keeps the columns and their bars move to the new hours.
                                    model: 31

                                    Item {
                                        id: dayColumn
                                        required property int index
                                        readonly property var modelData: dayCard.days[index]
                                            ?? ({ day: index + 1, minutes: 0, mergedMinutes: 0 })
                                        readonly property bool future: root.shownCurrentMonth && modelData.day > root.today.getDate()
                                        readonly property bool isToday: root.shownCurrentMonth && modelData.day === root.today.getDate()
                                        readonly property bool active: dayCard.hovered === index
                                        readonly property real liveMinutes: modelData.minutes - modelData.mergedMinutes
                                        property real liveShare: liveMinutes / 60 / root.dayScale.top
                                        property real mergedShare: modelData.mergedMinutes / 60 / root.dayScale.top

                                        Behavior on liveShare { enabled: root.growing; MotionNumber { speed: "slow" } }
                                        Behavior on mergedShare { enabled: root.growing; MotionNumber { speed: "slow" } }

                                        readonly property bool used: index < dayCard.days.length
                                        opacity: used ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: root.shifting; MotionNumber { speed: "normal" } }
                                        x: index * (dayCard.columnWidth + Theme.stats.barGap)
                                        width: dayCard.columnWidth
                                        height: chart.height

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: Theme.stats.barRadius
                                            color: Theme.palette.hoverOverlay
                                            opacity: dayColumn.active && !dayColumn.future ? 1 : 0
                                            visible: opacity > 0

                                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                                        }

                                        Surface {
                                            id: mergedSegment
                                            anchors.bottom: liveSegment.top
                                            width: parent.width
                                            height: chart.height * dayColumn.mergedShare * root.monthPresence
                                            radius: Theme.stats.barRadius
                                            bottomLeftRadius: dayColumn.liveMinutes > 0 ? 0 : radius
                                            bottomRightRadius: bottomLeftRadius
                                            fill: Theme.stats.mergedBar
                                            opacity: Theme.stats.mergedOpacity
                                        }
                                        Surface {
                                            id: liveSegment
                                            anchors.bottom: parent.bottom
                                            width: parent.width
                                            height: chart.height * dayColumn.liveShare * root.monthPresence
                                            radius: Theme.stats.barRadius
                                            topLeftRadius: dayColumn.modelData.mergedMinutes > 0 ? 0 : radius
                                            topRightRadius: topLeftRadius
                                            fill: dayColumn.isToday ? Qt.lighter(Theme.stats.barActive, Theme.stats.lightBrightness)
                                                : dayColumn.active ? Theme.stats.barActive : Theme.stats.bar
                                            fillEnd: dayColumn.isToday ? Qt.lighter(Theme.stats.barActiveEnd, Theme.stats.lightBrightness)
                                                   : dayColumn.active ? Theme.stats.barActiveEnd : Theme.stats.barEnd
                                            opacity: dayColumn.isToday || dayColumn.active ? 1 : Theme.stats.barOpacity

                                            Behavior on fill { enabled: Motion.fades; MotionColor {} }
                                            Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }
                                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                                        }

                                        HoverHandler {
                                            enabled: dayColumn.used && !dayColumn.future
                                            onHoveredChanged: {
                                                if (hovered)
                                                    dayCard.hovered = dayColumn.index
                                                else if (dayCard.hovered === dayColumn.index)
                                                    dayCard.hovered = -1
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    id: dayTip
                                    readonly property var day: dayCard.tipIndex >= 0 ? dayCard.days[dayCard.tipIndex] ?? null : null

                                    opacity: dayCard.hovered >= 0 ? 1 : 0
                                    visible: day !== null && opacity > 0
                                    z: 10
                                    width: tipColumn.implicitWidth + 2 * Theme.layout.margin * 0.75
                                    height: tipColumn.implicitHeight + 2 * Theme.layout.margin * 0.75
                                    x: day === null ? 0 : Math.max(0, Math.min(chart.width - width,
                                           dayCard.tipIndex * (dayCard.columnWidth + Theme.stats.barGap) + dayCard.columnWidth / 2 - width / 2))

                                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                                    Behavior on x { enabled: Motion.on && dayTip.visible; MotionSpring {} }
                                    y: -height - Theme.layout.spacing
                                    radius: Theme.stats.barRadius * 2
                                    color: Theme.tooltip.background
                                    border.color: Theme.tooltip.border
                                    border.width: 1

                                    ColumnLayout {
                                        id: tipColumn
                                        anchors.centerIn: parent
                                        spacing: Theme.layout.spacing * 0.75

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 190
                                            Label {
                                                id: tipDate
                                                Layout.fillWidth: true
                                                text: dayTip.day === null ? "" : root.dateText(dayTip.day.day)
                                                      + (root.shownCurrentMonth && dayTip.day.day === root.today.getDate() ? qsTr(" · Today") : "")
                                                color: Theme.tooltip.text
                                                font.weight: Theme.font.emphasisWeight
                                                font.pixelSize: Theme.font.headingSize
                                            }
                                            Label {
                                                id: tipTotal
                                                text: dayTip.day === null ? "" : dayTip.day.minutes > 0
                                                      ? root.hoursText(dayTip.day.minutes) : qsTr("Not played")
                                                color: Theme.stats.textMuted
                                                font.pixelSize: Theme.font.headingSize
                                            }
                                        }
                                        Repeater {
                                            model: dayTip.day === null ? [] : dayTip.day.games

                                            RowLayout {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: modelData.title
                                                    color: Theme.tooltip.text
                                                    font.pixelSize: Theme.font.smallSize
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    text: root.hoursText(modelData.minutes)
                                                    color: Theme.stats.textMuted
                                                    font.pixelSize: Theme.font.smallSize
                                                }
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true
                                            implicitHeight: 1
                                            color: Theme.tooltip.border
                                            visible: dayTip.day !== null && dayTip.day.mergedMinutes > 0
                                        }
                                        Muted {
                                            visible: dayTip.day !== null && dayTip.day.mergedMinutes > 0
                                            text: qsTr("Includes play from Steam while Lattis was closed")
                                        }
                                    }
                                }
                            }

                            Item {
                                id: axis
                                x: dayCard.axisWidth
                                y: Theme.stats.chartHeight + Theme.layout.spacing
                                width: chart.width
                                height: numberProbe.implicitHeight + namesProbe.implicitHeight

                                Muted { id: numberProbe; visible: false; text: "0" }
                                Muted { id: namesProbe; visible: false; text: "0"; font.pixelSize: Theme.font.smallSize - 1 }

                                Repeater {
                                    // Kept with the columns; the numbers stay and slide, the weekday names change.
                                    model: 31

                                    Column {
                                        id: dayLabel
                                        required property int index
                                        readonly property int day: index + 1
                                        readonly property bool used: index < dayCard.days.length
                                        readonly property bool future: root.shownCurrentMonth && day > root.today.getDate()
                                        readonly property bool isToday: root.shownCurrentMonth && day === root.today.getDate()
                                        readonly property int weekday: new Date(root.shownMonthYear, root.shownMonth - 1, day).getDay()
                                        readonly property bool weekend: weekday === 0 || weekday === 6

                                        x: index * (dayCard.columnWidth + Theme.stats.barGap)
                                        width: dayCard.columnWidth
                                        opacity: !used ? 0 : future ? 0.4 : 1
                                        visible: opacity > 0

                                        Behavior on opacity { enabled: root.shifting; MotionNumber { speed: "normal" } }

                                        Muted {
                                            width: parent.width
                                            horizontalAlignment: Text.AlignHCenter
                                            text: dayLabel.day
                                            color: dayLabel.isToday ? Qt.lighter(Theme.stats.barActive, Theme.stats.lightBrightness)
                                                                    : Theme.stats.textMuted
                                            font.weight: dayLabel.isToday ? Theme.font.emphasisWeight : Font.Normal

                                            Behavior on color { enabled: Motion.fades; MotionColor { speed: "normal" } }
                                        }
                                        FadeLabel {
                                            width: parent.width
                                            horizontalAlignment: Text.AlignHCenter
                                            pixelSize: Theme.font.smallSize - 1
                                            text: {
                                                const name = Qt.locale().dayName(dayLabel.weekday, Locale.ShortFormat)
                                                return dayCard.columnWidth < 30 ? name.charAt(0) : name
                                            }
                                            color: dayLabel.weekend ? Theme.stats.textMuted : Qt.alpha(Theme.stats.textMuted, 0.6)

                                            Behavior on color { enabled: root.fading; MotionColor { speed: "normal" } }
                                        }
                                    }
                                }
                            }

                            Item {
                                id: weekLines
                                x: dayCard.axisWidth
                                y: axis.y + axis.height + Theme.layout.spacing
                                width: chart.width
                                height: 2 + weekLabelHeight.implicitHeight + Theme.layout.spacing / 2

                                Muted { id: weekLabelHeight; visible: false; text: "Week" }

                                Repeater {
                                    // The most weeks a month touches; each line slides and stretches to its new days.
                                    model: 6

                                    Item {
                                        id: weekLine
                                        required property int index
                                        readonly property var week: (root.monthStats.weeks ?? [])[index] ?? null
                                        property real firstDay: 1
                                        property real lastDay: 1

                                        function follow() {
                                            if (week === null)
                                                return
                                            firstDay = week.firstDay
                                            lastDay = week.lastDay
                                        }
                                        onWeekChanged: follow()
                                        Component.onCompleted: follow()

                                        Behavior on firstDay { enabled: root.shifting; MotionNumber { speed: "slow" } }
                                        Behavior on lastDay { enabled: root.shifting; MotionNumber { speed: "slow" } }

                                        x: (firstDay - 1) * (dayCard.columnWidth + Theme.stats.barGap)
                                        width: (lastDay - firstDay + 1) * (dayCard.columnWidth + Theme.stats.barGap) - Theme.stats.barGap
                                        height: weekLines.height
                                        opacity: week !== null ? 1 : 0
                                        visible: opacity > 0

                                        Behavior on opacity { enabled: root.shifting; MotionNumber { speed: "normal" } }

                                        Rectangle {
                                            x: 2
                                            width: parent.width - 4
                                            height: 2
                                            radius: 1
                                            color: Theme.palette.border
                                        }
                                        FadeLabel {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: 2 + Theme.layout.spacing / 2
                                            horizontalAlignment: Text.AlignHCenter
                                            text: weekLine.week !== null && weekLine.week.lastDay - weekLine.week.firstDay >= 2
                                                  ? qsTr("Week %1").arg(weekLine.week.number) : ""
                                            opacity: Theme.stats.weekLabelOpacity
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Loader {
                        width: parent.width
                        opacity: root.yearPresence
                        visible: opacity > 0
                        active: root.yearUsed
                        sourceComponent: monthsCardComponent
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 3
                    columnSpacing: Theme.stats.sectionSpacing
                    rowSpacing: Theme.stats.sectionSpacing
                    uniformCellWidths: true

                    Card {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        CardTitle { text: qsTr("Hours by platform") }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Repeater {
                                model: root.platformSlots

                                ListSlot {
                                    id: platformRow
                                    entry: (root.stats.platforms ?? [])[index] ?? null

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: Theme.layout.spacing * 1.25

                                        Item {
                                            Layout.alignment: Qt.AlignVCenter
                                            implicitWidth: Theme.stats.rowHeight * 0.8
                                            implicitHeight: implicitWidth

                                            Turns { id: platformIcon; value: platformRow.shown.icon ?? "" }

                                            Icon {
                                                size: parent.width
                                                color: Theme.palette.text
                                                path: Icons.forPlatform(platformIcon.first ?? "")
                                                opacity: platformIcon.firstOpacity
                                                visible: opacity > 0
                                            }
                                            Icon {
                                                size: parent.width
                                                color: Theme.palette.text
                                                path: Icons.forPlatform(platformIcon.second ?? "")
                                                opacity: platformIcon.secondOpacity
                                                visible: opacity > 0
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            RowLayout {
                                                Layout.fillWidth: true
                                                FadeLabel {
                                                    Layout.fillWidth: true
                                                    text: platformRow.shown.name ?? platformRow.shown.id ?? ""
                                                    color: Theme.palette.text
                                                    pixelSize: Theme.font.size
                                                    elide: Text.ElideRight
                                                }
                                                Share { minutes: platformRow.shown.minutes ?? 0 }
                                            }
                                            ShareBar { minutes: platformRow.entry?.minutes ?? 0 }
                                        }
                                    }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        implicitWidth: weekCard.implicitWidth
                        implicitHeight: weekCard.implicitHeight

                        CardFrame { anchors.fill: parent }

                        Card {
                            id: weekCard
                            framed: false
                            anchors.fill: parent
                            opacity: root.monthPresence
                            visible: opacity > 0

                            CardTitle { text: qsTr("Hours by week") }

                            Item {
                                id: weekChart
                                readonly property var weeks: root.monthStats.weeks ?? []
                                readonly property real most: Math.max(60, ...weeks.map(w => w.minutes))
                                readonly property real spacing: Theme.layout.spacing * 1.5
                                property real count: weeks.length
                                Behavior on count { enabled: root.shifting; MotionNumber { speed: "slow" } }
                                readonly property real columnWidth: count > 0 ? (width - spacing * (count - 1)) / count : 0
                                readonly property bool withMonth: weekLabelMetrics.advanceWidth
                                    <= (width - spacing * (weeks.length - 1)) / Math.max(1, weeks.length)
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 220

                                TextMetrics {
                                    id: weekLabelMetrics
                                    font.pixelSize: Theme.font.smallSize
                                    text: weekChart.weeks.map(w => root.rangeText(w.firstDay, w.lastDay))
                                                         .reduce((a, b) => b.length > a.length ? b : a, "")
                                }

                                Repeater {
                                    // The most weeks a month touches, so stepping months keeps the columns and their bars move.
                                    model: 6

                                    ColumnLayout {
                                        id: weekColumn
                                        required property int index
                                        readonly property var modelData: weekChart.weeks[index]
                                            ?? ({ firstDay: 1, lastDay: 1, minutes: 0, mergedMinutes: 0 })
                                        property real share: modelData.minutes / weekChart.most
                                        property real mergedShare: modelData.mergedMinutes / weekChart.most

                                        Behavior on share { enabled: root.growing; MotionNumber { speed: "slow" } }
                                        Behavior on mergedShare { enabled: root.growing; MotionNumber { speed: "slow" } }

                                        x: index * (weekChart.columnWidth + weekChart.spacing)
                                        width: weekChart.columnWidth
                                        height: weekChart.height
                                        opacity: index < weekChart.weeks.length ? 1 : 0
                                        visible: opacity > 0
                                        spacing: Theme.layout.spacing

                                        Behavior on opacity { enabled: root.shifting; MotionNumber { speed: "normal" } }

                                        Item {
                                            id: weekBars
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            readonly property real barHeight:
                                                (height - weekTime.height - Theme.layout.spacing) * weekColumn.share * root.monthPresence
                                            readonly property real mergedHeight:
                                                (height - weekTime.height - Theme.layout.spacing) * weekColumn.mergedShare * root.monthPresence

                                            FadeLabel {
                                                id: weekTime
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                anchors.bottom: weekBar.top
                                                anchors.bottomMargin: Theme.layout.spacing
                                                horizontalAlignment: Text.AlignHCenter
                                                text: weekColumn.modelData.minutes > 0 ? root.hoursText(weekColumn.modelData.minutes) : "—"
                                                color: Theme.palette.text
                                            }
                                            Item {
                                                id: weekBar
                                                anchors.bottom: parent.bottom
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                width: Math.min(44, parent.width)
                                                height: weekBars.barHeight

                                                Surface {
                                                    anchors.bottom: parent.bottom
                                                    width: parent.width
                                                    height: parent.height - weekBars.mergedHeight
                                                    radius: Theme.stats.barRadius
                                                    topLeftRadius: weekBars.mergedHeight > 0 ? 0 : radius
                                                    topRightRadius: topLeftRadius
                                                    fill: Theme.stats.bar
                                                    fillEnd: Theme.stats.barEnd
                                                    opacity: Theme.stats.barOpacity
                                                }
                                                Surface {
                                                    anchors.top: parent.top
                                                    width: parent.width
                                                    height: weekBars.mergedHeight
                                                    radius: Theme.stats.barRadius
                                                    bottomLeftRadius: parent.height > weekBars.mergedHeight ? 0 : radius
                                                    bottomRightRadius: bottomLeftRadius
                                                    fill: Theme.stats.mergedBar
                                                    opacity: Theme.stats.mergedOpacity
                                                }
                                            }
                                        }
                                        FadeLabel {
                                            readonly property int first: weekColumn.modelData.firstDay
                                            readonly property int last: weekColumn.modelData.lastDay
                                            Layout.alignment: Qt.AlignHCenter
                                            horizontalAlignment: Text.AlignHCenter
                                            text: weekChart.withMonth ? root.rangeText(first, last)
                                                  : first === last ? String(first) : qsTr("%1–%2").arg(first).arg(last)
                                        }
                                    }
                                }
                            }
                        }

                        Loader {
                            anchors.fill: parent
                            opacity: root.yearPresence
                            visible: opacity > 0
                            active: root.yearUsed
                            sourceComponent: weekdayCardComponent
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        horizontalPadding: padding - Theme.layout.spacing

                        CardTitle { Layout.leftMargin: Theme.layout.spacing; text: qsTr("Most played") }

                        TextMetrics {
                            id: rankMetrics
                            font.pixelSize: Theme.font.smallSize
                            text: String((root.stats.top ?? []).length)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Repeater {
                                // As many as the list can hold, so the rows are kept from period to period.
                                model: Math.max(5, (root.stats.top ?? []).length)

                                ListSlot {
                                    id: topSlot
                                    entry: (root.stats.top ?? [])[index] ?? null

                                    Rectangle {
                                        id: topRow
                                        anchors.fill: parent
                                        radius: Theme.stats.barRadius
                                        color: topMouse.containsMouse ? Theme.palette.hoverOverlay : Qt.alpha(Theme.palette.hoverOverlay, 0)

                                        Behavior on color { enabled: Motion.fades; MotionColor {} }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: Theme.layout.spacing
                                            anchors.rightMargin: Theme.layout.spacing
                                            spacing: Theme.layout.spacing * 1.25

                                            Muted {
                                                Layout.preferredWidth: rankMetrics.advanceWidth
                                                horizontalAlignment: Text.AlignLeft
                                                text: topSlot.index + 1
                                            }
                                            Item {
                                                implicitWidth: Theme.stats.rowHeight * 0.65
                                                implicitHeight: implicitWidth

                                                Turns { id: topIcon; value: topSlot.shown; keyField: "id" }

                                                GameIcon {
                                                    size: parent.width
                                                    source: topIcon.first?.iconUrl ?? ""
                                                    title: topIcon.first?.title ?? ""
                                                    hue: topIcon.first?.hue ?? 0
                                                    opacity: topIcon.firstOpacity
                                                    visible: opacity > 0 && topIcon.first?.id !== undefined
                                                }
                                                GameIcon {
                                                    size: parent.width
                                                    source: topIcon.second?.iconUrl ?? ""
                                                    title: topIcon.second?.title ?? ""
                                                    hue: topIcon.second?.hue ?? 0
                                                    opacity: topIcon.secondOpacity
                                                    visible: opacity > 0 && topIcon.second?.id !== undefined
                                                }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    FadeLabel {
                                                        Layout.fillWidth: true
                                                        text: topSlot.shown.title ?? topSlot.shown.id ?? ""
                                                        color: Theme.palette.text
                                                        pixelSize: Theme.font.size
                                                        elide: Text.ElideRight
                                                    }
                                                    Share { minutes: topSlot.shown.minutes ?? 0 }
                                                }
                                                ShareBar { minutes: topSlot.entry?.minutes ?? 0 }
                                            }
                                        }

                                        MouseArea {
                                            id: topMouse
                                            anchors.fill: parent
                                            enabled: topSlot.entry !== null
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.gameRequested(topSlot.shown.id)
                                        }

                                        ToolTip.visible: topMouse.containsMouse
                                        ToolTip.delay: Theme.tooltip.delay
                                        ToolTip.text: qsTr("Open %1").arg(topSlot.shown.title ?? "")
                                    }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 3
                    columnSpacing: Theme.stats.sectionSpacing
                    rowSpacing: Theme.stats.sectionSpacing
                    uniformCellWidths: true

                    Highlight {
                        readonly property var longest: root.stats.longest ?? ({})
                        visible: root.stats.longest !== undefined && longest.minutes > 0
                        icon: Icons.timerLine
                        label: qsTr("Longest session")
                        value: visible ? qsTr("%1 · %2").arg(root.lengthText(longest.minutes)).arg(longest.title ?? "") : ""
                        sub: visible ? new Date(longest.endMs).toLocaleDateString(Qt.locale(), General.shortDateFormat) : ""
                    }
                    Highlight {
                        visible: (root.stats.streakDays ?? 0) > 0
                        icon: Icons.history
                        label: qsTr("Longest streak")
                        value: qsTr("%n day(s)", "", root.stats.streakDays ?? 0)
                        sub: !visible ? "" : root.yearShape ? root.streakText()
                             : root.rangeText(root.stats.streakFirstDay, root.stats.streakFirstDay + root.stats.streakDays - 1)
                    }
                    Highlight {
                        readonly property int sessions: root.stats.averageSessions ?? 0
                        visible: sessions > 0
                        icon: Icons.hourglass
                        label: qsTr("Average session")
                        value: root.lengthText(root.stats.averageMinutes ?? 0)
                        sub: qsTr("Across %n session(s)", "", sessions)
                    }
                }
            }
        }
    }
}
