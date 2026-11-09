import QtQuick
import QtQuick.Controls.Basic

// A bar chart (one bar per position) over a slider with one handle (the low one) or two.
// `moved` fires when a drag ends or a key moves a handle, not while it is dragged.
FocusScope {
    id: root

    property var counts: []
    property real minValue: 0
    property real step: 1
    property bool range: false
    property real low: minValue
    property real high: minValue + step * (count - 1)
    property string lowName
    property string highName
    property string minText
    property string maxText
    property var tipText: (value, games) => ""
    property int hoveredIndex: -1

    signal moved(real low, real high)

    readonly property int count: counts.length
    readonly property real maxValue: minValue + step * Math.max(0, count - 1)
    readonly property int handleSize: Theme.filters.sliderHandleSize
    readonly property real inset: handleSize / 2
    readonly property real trackWidth: Math.max(0, width - 2 * inset)
    readonly property real slot: count > 0 ? Math.max(0, width - 2 * inset) / count : 0
    readonly property real peak: counts.reduce((m, c) => Math.max(m, c), 0)

    property real shownLow: low
    property real shownHigh: high
    property bool dragging: false

    Binding { target: root; property: "shownLow"; value: root.low; when: !root.dragging }
    Binding { target: root; property: "shownHigh"; value: root.high; when: !root.dragging }

    implicitHeight: Theme.filters.histogramHeight + Theme.layout.spacing + handleSize + (axis.visible ? Theme.layout.spacing + axis.height : 0)

    function indexOf(value) {
        return Math.max(0, Math.min(count - 1, Math.round((value - minValue) / step)))
    }

    function xOf(value) {
        return count > 1 ? inset + indexOf(value) / (count - 1) * trackWidth : inset
    }

    function valueAt(x) {
        const position = count > 1 && trackWidth > 0 ? Math.round((x - inset) / trackWidth * (count - 1)) : 0
        return minValue + Math.max(0, Math.min(count - 1, position)) * step
    }

    function setHandle(which, value) {
        if (which === "low")
            shownLow = range ? Math.min(value, shownHigh) : value
        else
            shownHigh = Math.max(value, shownLow)
    }

    function commit() {
        moved(shownLow, range ? shownHigh : maxValue)
    }

    function nudge(which, by) {
        const current = which === "low" ? shownLow : shownHigh
        setHandle(which, Math.max(minValue, Math.min(maxValue, current + by * step)))
        commit()
    }

    function selected(position) {
        const value = minValue + position * step
        return value >= shownLow && (!range || value <= shownHigh)
    }

    Item {
        id: chart
        width: parent.width
        height: Theme.filters.histogramHeight

        Repeater {
            model: root.count

            Rectangle {
                required property int index
                x: root.inset + index * root.slot + Math.min(1, root.slot / 6)
                width: Math.max(1, root.slot - 2 * Math.min(1, root.slot / 6))
                height: root.peak > 0 ? Math.max(2,root.counts[index] / root.peak * chart.height) : 0
                y: chart.height - height
                radius: Theme.filters.histogramBarRadius
                color: root.selected(index) ? Theme.filters.histogramBarSelected : Theme.filters.histogramBar
                opacity: root.counts[index] > 0 ? 1 : Theme.filters.histogramEmptyOpacity
            }
        }

        Item {
            id: tipAnchor
            readonly property int index: root.hoveredIndex
            readonly property string tip: index >= 0 ? root.tipText(root.minValue + index * root.step, root.counts[index] ?? 0) : ""
            x: root.inset + Math.max(0, index) * root.slot
            width: root.slot
            height: chart.height

            ToolTip.visible: index >= 0 && tip !== ""
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: tip
        }
    }

    Row {
        id: axis
        visible: root.minText !== "" || root.maxText !== ""
        y: chart.height + 2 * Theme.layout.spacing + root.handleSize
        width: parent.width
        height: visible ? Math.max(minLabel.implicitHeight, maxLabel.implicitHeight) : 0

        Label {
            id: minLabel
            width: parent.width / 2
            text: root.minText
            color: Theme.filters.hintColor
            horizontalAlignment: Text.AlignLeft
        }
        Label {
            id: maxLabel
            width: parent.width / 2
            text: root.maxText
            color: Theme.filters.hintColor
            horizontalAlignment: Text.AlignRight
        }
    }

    HoverHandler {
        id: hover
        onPointChanged: {
            const p = point.position
            root.hoveredIndex = hovered && p.y < chart.height && p.x >= root.inset && p.x < root.width - root.inset && root.slot > 0
                ? Math.max(0, Math.min(root.count - 1, Math.floor((p.x - root.inset) / root.slot))) : -1
        }
        onHoveredChanged: if (!hovered) root.hoveredIndex = -1
    }

    Item {
        id: slider
        y: chart.height + Theme.layout.spacing
        width: parent.width
        height: root.handleSize

        Rectangle {
            id: track
            x: root.inset
            width: parent.width - 2 * root.inset
            height: Math.max(2, root.handleSize / 4)
            anchors.verticalCenter: parent.verticalCenter
            radius: height / 2
            color: Theme.filters.sliderTrack
        }
        Rectangle {
            x: root.xOf(root.shownLow)
            width: (root.range ? root.xOf(root.shownHigh) : track.x + track.width) - x
            height: track.height
            anchors.verticalCenter: parent.verticalCenter
            radius: height / 2
            color: Theme.filters.sliderFill
        }

        component Handle: Rectangle {
            id: handle
            required property string which
            property real value
            x: root.xOf(value) - width / 2
            width: root.handleSize
            height: width
            radius: width / 2
            color: Theme.filters.sliderHandle
            border.color: Theme.filters.focusRing
            border.width: activeFocus ? Theme.filters.focusRingWidth : 0
            activeFocusOnTab: true
            Accessible.role: Accessible.Slider
            Accessible.name: which === "low" ? root.lowName : root.highName

            // The list's navigation keys are window shortcuts; claiming these keeps them on the handle.
            Keys.onShortcutOverride: event => {
                event.accepted = [Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down, Qt.Key_PageUp, Qt.Key_PageDown,
                                  Qt.Key_Home, Qt.Key_End].includes(event.key)
            }
            Keys.onPressed: event => {
                const big = Math.max(1, Math.round(root.count / 10))
                const keys = { [Qt.Key_Left]: -1, [Qt.Key_Down]: -1, [Qt.Key_Right]: 1, [Qt.Key_Up]: 1,
                               [Qt.Key_PageDown]: -big, [Qt.Key_PageUp]: big }
                if (event.key in keys) {
                    root.nudge(which, keys[event.key])
                    event.accepted = true
                } else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                    root.setHandle(which, event.key === Qt.Key_Home ? root.minValue : root.maxValue)
                    root.commit()
                    event.accepted = true
                }
            }
        }

        Handle { which: "low"; value: root.shownLow }
        Handle { visible: root.range; which: "high"; value: root.shownHigh }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.count > 0
        cursorShape: Qt.PointingHandCursor
        property string grabbed: "low"

        function drag(x) {
            root.setHandle(grabbed, root.valueAt(x))
        }

        onPressed: mouse => {
            const value = root.valueAt(mouse.x)
            if (!root.range)
                grabbed = "low"
            else if (value < root.shownLow)
                grabbed = "low"
            else if (value > root.shownHigh)
                grabbed = "high"
            else
                grabbed = value - root.shownLow < root.shownHigh - value ? "low" : "high"
            root.dragging = true
            drag(mouse.x)
        }
        onPositionChanged: mouse => drag(mouse.x)
        onReleased: {
            root.commit()
            root.dragging = false
        }
    }
}
