import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    pops: true
    popDirection: "below"

    property var managers: []

    signal chosen(string id)

    Instantiator {
        model: root.managers
        delegate: SettingsMenuItem {
            required property var modelData
            iconPath: Icons.puzzle
            text: modelData.name
            keys: GameRunner.runningManagers.includes(modelData.id) ? qsTr("Running") : ""
            onTriggered: Qt.callLater(() => root.chosen(modelData.id))
        }
        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
}
