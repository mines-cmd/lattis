import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string mode: "month"
    property int pickedYear
    property int pickedMonth
    property int firstYear
    property int firstMonth
    property int todayYear
    property int todayMonth
    property var history: []
    property bool allowYearView: false

    signal monthPicked(int year, int month)
    signal yearPicked(int year)
    signal yearViewRequested(int year)

    property int gridYear: pickedYear
    property int focusIndex: 0
    property bool keyboardActive: false

    readonly property real cellHeight: Theme.layout.toolbarHeight
    readonly property real cellWidth: 2 * Theme.layout.toolbarHeight
    readonly property int firstKey: firstYear * 12 + firstMonth - 1
    readonly property int todayKey: todayYear * 12 + todayMonth - 1
    readonly property var years: {
        const list = []
        for (let y = todayYear; y >= firstYear; --y)
            list.push(y)
        return list
    }

    function minutesIn(y: int, m: int): int {
        let total = 0
        for (const h of history) {
            if (h.year === y && (m === 0 || h.month === m))
                total += h.minutes
        }
        return total
    }

    function hasHistoryIn(y: int, m: int): bool {
        return history.some(h => h.year === y && h.month === m)
    }

    function monthEnabled(y: int, m: int): bool {
        const key = y * 12 + m - 1
        return key >= firstKey && key <= todayKey
    }

    function hoursText(minutes: int): string {
        return minutes <= 0 ? qsTr("No history") : Library.shortPlayTime(minutes)
    }

    function pageYear(delta: int) {
        const y = gridYear + delta
        if (y >= firstYear && y <= todayYear)
            gridYear = y
    }

    function activate(index: int) {
        if (mode === "month") {
            if (!monthEnabled(gridYear, index + 1))
                return
            monthPicked(gridYear, index + 1)
        } else {
            yearPicked(years[index])
        }
        close()
    }

    function show() {
        gridYear = Math.min(Math.max(pickedYear, firstYear), todayYear)
        focusIndex = mode === "month" ? pickedMonth - 1 : Math.max(0, years.indexOf(pickedYear))
        keyboardActive = false
        open()
    }

    padding: Theme.filters.padding
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    onOpened: contentItem.forceActiveFocus(Qt.TabFocusReason)

    background: Surface {
        radius: Theme.filters.radius
        fill: Theme.filters.background
        fillEnd: Theme.filters.backgroundEnd
        direction: Theme.filters.gradientDirection
        border.color: Theme.filters.border
        border.width: Theme.filters.borderWidth
    }

    component Cell: Rectangle {
        id: cell

        property string text
        property string sideText
        property bool picked: false
        property bool dimmed: false
        property bool focused: false
        property string tip
        property alias hovered: cellMouse.containsMouse

        signal clicked()

        implicitHeight: root.cellHeight
        implicitWidth: root.cellWidth
        radius: Theme.filters.sortRadius
        color: picked ? Qt.alpha(Theme.stats.toggleCheckedBackground, Theme.stats.toggleCheckedOpacity)
                      : cellMouse.containsMouse && enabled ? Theme.filters.sortHover : "transparent"
        opacity: !enabled ? 0.3 : dimmed ? 0.55 : 1
        Behavior on color { enabled: Motion.fades; MotionColor {} }
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
        scale: cellMouse.pressed ? Theme.motion.pressScale : 1
        Behavior on scale { enabled: Motion.on; MotionNumber {} }
        border.color: Theme.filters.focusRing
        border.width: focused && root.keyboardActive ? Theme.filters.focusRingWidth : 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.layout.spacing
            anchors.rightMargin: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                horizontalAlignment: cell.sideText === "" ? Text.AlignHCenter : Text.AlignLeft
                text: cell.text
                color: cell.picked ? Qt.lighter(Theme.stats.toggleCheckedText, Theme.stats.lightBrightness)
                                   : Theme.stats.toggleText
                font.weight: cell.picked ? Theme.font.emphasisWeight : Font.Normal
                Behavior on color { enabled: Motion.fades; MotionColor {} }
            }
            Label {
                visible: cell.sideText !== ""
                text: cell.sideText
                color: Theme.stats.textMuted
                font.pixelSize: Theme.font.smallSize
            }
        }

        MouseArea {
            id: cellMouse
            anchors.fill: parent
            enabled: cell.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPositionChanged: root.keyboardActive = false
            onClicked: cell.clicked()
        }

        ToolTip.visible: cell.tip !== "" && cellMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: cell.tip
    }

    component PageButton: Cell {
        property string icon
        property int rotation: 0

        implicitWidth: root.cellHeight
        text: ""

        Icon {
            anchors.centerIn: parent
            size: Theme.iconButton.iconSize
            color: Theme.stats.toggleText
            path: parent.icon
            rotation: parent.rotation
        }
    }

    contentItem: FocusScope {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Keys.onPressed: event => {
            root.keyboardActive = true
            event.accepted = true
            const month = root.mode === "month"
            const count = month ? 12 : root.years.length
            const across = month ? 4 : 1
            const set = i => root.focusIndex = Math.min(Math.max(i, 0), count - 1)
            switch (event.key) {
            case Qt.Key_Left:
                if (month)
                    set(root.focusIndex - 1)
                break
            case Qt.Key_Right:
                if (month)
                    set(root.focusIndex + 1)
                break
            case Qt.Key_Up:
                set(root.focusIndex - across)
                break
            case Qt.Key_Down:
                set(root.focusIndex + across)
                break
            case Qt.Key_PageUp:
                if (month)
                    root.pageYear(-1)
                break
            case Qt.Key_PageDown:
                if (month)
                    root.pageYear(1)
                break
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Space:
                root.activate(root.focusIndex)
                break
            case Qt.Key_Escape:
                root.close()
                break
            default:
                event.accepted = false
            }
        }

        ColumnLayout {
            id: column
            width: parent.width
            spacing: Theme.layout.spacing

            RowLayout {
                visible: root.mode === "month"
                Layout.fillWidth: true
                spacing: 0

                PageButton {
                    icon: Icons.chevronRight
                    rotation: 180
                    enabled: root.gridYear > root.firstYear
                    onClicked: root.pageYear(-1)
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root.gridYear
                    color: Theme.palette.text
                    font.weight: Theme.font.emphasisWeight
                }
                PageButton {
                    icon: Icons.chevronRight
                    enabled: root.gridYear < root.todayYear
                    onClicked: root.pageYear(1)
                }
            }

            GridLayout {
                visible: root.mode === "month"
                columns: 4
                columnSpacing: Theme.layout.spacing / 2
                rowSpacing: Theme.layout.spacing / 2

                Repeater {
                    model: root.mode === "month" ? 12 : 0

                    Cell {
                        required property int index
                        readonly property int month: index + 1
                        text: Qt.locale().monthName(index, Locale.ShortFormat)
                        enabled: root.monthEnabled(root.gridYear, month)
                        picked: root.gridYear === root.pickedYear && month === root.pickedMonth
                        dimmed: enabled && !root.hasHistoryIn(root.gridYear, month)
                        focused: root.focusIndex === index
                        tip: enabled ? qsTr("%1 · %2").arg(Qt.locale().monthName(index, Locale.LongFormat))
                                         .arg(root.hoursText(root.minutesIn(root.gridYear, month))) : ""
                        onClicked: root.activate(index)
                    }
                }
            }

            Label {
                visible: root.mode === "month" && root.allowYearView
                Layout.alignment: Qt.AlignHCenter
                // The gap above the link matches the popup's padding below it.
                Layout.topMargin: Math.max(0, Theme.filters.padding - Theme.layout.spacing)
                text: qsTr("All of %1").arg(root.gridYear)
                color: Qt.lighter(Theme.stats.toggleCheckedText, Theme.stats.lightBrightness)
                font.underline: allMouse.containsMouse

                MouseArea {
                    id: allMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.yearViewRequested(root.gridYear)
                        root.close()
                    }
                }
            }

            ListView {
                visible: root.mode === "year"
                Layout.preferredWidth: 2.5 * root.cellWidth
                Layout.preferredHeight: Math.min(count, 8) * (root.cellHeight + Theme.layout.spacing / 2)
                clip: true
                spacing: Theme.layout.spacing / 2
                model: root.mode === "year" ? root.years : []
                currentIndex: root.focusIndex
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

                delegate: Cell {
                    required property int index
                    required property int modelData
                    width: ListView.view.width
                    text: String(modelData)
                    sideText: root.hoursText(root.minutesIn(modelData, 0))
                    picked: modelData === root.pickedYear
                    focused: root.focusIndex === index
                    onClicked: root.activate(index)
                }
            }
        }
    }
}
