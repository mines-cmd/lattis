import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    signal message(string text)
    signal undoRequested()

    Action {
        text: qsTr("Apply")
        onTriggered: {
            const e = GameRunner.closeSteamAndApply()
            if (e !== "")
                message(e)
        }
    }
    Action {
        text: qsTr("Undo")
        onTriggered: undoRequested()
    }
}
