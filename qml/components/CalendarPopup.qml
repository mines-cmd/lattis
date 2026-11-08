import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

MotionPopup {
    id: root

    // A Date (any time of day); anything else opens on the month of maximumDate, or today's.
    property var date: null
    // Days after this one can't be picked and the month after its month can't be shown.
    property var maximumDate: null

    signal picked(var date)

    readonly property var today: new Date()
    readonly property var limit: maximumDate instanceof Date ? maximumDate : null
    property int shownYear: today.getFullYear()
    property int shownMonth: today.getMonth()
    // The day the keyboard is on; the field's own date until an arrow moves it.
    property var cursor: today

    function sameDay(a, b) {
        return a && b && a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth()
               && a.getDate() === b.getDate()
    }

    function allowed(day) {
        return !limit || day.getTime() <= new Date(limit.getFullYear(), limit.getMonth(), limit.getDate(), 23, 59, 59).getTime()
    }

    function clamped(day) {
        if (allowed(day))
            return day
        return new Date(limit.getFullYear(), limit.getMonth(), limit.getDate())
    }

    function showMonthOf(day) {
        shownYear = day.getFullYear()
        shownMonth = day.getMonth()
    }

    function moveCursor(days, months) {
        const from = cursor
        let year = from.getFullYear()
        let month = from.getMonth() + months
        let day = from.getDate()
        if (months !== 0) {
            const last = new Date(year, month + 1, 0).getDate()
            day = Math.min(day, last)
        }
        const target = clamped(new Date(year, month, day + days))
        cursor = target
        showMonthOf(target)
    }

    function shiftMonth(step) {
        const first = new Date(shownYear, shownMonth + step, 1)
        if (step > 0 && limit && first.getTime() > limit.getTime())
            return
        showMonthOf(first)
    }

    readonly property bool atLimit: limit !== null && (shownYear > limit.getFullYear()
            || (shownYear === limit.getFullYear() && shownMonth >= limit.getMonth()))

    function pick(day) {
        if (!allowed(day))
            return
        picked(new Date(day.getFullYear(), day.getMonth(), day.getDate()))
        close()
    }

    onAboutToShow: {
        const start = date instanceof Date ? date : clamped(new Date())
        cursor = new Date(start.getFullYear(), start.getMonth(), start.getDate())
        showMonthOf(cursor)
        const field = parent
        if (field) {
            const below = field.mapToItem(null, 0, field.height).y
            const room = Overlay.overlay ? Overlay.overlay.height - below : implicitHeight
            y = room >= implicitHeight + Theme.menu.buttonGap ? field.height + Theme.menu.buttonGap
                                                              : -implicitHeight - Theme.menu.buttonGap
        }
    }

    padding: Theme.calendar.padding
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    background: Item {
        RectangularShadow {
            visible: Theme.menu.popupShadowSize > 0
            anchors.fill: parent
            offset.y: Theme.menu.popupShadowSize / 6
            radius: Theme.calendar.radius
            blur: Theme.menu.popupShadowSize
            color: Theme.menu.popupShadowColor
        }
        Rectangle {
            anchors.fill: parent
            color: Theme.calendar.background
            border.color: Theme.calendar.border
            border.width: Theme.calendar.borderWidth
            radius: Theme.calendar.radius
        }
    }

    contentItem: Item {
        id: content

        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight
        focus: true

        Keys.onPressed: event => {
            switch (event.key) {
            case Qt.Key_Left: root.moveCursor(-1, 0); break
            case Qt.Key_Right: root.moveCursor(1, 0); break
            case Qt.Key_Up: root.moveCursor(-7, 0); break
            case Qt.Key_Down: root.moveCursor(7, 0); break
            case Qt.Key_PageUp: root.moveCursor(0, -1); break
            case Qt.Key_PageDown: root.moveCursor(0, 1); break
            case Qt.Key_Home: {
                const now = root.clamped(new Date())
                root.cursor = now
                root.showMonthOf(now)
                break
            }
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Space:
                root.pick(root.cursor)
                break
            default:
                return
            }
            event.accepted = true
        }

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                CalendarArrow {
                    icon: Icons.chevronLeft
                    onClicked: root.shiftMonth(-1)
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root.locale.standaloneMonthName(root.shownMonth) + " " + root.shownYear
                    color: Theme.calendar.headerColor
                    font.weight: Theme.font.strongWeight
                }
                CalendarArrow {
                    icon: Icons.chevronRight
                    enabled: !root.atLimit
                    onClicked: root.shiftMonth(1)
                }
            }

            DayOfWeekRow {
                Layout.fillWidth: true
                locale: root.locale
                spacing: 0
                topPadding: 0
                bottomPadding: 0
                delegate: Label {
                    required property var model

                    width: Theme.calendar.cellSize
                    height: Theme.calendar.cellSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: model.narrowName
                    color: Theme.calendar.weekdayColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                }
            }

            MonthGrid {
                id: grid
                Layout.fillWidth: true
                locale: root.locale
                year: root.shownYear
                month: root.shownMonth
                spacing: 0
                topPadding: 0
                bottomPadding: 0

                delegate: Item {
                    id: cell

                    required property var model

                    readonly property var day: new Date(model.year, model.month, model.day)
                    readonly property bool inMonth: model.month === grid.month
                    readonly property bool pickable: root.allowed(day)
                    readonly property bool isPicked: root.sameDay(day, root.date)
                    readonly property bool onCursor: root.sameDay(day, root.cursor)

                    implicitWidth: Theme.calendar.cellSize
                    implicitHeight: Theme.calendar.cellSize

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Theme.calendar.todayRingWidth > 0 ? 1 : 0
                        radius: Theme.calendar.cellRadius
                        color: cell.isPicked ? Theme.calendar.pickedBackground
                               : (cellHover.hovered || cell.onCursor) && cell.pickable ? Theme.calendar.hoverBackground
                               : "transparent"
                        border.color: Theme.calendar.todayRing
                        border.width: cell.model.today ? Theme.calendar.todayRingWidth : 0

                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: cell.model.day
                        color: cell.isPicked ? Theme.calendar.pickedText
                               : !cell.pickable ? Theme.calendar.disabledColor
                               : cell.inMonth ? Theme.calendar.dayColor : Theme.calendar.outsideColor
                        opacity: cell.pickable ? 1 : Theme.calendar.disabledOpacity
                    }

                    HoverHandler {
                        id: cellHover
                        enabled: cell.pickable
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        enabled: cell.pickable
                        onTapped: root.pick(cell.day)
                    }
                }
            }
        }
    }

    component CalendarArrow: Item {
        id: arrow

        property string icon
        signal clicked()

        implicitWidth: Theme.calendar.cellSize
        implicitHeight: Theme.calendar.cellSize

        Rectangle {
            anchors.fill: parent
            radius: Theme.calendar.cellRadius
            color: arrowHover.hovered && arrow.enabled ? Theme.calendar.hoverBackground : "transparent"
            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }
        Icon {
            anchors.centerIn: parent
            path: arrow.icon
            size: Theme.menu.iconSize
            color: arrow.enabled ? Theme.calendar.headerColor : Theme.calendar.arrowDisabledColor
            opacity: arrow.enabled ? 1 : Theme.calendar.disabledOpacity
        }
        HoverHandler { id: arrowHover; enabled: arrow.enabled; cursorShape: Qt.PointingHandCursor }
        TapHandler { enabled: arrow.enabled; onTapped: arrow.clicked() }
    }
}
