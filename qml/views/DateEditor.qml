import QtQuick
import QtQuick.Controls.Basic

FocusScope {
    id: root

    readonly property string format: General.shortDateFormat
    readonly property var order: ["d", "M", "y"].sort((a, b) => format.indexOf(a) - format.indexOf(b))
    readonly property string separator: (format.match(/[^dMy\s]/) ?? ["/"])[0]
    readonly property var parts: ({
        d: { placeholder: qsTr("DD"), top: 31, digits: 2 },
        M: { placeholder: qsTr("MM"), top: 12, digits: 2 },
        y: { placeholder: qsTr("YYYY"), top: 9999, digits: 4 }
    })

    signal accepted()

    function field(part) {
        return segments.itemAt(order.indexOf(part))
    }

    // A Date (at local midnight), null when all three segments are empty, undefined when they
    // don't make a whole date.
    function value() {
        const day = field("d").text, month = field("M").text, year = field("y").text
        if (day === "" && month === "" && year === "")
            return null
        const d = parseInt(day), m = parseInt(month), y = parseInt(year)
        if (year.length !== 4 || !(d > 0) || !(m > 0))
            return undefined
        const date = new Date(y, m - 1, d)
        return date.getDate() === d && date.getMonth() === m - 1 ? date : undefined
    }

    function set(date) {
        const valid = date instanceof Date && !isNaN(date)
        field("d").text = valid ? String(date.getDate()).padStart(2, "0") : ""
        field("M").text = valid ? String(date.getMonth() + 1).padStart(2, "0") : ""
        field("y").text = valid ? String(date.getFullYear()) : ""
    }

    // From the date in the segments, or today while they don't hold one. Months and years keep
    // the day where they can (31 January + 1 month = 28 or 29 February).
    function step(part, direction) {
        const base = value() ?? new Date()
        const y = base.getFullYear() + (part === "y" ? direction : 0)
        const m = base.getMonth() + (part === "M" ? direction : 0)
        const days = new Date(y, m + 1, 0).getDate()
        const date = part === "d" ? new Date(y, m, base.getDate() + direction)
                                  : new Date(y, m, Math.min(base.getDate(), days))
        if (date.getFullYear() >= 1000 && date.getFullYear() <= 9999)
            set(date)
    }

    function edit() {
        segments.itemAt(0).forceActiveFocus()
    }

    function move(index, direction) {
        segments.itemAt(index + direction)?.forceActiveFocus()
    }

    implicitWidth: row.implicitWidth + 2 * Theme.gameEditor.numberFieldPadding
    implicitHeight: row.implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: Theme.controls.inputRadius
        color: Theme.controls.inputBackground
        border.color: root.activeFocus ? Theme.controls.inputBorderFocus : Theme.controls.inputBorder
        border.width: root.activeFocus ? Theme.controls.inputBorderFocusWidth : Theme.controls.inputBorderWidth

        TapHandler { onTapped: if (!root.activeFocus) root.edit() }
    }

    // The font of the other fields; a plain TextInput doesn't take the controls' font.
    Label {
        id: fontSource
        visible: false
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.gameEditor.dateSpacing

        Repeater {
            id: segments
            model: root.order

            TextInput {
                id: segment
                required property string modelData
                required property int index
                readonly property var spec: root.parts[modelData]

                anchors.verticalCenter: parent.verticalCenter
                // As wide as what it shows, so the separators sit evenly between the digits.
                width: Math.ceil(text === "" ? placeholder.advanceWidth : digits.advanceWidth)
                activeFocusOnTab: true
                font: fontSource.font
                color: Theme.controls.inputText
                horizontalAlignment: TextInput.AlignHCenter
                maximumLength: spec.digits
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: segment.spec.top }

                // After the click that focused it has placed the cursor. Leaving pads a lone digit.
                onActiveFocusChanged: {
                    if (activeFocus)
                        Qt.callLater(selectAll)
                    else if (spec.digits === 2 && text.length === 1)
                        text = text.padStart(2, "0")
                }
                // A full segment, or a digit no two-digit value starts with (a 4 for a day), moves on.
                onTextEdited: {
                    const early = spec.digits === 2 && text.length === 1 && parseInt(text) * 10 > spec.top
                    if (early)
                        text = text.padStart(2, "0")
                    if (early || text.length === spec.digits)
                        root.move(index, 1)
                }
                Keys.onUpPressed: root.step(modelData, 1)
                Keys.onDownPressed: root.step(modelData, -1)
                Keys.onLeftPressed: event => {
                    if (cursorPosition === 0 || selectedText === text)
                        root.move(index, -1)
                    else
                        event.accepted = false
                }
                Keys.onRightPressed: event => {
                    if (cursorPosition === text.length || selectedText === text)
                        root.move(index, 1)
                    else
                        event.accepted = false
                }
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Backspace && text === "")
                        root.move(index, -1)
                    else
                        event.accepted = false
                }
                // Enter even while the segment is empty, which the validator doesn't accept.
                Keys.onReturnPressed: root.accepted()
                Keys.onEnterPressed: root.accepted()

                Rectangle {
                    z: -1
                    anchors.fill: parent
                    anchors.leftMargin: -Theme.gameEditor.dateSpacing / 2
                    anchors.rightMargin: -Theme.gameEditor.dateSpacing / 2
                    radius: Theme.controls.inputRadius
                    color: Theme.gameEditor.dateSegmentFocus
                    visible: segment.activeFocus
                }
                Text {
                    visible: segment.index > 0
                    anchors.verticalCenter: parent.verticalCenter
                    x: -(row.spacing + width) / 2
                    text: root.separator
                    font: segment.font
                    color: Theme.gameEditor.dateSeparatorColor
                }
                Text {
                    anchors.centerIn: parent
                    visible: segment.text === ""
                    text: segment.spec.placeholder
                    font: segment.font
                    color: Theme.controls.placeholderColor
                }
                TextMetrics {
                    id: digits
                    font: segment.font
                    text: "0".repeat(segment.spec.digits)
                }
                TextMetrics {
                    id: placeholder
                    font: segment.font
                    text: segment.spec.placeholder
                }
                WheelHandler {
                    enabled: segment.activeFocus
                    onWheel: event => root.step(segment.modelData, Math.sign(event.angleDelta.y))
                }
            }
        }
    }
}
