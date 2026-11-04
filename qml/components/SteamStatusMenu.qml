import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    signal message(string text)

    pops: true
    checkColumn: true

    function report(error) {
        if (error !== "")
            message(error)
    }

    component StatusRow: MenuCheckRow {
        property string name
        // What Steam says, not what was clicked: the check moves when the file changes.
        checked: SteamStatus.status === name
        enabled: SteamStatus.running
        opacity: enabled ? 1 : 0.45
        onToggled: {
            root.close()
            root.report(SteamStatus.setStatus(name))
        }
    }

    StatusRow { name: "online"; text: qsTr("Online") }
    StatusRow { name: "away"; text: qsTr("Away") }
    StatusRow { name: "invisible"; text: qsTr("Invisible") }
    SettingsMenuSeparator {}
    SettingsMenuItem {
        text: qsTr("Open friends list")
        onTriggered: root.report(SteamStatus.openFriends())
    }
}
