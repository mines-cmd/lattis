import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    signal message(string text)

    function undo() {
        close()
        const e = Library.undoAllSteamEdits()
        if (e !== "")
            message(e)
    }

    carded: false
    title: qsTr("Undo the changes waiting for Steam?")

    onOpened: cancelButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("Steam keeps what it has now, and these games take it back:")
    }
    Repeater {
        model: Library.steamWaiting

        Label {
            required property var modelData
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: "•  " + (modelData.adds
                           ? qsTr("%1: not in Steam yet, so it leaves the library").arg(modelData.title)
                           : modelData.title)
        }
    }

    footer: [
        SettingsButton {
            id: cancelButton
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            text: qsTr("Undo")
            danger: true
            onClicked: root.undo()
        }
    ]
}
