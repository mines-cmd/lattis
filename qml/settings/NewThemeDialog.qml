import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    carded: false
    title: isColors ? qsTr("New colors") : qsTr("New style")

    property string half: "colors"
    property string failure
    readonly property bool isColors: half === "colors"
    readonly property var names: isColors ? Theme.availableColors : Theme.availableStyles

    function openFor(half, basedOn) {
        root.half = half
        root.failure = ""
        nameField.text = isColors ? qsTr("My colors") : qsTr("My style")
        descriptionField.text = ""
        basedCombo.currentIndex = Math.max(0, basedCombo.indexOfValue(basedOn))
        open()
    }

    Label { text: qsTr("Name"); color: Theme.controls.descriptionColor }
    SettingsTextField {
        id: nameField
        Layout.fillWidth: true
        implicitHeight: Theme.controls.inputHeight
        maximumLength: Theme.maxNameLength
        onTextChanged: root.failure = ""
    }
    PlainLabel {
        readonly property string error: root.failure !== "" ? root.failure : Theme.nameError(root.half, nameField.text)
        visible: error !== ""
        Layout.fillWidth: true
        text: error
        wrapMode: Text.WordWrap
        color: Theme.controls.descriptionColor
    }

    Label {
        visible: !root.isColors
        text: qsTr("Description (optional)")
        color: Theme.controls.descriptionColor
    }
    SettingsTextField {
        id: descriptionField
        visible: !root.isColors
        Layout.fillWidth: true
        implicitHeight: Theme.controls.inputHeight
        maximumLength: 48
    }
    Label {
        visible: !root.isColors
        Layout.alignment: Qt.AlignRight
        text: descriptionField.length + "/48"
        color: Theme.controls.descriptionColor
    }

    Label { text: qsTr("Based on"); color: Theme.controls.descriptionColor }
    SettingsComboBox {
        id: basedCombo
        Layout.fillWidth: true
        model: root.names.map(name => ({ text: name, value: name }))
        textRole: "text"
        valueRole: "value"
    }

    footer: [
        SettingsButton { text: qsTr("Cancel"); onClicked: root.close() },
        SettingsButton {
            primary: true
            text: qsTr("Create")
            enabled: Theme.nameError(root.half, nameField.text) === ""
            onClicked: {
                const ok = root.isColors ? Theme.createColors(nameField.text, basedCombo.currentValue)
                                         : Theme.createStyle(nameField.text, basedCombo.currentValue, descriptionField.text)
                if (ok)
                    root.close()
                else
                    root.failure = qsTr("Couldn't make the file. The Customize page's problems list says why.")
            }
        }
    ]
}
