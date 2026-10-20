import QtQuick
import QtQuick.Controls.Basic

SettingsMenu {
    id: root

    property var platform: ({})

    signal editRequested(string id)
    signal hideRequested(string id)

    pops: true

    SettingsMenuItem {
        iconPath: Icons.editLine
        text: qsTr("Edit platform")
        onTriggered: Qt.callLater(root.editRequested, root.platform.id)
    }
    SettingsMenuItem {
        iconPath: Icons.eyeCloseLine
        text: qsTr("Hide from the rail")
        onTriggered: Qt.callLater(root.hideRequested, root.platform.id)
    }
}
