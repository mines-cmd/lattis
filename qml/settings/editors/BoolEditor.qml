import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string section
    property string key
    property var value

    Item { Layout.fillWidth: true }

    SettingsSwitch {
        id: sw
        Connections {
            target: root
            function onValueChanged() { sw.checked = root.value === true }
        }
        onToggled: Theme.setValue(root.section, root.key, checked)
    }
}
