import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var game: ({})

    signal failed(string text)
    signal done()

    function ask(g) {
        game = g
        open()
    }

    function uninstall() {
        close()
        const e = GameRunner.uninstall(game.id)
        if (e !== "")
            failed(e)
        else
            done()
    }

    carded: false
    scrim: false
    title: qsTr("Uninstall %1?").arg(game.title ?? "")

    onOpened: cancelButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("Steam will ask to confirm and remove the files. The game stays in your library as "
                   + "not installed, with its play time, tags and art.")
    }

    footer: [
        SettingsButton {
            id: cancelButton
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            text: qsTr("Uninstall")
            danger: true
            onClicked: root.uninstall()
        }
    ]
}
