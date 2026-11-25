import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property alias field: hostField

    implicitWidth: hostColumn.implicitWidth + 2 * Theme.menu.dialogPadding
    implicitHeight: hostColumn.implicitHeight + 2 * Theme.menu.dialogPadding
    color: Theme.menu.popupBackground
    radius: Theme.controls.inputRadius

    ColumnLayout {
        id: hostColumn
        anchors.centerIn: parent
        spacing: Theme.controls.controlSpacing

        DateTimeField {
            id: hostField
            maximumDate: new Date()
        }
        Label {
            text: hostField.valid ? "picked " + Qt.formatDateTime(hostField.value, "yyyy-MM-dd HH:mm") : "nothing picked"
            color: Theme.controls.labelColor
        }
    }
}
