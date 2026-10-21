import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string section
    property string key
    property var value
    property real min
    property real max
    property string type
    property bool onRelease: false

    readonly property bool isInt: type === "Int"
    readonly property real minimumWidth: 48 + spacing + field.Layout.preferredWidth

    spacing: Theme.controls.controlSpacing

    function commit(v) {
        Theme.setValue(root.section, root.key, root.isInt ? Math.round(v) : v)
    }

    function format(v) {
        if (v === undefined)
            return ""
        return root.isInt ? Math.round(v).toString() : Number(v).toLocaleString(Qt.locale("C"), 'f', 3)
    }

    SettingsSlider {
        id: slider
        Layout.fillWidth: true
        from: root.min
        to: root.max
        stepSize: root.isInt ? 1 : (root.max - root.min) / 200

        Connections {
            target: root
            function onValueChanged() { if (!slider.pressed && root.value !== undefined) slider.value = root.value }
        }

        property bool dragged: false

        onMoved: {
            if (root.onRelease) {
                field.text = root.format(value)
                dragged = true
            } else
                root.commit(value)
        }
        onPressedChanged: {
            if (!pressed && dragged) {
                dragged = false
                root.commit(value)
            }
        }
    }

    SettingsTextField {
        id: field
        Layout.preferredWidth: 70
        horizontalAlignment: TextInput.AlignRight

        Connections {
            target: root
            function onValueChanged() { if (!field.activeFocus) field.text = root.format(root.value) }
        }

        onEditingFinished: {
            if (text === root.format(root.value))
                return // not edited; saving would add the key to the file
            const v = parseFloat(text)
            if (!isNaN(v) && v >= root.min && v <= root.max)
                Theme.setValue(root.section, root.key, root.isInt ? Math.round(v) : v)
            else
                text = root.format(root.value)
        }
    }
}
