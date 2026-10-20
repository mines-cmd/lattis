import QtQuick
import QtQuick.Layouts

Item {
    property bool last: false
    default property alias content: listRowContent.data

    Layout.fillWidth: true
    implicitHeight: Math.max(Theme.settingsWindow.inputHeight, listRowContent.implicitHeight + 8)

    RowLayout {
        id: listRowContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 10
        anchors.rightMargin: 4
        spacing: Theme.settingsWindow.controlSpacing
    }
    Rectangle {
        visible: !parent.last
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: 1
        color: Theme.settingsWindow.rowDivider
    }
}
