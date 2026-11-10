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

    function run(restart) {
        close()
        const e = restart ? GameRunner.restartSteamAndLaunch(game.id) : GameRunner.launch(game.id)
        if (e !== "")
            message(e)
    }

    carded: false
    title: qsTr("Restart Steam to launch %1 with its changes?").arg(game.title ?? "")

    onOpened: restartButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: (root.game.shortcutQueued ?? false)
              ? qsTr("Its shortcut isn't in Steam yet, and Steam only takes it while it's closed. Restarting "
                     + "closes Steam, adds it and starts the game.")
              : root.game.platform === "proton"
              ? qsTr("Its launch options, Proton version or shortcut changed here, and Steam only takes them "
                     + "while it's closed. Restarting closes Steam, applies them and starts the game.")
              : qsTr("Its launch options or Proton version changed here, and Steam only takes them while it's "
                     + "closed. Restarting closes Steam, applies them and starts the game.")
    }

    footer: [
        SettingsButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            // Steam can't launch a shortcut it doesn't have yet.
            visible: !(root.game.shortcutQueued ?? false)
            text: qsTr("Launch with the old options")
            onClicked: root.run(false)
        },
        SettingsButton {
            id: restartButton
            text: qsTr("Restart Steam and launch")
            primary: true
            onClicked: root.run(true)
        }
    ]
}
