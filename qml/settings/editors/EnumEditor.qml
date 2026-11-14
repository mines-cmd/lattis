import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string section
    property string key
    property var value
    property var choices

    Item { Layout.fillWidth: true }

    SettingsComboBox {
        id: combo
        Layout.preferredWidth: 200
        model: (root.choices ?? []).map(c => { const t = c.replace(/_/g, " "); return t.charAt(0).toUpperCase() + t.slice(1) })

        Connections {
            target: root
            function onValueChanged() { if (root.choices) combo.currentIndex = root.choices.indexOf(root.value) }
        }

        onActivated: if (root.choices[currentIndex] !== root.value) Theme.setValue(root.section, root.key, root.choices[currentIndex])
    }
}
