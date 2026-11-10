import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    signal updateRequested()

    carded: false
    size: "medium"
    title: qsTr("About Lattis")
    width: Math.min(Theme.about.width, parent.width - 2 * Theme.gameEditor.padding)

    AboutContent {
        Layout.fillWidth: true
        backgroundColor: Theme.gameEditor.plainBackground
        labelColor: Theme.gameEditor.labelColor
        linkColor: Theme.gameEditor.linkColor
        onUpdateRequested: {
            root.close()
            root.updateRequested()
        }
    }
}
