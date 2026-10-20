import QtCore
import QtQuick
import QtQuick.Dialogs

FileDialog {
    id: root

    signal picked(string file)
    signal closed()

    fileMode: FileDialog.OpenFile
    nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.PNG *.JPG *.JPEG *.WEBP)")]
    currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)

    Component.onCompleted: open()
    onAccepted: {
        picked(selectedFile.toString())
        closed()
    }
    onRejected: closed()
}
