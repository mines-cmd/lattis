import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: root
    property alias label: name.text
    property alias field: input
    property bool secret: false
    required property real buttonWidth
    signal edited()
    signal finished()
    Layout.fillWidth: true
    spacing: Theme.layout.spacing

    Label {
        id: name
        Layout.preferredWidth: Theme.settingsWindow.labelMinWidth
        color: Theme.controls.labelColor
        font.pixelSize: Theme.controls.labelFontSize
    }
    SettingsTextField {
        id: input
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.controls.inputHeight
        echoMode: root.secret && !show.checked ? TextInput.Password : TextInput.Normal
        onTextEdited: root.edited()
        onEditingFinished: root.finished()
    }
    SettingsButton {
        id: show
        // Kept, invisible, on other rows so the fields line up.
        opacity: root.secret ? 1 : 0
        enabled: root.secret
        checkable: true
        text: checked ? qsTr("Hide") : qsTr("Show")
        Layout.preferredWidth: root.buttonWidth
    }
}
