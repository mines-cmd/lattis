import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    // The play history popup this opened from: its words for days, times and lengths.
    property var popup
    property string gameId
    property var sessions: []
    property var copies: []
    property int copyIndex: 0
    // "length" or "end": the one edited last is kept, the other is worked out.
    property string leading: "length"
    property var now: new Date()

    readonly property var copy: copies[copyIndex] ?? ({ id: gameId, launcher: "" })
    readonly property var copySessions: sessions.filter(s => (s.gameId || gameId) === copy.id && !s.merged)

    readonly property var startDate: startField.valid ? startField.value : null
    readonly property int minutes: leading === "end" && startDate && endField.valid
        ? Math.round((endField.value - startDate) / 60000) : lengthField.minutes
    readonly property var endDate: leading === "end" ? (endField.valid ? endField.value : null)
        : startDate && minutes > 0 ? new Date(startDate.getTime() + minutes * 60000) : null

    readonly property var overlap: {
        if (!startDate || !endDate || minutes <= 0)
            return null
        return copySessions.find(s => s.startMs < endDate.getTime() && startDate.getTime() < s.endMs) ?? null
    }

    readonly property string refusal: {
        if (!startDate)
            return qsTr("Enter the start's date and time")
        if (leading === "end" && !endDate)
            return qsTr("Enter the end's date and time")
        if (leading === "length" && !lengthField.valid)
            return qsTr("Enter the length")
        if (minutes <= 0)
            return leading === "end" ? qsTr("The end must be after the start") : qsTr("The length must be at least a minute")
        if (startDate > now)
            return qsTr("The start can't be in the future")
        if (endDate > now)
            return qsTr("The end can't be in the future")
        if (copySessions.some(s => Math.floor(s.endMs / 1000) === Math.floor(endDate.getTime() / 1000)))
            return qsTr("Another session of this game ends at that time")
        return ""
    }

    readonly property string resultText: {
        if (refusal !== "")
            return refusal
        const when = qsTr("%1, %2, %3").arg(popup.dayText(startDate)).arg(popup.timeText(startDate)).arg(popup.durationText(minutes))
        if (overlap)
            return qsTr("%1. Overlaps a session on %2").arg(when)
                .arg(new Date(overlap.startMs).toLocaleDateString(Qt.locale(), qsTr("ddd, d MMM")))
        return when
    }

    signal added(string copyId, var session)

    function openFor(popupItem, id, history, list) {
        popup = popupItem
        gameId = id
        sessions = history.sessions ?? []
        copies = list
        copyIndex = 0
        leading = "length"
        const start = new Date(Date.now() - 3600000)
        start.setMinutes(start.getMinutes() - start.getMinutes() % 5, 0, 0)
        now = new Date()
        startField.value = start
        lengthField.minutes = 60
        endField.value = new Date(start.getTime() + 3600000)
        open()
    }

    // The start keeps the length and moves the end.
    function startEdited() {
        if (!startDate || !lengthField.valid || lengthField.minutes <= 0)
            return
        leading = "length"
        endField.value = new Date(startDate.getTime() + lengthField.minutes * 60000)
    }

    // Whichever of length and end was not edited last follows the other.
    function followLength() {
        if (!startDate)
            return
        if (leading === "end") {
            if (endField.valid && minutes > 0)
                lengthField.minutes = minutes
        } else if (lengthField.valid && lengthField.minutes > 0) {
            endField.value = new Date(startDate.getTime() + lengthField.minutes * 60000)
        }
    }

    function add() {
        if (refusal !== "")
            return
        const result = Library.addSession(copy.id, Qt.formatDateTime(startDate, "yyyy-MM-ddTHH:mm:ss"), minutes)
        if (result.error !== "") {
            error.text = result.error
            return
        }
        const owner = copy.id
        close()
        added(owner, result.session)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(implicitWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    onOpened: {
        error.text = ""
        startField.dateField.forceActiveFocus()
    }

    onResultTextChanged: error.text = ""

    component FieldLabel: Label {
        color: Theme.gameEditor.labelColor
        Layout.alignment: Qt.AlignVCenter
    }

    component DayButtons: RowLayout {
        id: buttons
        property var field
        spacing: Theme.layout.spacing

        function pick(offset) {
            const day = new Date()
            day.setDate(day.getDate() + offset)
            field.pickDay(day)
        }

        SettingsButton { text: qsTr("Today"); onClicked: buttons.pick(0) }
        SettingsButton { text: qsTr("Yesterday"); onClicked: buttons.pick(-1) }
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Add session")
            color: Theme.gameEditor.headingColor
            font.pixelSize: Theme.gameEditor.headingFontSize
            font.weight: Theme.font.strongWeight
            elide: Text.ElideRight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.gameEditor.columnSpacing
            rowSpacing: Theme.gameEditor.rowSpacing

            FieldLabel { text: qsTr("Copy"); visible: root.copies.length > 1 }
            SettingsComboBox {
                visible: root.copies.length > 1
                Layout.columnSpan: 2
                Layout.fillWidth: true
                model: root.copies.map(c => c.platformName ? qsTr("%1 (%2)").arg(c.title).arg(c.platformName) : c.title)
                currentIndex: root.copyIndex
                onActivated: index => root.copyIndex = index
            }

            FieldLabel { text: qsTr("Start") }
            DateTimeField {
                id: startField
                maximumDate: root.now
                onEdited: root.startEdited()
                onAccepted: root.add()
            }
            DayButtons { field: startField }

            FieldLabel { text: qsTr("Length") }
            DurationField {
                id: lengthField
                Layout.columnSpan: 2
                onEdited: {
                    root.leading = "length"
                    root.followLength()
                }
                onAccepted: root.add()
            }

            FieldLabel { text: qsTr("End") }
            DateTimeField {
                id: endField
                maximumDate: root.now
                onEdited: {
                    root.leading = "end"
                    root.followLength()
                }
                onAccepted: root.add()
            }
            DayButtons { field: endField }
        }

        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: root.copy.launcher === "heroic"
            text: qsTr("Play through Heroic is counted already")
            color: Theme.playHistory.noteColor
            wrapMode: Text.WordWrap
        }

        Label {
            id: result
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            text: error.text !== "" ? error.text : root.resultText
            color: error.text !== "" || root.refusal !== "" ? Theme.gameEditor.errorColor
                 : root.overlap ? Theme.gameEditor.warningColor : Theme.gameEditor.labelColor
            wrapMode: Text.WordWrap
        }
        QtObject {
            id: error
            property string text: ""
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                text: qsTr("Add")
                primary: true
                enabled: root.refusal === ""
                onClicked: root.add()
            }
        }
    }
}
