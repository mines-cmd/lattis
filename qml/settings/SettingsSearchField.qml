import QtQuick

SettingsTextField {
    id: root

    implicitHeight: Theme.settingsWindow.toolbarHeight
    leftPadding: Theme.settingsWindow.iconSize + 22
    rightPadding: text !== "" ? Theme.settingsWindow.iconSize + 22 : 12

    Icon {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        path: Icons.search
        size: Theme.settingsWindow.iconSize
        color: Theme.settingsWindow.iconColor
    }

    Icon {
        id: clearIcon
        visible: root.text !== ""
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        path: Icons.close
        size: Theme.settingsWindow.iconSize
        color: clearMouse.containsMouse ? Theme.settingsWindow.inputText : Theme.settingsWindow.iconColor

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
