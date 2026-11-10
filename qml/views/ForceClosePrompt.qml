import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var game: ({})

    signal message(string text)

    function ask(g) {
        game = g
        open()
    }

    function closeGame() {
        close()
        const e = GameRunner.forceClose(game.id)
        if (e !== "")
            message(e)
    }

    carded: false
    title: qsTr("Force close %1?").arg(game.title ?? "")

    onOpened: cancelButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("The game is ended at once, and what it hasn't saved is lost.")
    }

    footer: [
        SettingsButton {
            id: cancelButton
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            text: qsTr("Force close")
            danger: true
            onClicked: root.closeGame()
        }
    ]
}
