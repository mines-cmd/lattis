import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property string half: "colors"
    readonly property bool isColors: half === "colors"
    readonly property var names: isColors ? Theme.availableColors : Theme.availableStyles

    function openFor(half, basedOn) {
        root.half = half
        nameField.text = isColors ? qsTr("My colors") : qsTr("My style")
        descriptionField.text = ""
        basedCombo.currentIndex = Math.max(0, basedCombo.indexOfValue(basedOn))
        open()
    }

    anchors.centerIn: parent
    modal: true
    Overlay.modal: Scrim {}
    padding: Theme.menu.dialogPadding
    width: 320

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
            text: root.isColors ? qsTr("New colors") : qsTr("New style")
            color: Theme.controls.labelColor
            font.weight: Theme.font.strongWeight
        }

        Label { text: qsTr("Name"); color: Theme.controls.descriptionColor }
        SettingsTextField {
            id: nameField
            Layout.fillWidth: true
            implicitHeight: Theme.controls.inputHeight
        }
        Label {
            readonly property string error: Theme.nameError(root.half, nameField.text)
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

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.controls.controlSpacing
            SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
            SettingsButton {
                primary: true
                text: qsTr("Create")
                enabled: Theme.nameError(root.half, nameField.text) === ""
                onClicked: {
                    const ok = root.isColors ? Theme.createColors(nameField.text, basedCombo.currentValue)
                                             : Theme.createStyle(nameField.text, basedCombo.currentValue, descriptionField.text)
                    if (ok)
                        root.close()
                }
            }
        }
    }
}
