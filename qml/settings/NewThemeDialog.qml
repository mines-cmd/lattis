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
    padding: Theme.settingsWindow.dialogPadding
    width: 320

    background: Rectangle {
        color: Theme.settingsWindow.popupBackground
        border.color: Theme.settingsWindow.inputBorder
        border.width: Theme.settingsWindow.inputBorderWidth
        radius: Theme.settingsWindow.inputRadius
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            text: root.isColors ? qsTr("New colors") : qsTr("New style")
            color: Theme.settingsWindow.labelColor
            font.bold: true
        }

        Label { text: qsTr("Name"); color: Theme.settingsWindow.descriptionColor }
        SettingsTextField {
            id: nameField
            Layout.fillWidth: true
            implicitHeight: Theme.settingsWindow.inputHeight
        }
        Label {
            readonly property string error: Theme.nameError(root.half, nameField.text)
            visible: error !== ""
            Layout.fillWidth: true
            text: error
            wrapMode: Text.WordWrap
            color: Theme.settingsWindow.descriptionColor
        }

        Label {
            visible: !root.isColors
            text: qsTr("Description (optional)")
            color: Theme.settingsWindow.descriptionColor
        }
        SettingsTextField {
            id: descriptionField
            visible: !root.isColors
            Layout.fillWidth: true
            implicitHeight: Theme.settingsWindow.inputHeight
            maximumLength: 48
        }
        Label {
            visible: !root.isColors
            Layout.alignment: Qt.AlignRight
            text: descriptionField.length + "/48"
            color: Theme.settingsWindow.descriptionColor
        }

        Label { text: qsTr("Based on"); color: Theme.settingsWindow.descriptionColor }
        SettingsComboBox {
            id: basedCombo
            Layout.fillWidth: true
            model: root.names.map(name => ({ text: name, value: name }))
            textRole: "text"
            valueRole: "value"
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.settingsWindow.controlSpacing
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
