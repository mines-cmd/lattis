import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var titles: []

    signal quitConfirmed()

    function ask(list) {
        titles = list
        open()
    }

    carded: false
    title: qsTr("Quit Lattis?")

    onOpened: stayButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: root.titles.length === 1
              ? qsTr("%1 is still running. It keeps running if you quit, but its play time isn't recorded.")
                    .arg(root.titles[0])
              : qsTr("%1 are still running. They keep running if you quit, but their play time isn't recorded.")
                    .arg(root.titles.join(", "))
    }

    footer: [
        SettingsButton {
            id: stayButton
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            text: qsTr("Quit")
            danger: true
            onClicked: root.quitConfirmed()
        }
    ]
}
