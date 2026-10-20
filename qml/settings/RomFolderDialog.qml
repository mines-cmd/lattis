import QtCore
import QtQuick
import QtQuick.Dialogs

FolderDialog {
    id: root

    signal picked(string path)
    signal closed()

    readonly property string home: decodeURIComponent(new URL(StandardPaths.writableLocation(StandardPaths.HomeLocation)).pathname)

    currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)

    Component.onCompleted: open()
    onAccepted: {
        let path = decodeURIComponent(new URL(selectedFolder).pathname)
        if (path.startsWith(home + "/"))
            path = "~" + path.slice(home.length)
        picked(path)
        closed()
    }
    onRejected: closed()
}
