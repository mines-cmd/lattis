import QtQuick
import QtTest
import "../../qml/components/DateInput.js" as DateInput

TestCase {
    name: "DateInput"

    readonly property var en: Qt.locale("en_US")
    readonly property var de: Qt.locale("de_DE")

    function test_dateFormat_data() {
        return [
            { tag: "two-digit year", format: "dd/MM/yy", shown: "dd/MM/yyyy", parsed: "d/M/yyyy" },
            { tag: "us short", format: "M/d/yy", shown: "MM/dd/yyyy", parsed: "M/d/yyyy" },
            { tag: "dots", format: "dd.MM.yyyy", shown: "dd.MM.yyyy", parsed: "d.M.yyyy" },
            { tag: "year first", format: "yyyy-MM-dd", shown: "yyyy-MM-dd", parsed: "yyyy-M-d" },
        ]
    }

    function test_dateFormat(data) {
        compare(DateInput.dateFormat(data.format), data.shown)
        compare(DateInput.dateParseFormat(data.format), data.parsed)
    }

    function test_parseDate() {
        const d = DateInput.parseDate(de, "5.3.2026", "dd.MM.yy")
        verify(d !== null)
        compare(d.getFullYear(), 2026)
        compare(d.getMonth(), 2)
        compare(d.getDate(), 5)
        const padded = DateInput.parseDate(de, "05.03.2026", "dd.MM.yy")
        compare(padded.getTime(), d.getTime())
    }

    function test_parseDate_refused_data() {
        return [
            { tag: "empty", text: "" },
            { tag: "no such day", text: "31.02.2026" },
            { tag: "two-digit year", text: "5.3.26" },
            { tag: "words", text: "yesterday" },
            { tag: "partial", text: "5.3." },
        ]
    }

    function test_parseDate_refused(data) {
        compare(DateInput.parseDate(de, data.text, "dd.MM.yy"), null)
    }

    function test_parseTime_data() {
        return [
            { tag: "24 h", locale: de, format: "HH:mm", text: "21:40", hours: 21, minutes: 40 },
            { tag: "24 h one digit", locale: de, format: "HH:mm", text: "9:05", hours: 9, minutes: 5 },
            { tag: "12 h pm", locale: en, format: "h:mm AP", text: "9:40 PM", hours: 21, minutes: 40 },
            { tag: "12 h lower case", locale: en, format: "h:mm AP", text: "9:40 pm", hours: 21, minutes: 40 },
            { tag: "12 h noon", locale: en, format: "h:mm AP", text: "12:00 PM", hours: 12, minutes: 0 },
            { tag: "12 h midnight", locale: en, format: "h:mm AP", text: "12:15 AM", hours: 0, minutes: 15 },
            { tag: "24 h typed in a 12 h locale", locale: en, format: "h:mm AP", text: "21:40", hours: 21, minutes: 40 },
        ]
    }

    function test_parseTime(data) {
        const t = DateInput.parseTime(data.locale, data.text, data.format)
        verify(t !== null)
        compare(t.hours, data.hours)
        compare(t.minutes, data.minutes)
    }

    function test_parseTime_refused_data() {
        return [
            { tag: "empty", text: "" },
            { tag: "hour 25", text: "25:00" },
            { tag: "minute 60", text: "10:60" },
            { tag: "words", text: "noon" },
        ]
    }

    function test_parseTime_refused(data) {
        compare(DateInput.parseTime(de, data.text, "HH:mm"), null)
    }

    function test_timeFormat() {
        compare(DateInput.timeFormat(en, false), "HH:mm")
        compare(DateInput.timeFormat(en, true), en.timeFormat(Locale.ShortFormat))
    }

    function test_round_trip() {
        const when = new Date(2026, 10, 7, 21, 5)
        const format = "dd/MM/yy"
        const text = DateInput.formatDate(en, when, format)
        compare(text, "07/11/2026")
        compare(DateInput.parseDate(en, text, format).getDate(), 7)
        const time = DateInput.formatTime(en, when, "h:mm AP")
        compare(time, "9:05 PM")
    }
}
