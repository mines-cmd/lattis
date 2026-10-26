import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    property var media: ({})

    signal message(string text)

    atPointer: true

    SettingsMenuItem {
        text: qsTr("Show in folder")
        onTriggered: {
            if (!Library.showClipFolder(root.media.folder))
                root.message(qsTr("Couldn't open %1").arg(root.media.folder))
        }
    }
}
