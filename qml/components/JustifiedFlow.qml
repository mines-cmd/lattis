import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    property real spacing: 0
    property int maxRows: -1

    readonly property var items: {
        const out = []
        for (let i = 0; i < children.length; ++i) {
            if (!(children[i] instanceof Repeater))
                out.push(children[i])
        }
        return out
    }
    // Read only so the layout reruns when a child's size changes.
    readonly property var sizes: items.map(c => [c.implicitWidth, c.implicitHeight])

    onSizesChanged: Qt.callLater(relayout)
    onWidthChanged: Qt.callLater(relayout)
    onSpacingChanged: Qt.callLater(relayout)
    onMaxRowsChanged: Qt.callLater(relayout)

    function relayout() {
        const rows = []
        let row = []
        let used = 0
        for (const c of items) {
            if (row.length > 0 && used + spacing + c.implicitWidth > width) {
                rows.push(row)
                row = []
                used = 0
            }
            used += (row.length > 0 ? spacing : 0) + c.implicitWidth
            row.push(c)
        }
        if (row.length > 0)
            rows.push(row)

        let y = 0
        let height = 0
        rows.forEach((r, i) => {
            const shown = maxRows < 0 || i < maxRows
            const natural = r.reduce((sum, c) => sum + c.implicitWidth, 0) + (r.length - 1) * spacing
            // The list's own last row stays ragged: stretching two chips across the card looks worse.
            const extra = i === rows.length - 1 ? 0 : Math.max(0, (width - natural) / r.length)
            const rowHeight = Math.max(...r.map(c => c.implicitHeight))
            let x = 0
            for (const c of r) {
                // Edges rounded from the running total, so the rounding doesn't add up along the row.
                const left = Math.round(x)
                x += c.implicitWidth + extra
                c.x = left
                c.y = Math.round(y + (rowHeight - c.implicitHeight) / 2)
                c.width = Math.round(x) - left
                c.height = c.implicitHeight
                c.visible = shown
                x += spacing
            }
            if (shown)
                height = y + rowHeight
            y += rowHeight + spacing
        })
        implicitHeight = height
    }
}
