import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

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

    anchors.centerIn: parent
    modal: true
    Overlay.modal: Scrim {}
    padding: Theme.menu.dialogPadding
    width: 340

    background: Rectangle {
        color: Theme.menu.popupBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth
        radius: Theme.controls.inputRadius
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Description of %1").arg(root.name)
            color: Theme.controls.labelColor
            font.weight: Theme.font.strongWeight
            elide: Text.ElideRight
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

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.controls.controlSpacing
            SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
            SettingsButton {
                primary: true
                text: qsTr("Save")
                onClicked: root.save()
            }
        }
    }
}
