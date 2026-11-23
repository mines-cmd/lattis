import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var game: ({})
    readonly property bool queued: game.shortcutQueued ?? false

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

    function option(name, text) {
        return "<font color='%1'>%2</font> %3".arg(Theme.gameEditor.headingColor).arg(name).arg(text)
    }

    carded: false
    title: qsTr("Restart Steam to launch?")

    onOpened: restartButton.forceActiveFocus()

    PlainLabel {
        Layout.fillWidth: true
        Layout.topMargin: -Theme.layout.spacingSmall
        text: root.game.title ?? ""
        color: Theme.gameEditor.hintColor
        elide: Text.ElideRight
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: root.queued
              ? qsTr("Its shortcut isn't in Steam yet. Steam only takes it while it's closed.")
              : root.game.platform === "proton"
              ? qsTr("Launch options, the Proton version or the shortcut changed here. Steam only takes them "
                     + "while it's closed.")
              : qsTr("Launch options or the Proton version changed here. Steam only takes them while it's closed.")
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: 0
        implicitHeight: choices.height
        color: "transparent"
        radius: Theme.radius.small
        border.color: Theme.controls.inputBorder
        border.width: Theme.layout.borderWidth

        Column {
            id: choices
            width: parent.width
            spacing: 0

            Label {
                x: 2 * Theme.layout.spacing
                width: choices.width - 4 * Theme.layout.spacing
                topPadding: Theme.layout.spacing
                bottomPadding: Theme.layout.spacing
                wrapMode: Text.WordWrap
                textFormat: Text.StyledText
                color: Theme.gameEditor.hintColor
                text: root.option(qsTr("Restart Steam"),
                                  root.queued ? qsTr("closes it, adds the shortcut, starts the game.")
                                              : qsTr("closes it, applies the changes, starts the game."))
            }

            Rectangle {
                visible: !root.queued
                width: choices.width
                height: Theme.layout.borderWidth
                color: Theme.controls.inputBorder
            }

            Label {
                visible: !root.queued
                x: 2 * Theme.layout.spacing
                width: choices.width - 4 * Theme.layout.spacing
                topPadding: Theme.layout.spacing
                bottomPadding: Theme.layout.spacing
                wrapMode: Text.WordWrap
                textFormat: Text.StyledText
                color: Theme.gameEditor.hintColor
                text: root.option(qsTr("Old options"),
                                  qsTr("keeps Steam open; the changes wait for the next restart."))
            }
        }
    }

    footerStart: [
        SettingsButton {
            // Steam can't launch a shortcut it doesn't have yet.
            visible: !root.queued
            text: qsTr("Launch with old options")
            flat: true
            onClicked: root.run(false)
        }
    ]

    footer: [
        SettingsButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            id: restartButton
            text: qsTr("Restart Steam and launch")
            primary: true
            onClicked: root.run(true)
        }
    ]
}
