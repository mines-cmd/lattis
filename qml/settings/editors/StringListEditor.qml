import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string section
    property string key
    property var value
    property var choices

    readonly property var remaining: (root.choices && root.value) ? root.choices.filter(c => root.value.indexOf(c) === -1) : []

    spacing: 4

    function commit(list) { Theme.setValue(root.section, root.key, list) }
    function move(i, delta) {
        const list = root.value.slice()
        const j = i + delta
        if (j < 0 || j >= list.length)
            return
        const tmp = list[i]
        list[i] = list[j]
        list[j] = tmp
        commit(list)
    }
    function removeAt(i) {
        const list = root.value.slice()
        list.splice(i, 1)
        commit(list)
    }
    function add(name) {
        commit(root.value.concat([name]))
    }

    Repeater {
        model: root.value

        RowLayout {
            required property string modelData
            required property int index
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: modelData
                color: Theme.controls.inputText
            }
            IconButton {
                implicitWidth: 22; implicitHeight: 22
                iconSize: 12
                icon: Icons.chevron
                rotation: -90
                enabled: index > 0
                opacity: enabled ? 1 : 0.3
                onClicked: root.move(index, -1)
            }
            IconButton {
                implicitWidth: 22; implicitHeight: 22
                iconSize: 12
                icon: Icons.chevron
                rotation: 90
                enabled: index < root.value.length - 1
                opacity: enabled ? 1 : 0.3
                onClicked: root.move(index, 1)
            }
            IconButton {
                implicitWidth: 22; implicitHeight: 22
                iconSize: 12
                icon: Icons.close
                onClicked: root.removeAt(index)
            }
        }
    }

    SettingsComboBox {
        visible: root.remaining.length > 0
        Layout.preferredWidth: 180
        model: root.remaining
        displayText: qsTr("Add")
        onActivated: { root.add(currentText); currentIndex = -1 }
    }
}
