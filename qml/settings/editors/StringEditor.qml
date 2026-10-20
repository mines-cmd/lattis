import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string section
    property string key
    property var value
    readonly property real minimumWidth: 120

    SettingsTextField {
        id: field
        Layout.fillWidth: true

        Connections {
            target: root
            function onValueChanged() { if (!field.activeFocus) field.text = root.value ?? "" }
        }

        // Leaving the field without typing isn't an edit; saving then would add the key to the file.
        onEditingFinished: if (text !== (root.value ?? "")) Theme.setValue(root.section, root.key, text)
    }
}
