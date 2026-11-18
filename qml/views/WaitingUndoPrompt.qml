import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    readonly property bool steam: Library.steamWaiting.length > 0
    readonly property bool heroic: Library.heroicWaiting.length > 0

    signal message(string text)

    function undo() {
        close()
        if (heroic)
            Library.undoAllHeroicEdits()
        if (steam) {
            const e = Library.undoAllSteamEdits()
            if (e !== "")
                message(e)
        }
    }

    carded: false
    title: steam && heroic ? qsTr("Undo the waiting changes?")
         : heroic ? qsTr("Undo the changes waiting for Heroic?")
         : qsTr("Undo the changes waiting for Steam?")

    onOpened: cancelButton.forceActiveFocus()

    Label {
        visible: root.steam
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

    Label {
        visible: root.heroic
        Layout.fillWidth: true
        Layout.topMargin: root.steam ? Theme.layout.spacing : 0
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("Heroic keeps its settings for these games, and their waiting changes are dropped:")
    }
    Repeater {
        model: Library.heroicWaiting

        Label {
            required property var modelData
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: "•  " + (modelData.changes !== "" ? qsTr("%1: %2").arg(modelData.title).arg(modelData.changes)
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
