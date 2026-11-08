import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: root

    property int minutes: 0
    readonly property bool valid: hoursInput.text !== "" || minutesInput.text !== ""
    readonly property alias hoursField: hoursInput
    readonly property alias minutesField: minutesInput

    signal edited()
    signal accepted()

    property bool settingMinutes: false

    function refresh() {
        hoursInput.text = String(Math.floor(minutes / 60))
        minutesInput.text = String(minutes % 60).padStart(2, "0")
    }

    function reparse() {
        settingMinutes = true
        minutes = valid ? (parseInt(hoursInput.text) || 0) * 60 + (parseInt(minutesInput.text) || 0) : 0
        settingMinutes = false
        edited()
    }

    onMinutesChanged: if (!settingMinutes) refresh()
    Component.onCompleted: refresh()

    spacing: Theme.layout.spacing

    SettingsTextField {
        id: hoursInput
        Layout.preferredWidth: hoursSample.advanceWidth + leftPadding + rightPadding
        horizontalAlignment: TextInput.AlignRight
        placeholderText: "0"
        inputMethodHints: Qt.ImhDigitsOnly
        validator: IntValidator { bottom: 0; top: 999 }
        onTextEdited: root.reparse()
        onEditingFinished: if (root.valid) root.refresh()
        onAccepted: root.accepted()

        TextMetrics {
            id: hoursSample
            font: hoursInput.font
            text: "000"
        }
    }
    Label {
        text: qsTr("h")
        color: Theme.gameEditor.labelColor
    }
    SettingsTextField {
        id: minutesInput
        Layout.leftMargin: Theme.layout.spacing
        Layout.preferredWidth: minutesSample.advanceWidth + leftPadding + rightPadding
        horizontalAlignment: TextInput.AlignRight
        placeholderText: "00"
        inputMethodHints: Qt.ImhDigitsOnly
        validator: IntValidator { bottom: 0; top: 59 }
        onTextEdited: root.reparse()
        onEditingFinished: if (root.valid) root.refresh()
        onAccepted: root.accepted()

        TextMetrics {
            id: minutesSample
            font: minutesInput.font
            text: "00"
        }
    }
    Label {
        text: qsTr("m")
        color: Theme.gameEditor.labelColor
    }
}
