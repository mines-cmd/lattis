import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    property var media: ({})
    // Set by the Gallery, whose items belong to a game: adds Go to game.
    property string gameId

    signal message(string text)
    signal gameRequested(string id)

    atPointer: true

    SettingsMenuItem {
        text: qsTr("Show in folder")
        onTriggered: {
            if (!Library.showClipFolder(root.media.folder))
                root.message(qsTr("Couldn't open %1").arg(root.media.folder))
        }
    }
    SettingsMenuItem {
        want: root.gameId !== ""
        text: qsTr("Go to game")
        onTriggered: Qt.callLater(() => root.gameRequested(root.gameId))
    }
}
