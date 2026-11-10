import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    carded: false
    title: qsTr("Description of %1").arg(name)

    property string name

    signal saved(string text)

    function openFor(name) {
        root.name = name
        field.text = Theme.styleDescription(name)
        open()
        field.forceActiveFocus()
        field.selectAll()
    }

    function save() {
        if (Theme.setStyleDescription(name, field.text))
            saved(qsTr("Changed the description of %1").arg(name))
        close()
    }

    SettingsTextField {
        id: field
        Layout.fillWidth: true
        implicitHeight: Theme.controls.inputHeight
        maximumLength: 48
        onAccepted: root.save()
    }
    Label {
        Layout.alignment: Qt.AlignRight
        text: field.length + "/48"
        color: Theme.controls.descriptionColor
    }

    footer: [
        SettingsButton { text: qsTr("Cancel"); onClicked: root.close() },
        SettingsButton {
            primary: true
            text: qsTr("Save")
            onClicked: root.save()
        }
    ]
}
