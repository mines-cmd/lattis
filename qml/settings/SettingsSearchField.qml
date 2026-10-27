import QtQuick

SettingsTextField {
    id: root

    property color iconColor: Theme.controls.iconColor

    implicitHeight: Theme.settingsWindow.toolbarHeight
    leftPadding: Theme.controls.iconSize + 22
    rightPadding: text !== "" ? Theme.controls.iconSize + 22 : 12

    Icon {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        path: Icons.search
        size: Theme.controls.iconSize
        color: root.iconColor
    }

    Icon {
        id: clearIcon
        visible: root.text !== ""
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        path: Icons.close
        size: Theme.controls.iconSize
        color: clearMouse.containsMouse ? root.textColor : root.iconColor

        MouseArea {
            id: clearMouse
            anchors.fill: parent
            anchors.margins: -6
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clear()
        }
    }
}
