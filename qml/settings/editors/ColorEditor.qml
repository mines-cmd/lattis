import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string section
    property string key
    property var value
    property var raw

    spacing: Theme.settingsWindow.controlSpacing

    function formatHex(c) {
        const toHex = n => Math.round(n * 255).toString(16).padStart(2, "0")
        let hex = "#" + toHex(c.r) + toHex(c.g) + toHex(c.b)
        if (c.a < 1)
            hex += toHex(c.a)
        return hex
    }

    function currentText() {
        if (root.value === undefined)
            return ""
        return (typeof root.raw === "string" && root.raw.startsWith("@")) ? root.raw : formatHex(root.value)
    }

    Item { Layout.fillWidth: true }

    Rectangle {
        Layout.preferredWidth: Theme.settingsWindow.swatchSize
        Layout.preferredHeight: Theme.settingsWindow.swatchSize
        radius: 4
        color: root.value ?? "transparent"
        border.color: Theme.settingsWindow.inputBorder
        border.width: Theme.settingsWindow.inputBorderWidth > 0 ? Theme.settingsWindow.inputBorderWidth : 1

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: picker.openFor(root, root.value, root.raw)
        }
    }

    SettingsTextField {
        id: field
        Layout.preferredWidth: 90
        leftPadding: 8
        rightPadding: 8

        Connections {
            target: root
            function onValueChanged() { if (!field.activeFocus) field.text = root.currentText() }
            function onRawChanged() { if (!field.activeFocus) field.text = root.currentText() }
        }

        onEditingFinished: applyText()

        function applyText() {
            const t = text.trim()
            if (t === root.currentText())
                return // not edited; saving would add the key to the file
            if (t.startsWith("@") && t.length > 1) {
                Theme.setValue(root.section, root.key, t)
                return
            }
            if (/^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(t))
                Theme.setValue(root.section, root.key, t)
            else
                text = root.currentText()
        }
    }

    ColorPickerPopup {
        id: picker
        excludedName: root.section === "palette" ? root.key : ""
        onColorPicked: c => Theme.setValue(root.section, root.key, c)
        onReferencePicked: (reference, c) => Theme.setValue(root.section, root.key, reference)
    }
}
