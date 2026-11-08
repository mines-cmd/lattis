import QtQuick
import QtQuick.Layouts
import "DateInput.js" as DateInput

RowLayout {
    id: root

    // A Date to the minute, or null while either field doesn't hold a whole date or time.
    property var value: null
    readonly property bool valid: value instanceof Date
    readonly property alias dateField: dateInput
    readonly property alias timeField: timeInput

    readonly property var locale: Qt.locale()
    readonly property string shortFormat: General.shortDateFormat
    readonly property string timeFormat: DateInput.timeFormat(locale, General.systemLocale)

    signal edited()
    signal accepted()

    property bool settingValue: false

    function refresh() {
        dateInput.text = valid ? DateInput.formatDate(locale, value, shortFormat) : ""
        timeInput.text = valid ? DateInput.formatTime(locale, value, timeFormat) : ""
    }

    function reparse() {
        const date = DateInput.parseDate(locale, dateInput.text, shortFormat)
        const time = DateInput.parseTime(locale, timeInput.text, timeFormat)
        settingValue = true
        value = date && time ? new Date(date.getFullYear(), date.getMonth(), date.getDate(), time.hours, time.minutes) : null
        settingValue = false
        edited()
    }

    onValueChanged: if (!settingValue) refresh()
    onShortFormatChanged: refresh()
    onTimeFormatChanged: refresh()
    Component.onCompleted: refresh()

    spacing: Theme.layout.spacing

    SettingsTextField {
        id: dateInput
        readonly property bool unreadable: text.trim() !== "" && !DateInput.parseDate(root.locale, text, root.shortFormat)

        Layout.preferredWidth: sample.advanceWidth + leftPadding + rightPadding
        placeholderText: DateInput.dateFormat(root.shortFormat).toLowerCase()
        inputMethodHints: Qt.ImhPreferNumbers
        invalid: unreadable
        onTextEdited: root.reparse()
        onEditingFinished: if (root.valid) root.refresh()
        onAccepted: root.accepted()

        TextMetrics {
            id: sample
            font: dateInput.font
            text: DateInput.formatDate(root.locale, new Date(2000, 11, 30), root.shortFormat)
        }
    }

    SettingsTextField {
        id: timeInput
        readonly property bool unreadable: text.trim() !== "" && !DateInput.parseTime(root.locale, text, root.timeFormat)

        Layout.preferredWidth: timeSample.advanceWidth + leftPadding + rightPadding
        placeholderText: root.timeFormat
        inputMethodHints: Qt.ImhPreferNumbers
        invalid: unreadable
        onTextEdited: root.reparse()
        onEditingFinished: if (root.valid) root.refresh()
        onAccepted: root.accepted()

        TextMetrics {
            id: timeSample
            font: timeInput.font
            text: DateInput.formatTime(root.locale, new Date(2000, 0, 1, 12, 59), root.timeFormat)
        }
    }
}
