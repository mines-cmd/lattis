import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
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

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Math.max(Theme.gameEditor.confirmWidth, buttons.implicitWidth + leftPadding + rightPadding),
                    parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    onOpened: restartButton.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Restart Steam to launch %1 with its changes?").arg(root.game.title ?? "")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }
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
        RowLayout {
            id: buttons
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                // Steam can't launch a shortcut it doesn't have yet.
                visible: !(root.game.shortcutQueued ?? false)
                text: qsTr("Launch with the old options")
                onClicked: root.run(false)
            }
            SettingsButton {
                id: restartButton
                text: qsTr("Restart Steam and launch")
                primary: true
                onClicked: root.run(true)
            }
        }
    }
}
