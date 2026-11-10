import QtQuick
import QtQuick.Layouts

Item {
    property bool last: false
    default property alias content: listRowContent.data

    Layout.fillWidth: true
    implicitHeight: Math.max(Theme.controls.inputHeight, listRowContent.implicitHeight + 8)

    RowLayout {
        id: listRowContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.layout.spacing
        anchors.rightMargin: Theme.layout.spacingSmall
        spacing: Theme.controls.controlSpacing
    }
    Rectangle {
        visible: !parent.last
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.layout.spacing
        anchors.rightMargin: Theme.layout.spacing
        height: 1
        color: Theme.settingsWindow.rowDivider
    }
}
