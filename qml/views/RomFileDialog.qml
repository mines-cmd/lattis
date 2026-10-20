import QtCore
import QtQuick
import QtQuick.Dialogs

FileDialog {
    id: root

    signal picked(string file)
    signal closed()

    title: qsTr("Add a game")
    fileMode: FileDialog.OpenFile
    nameFilters: Library.romNameFilters()
    currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)

    Component.onCompleted: open()
    onAccepted: {
        picked(selectedFile.toString())
        closed()
    }
    onRejected: closed()
}
