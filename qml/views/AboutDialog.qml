import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    signal updateRequested()

    carded: false
    size: "medium"
    // The wordmark under the ✕ already names the app.
    title: ""
    width: Math.min(Theme.about.width, parent.width - 2 * Theme.gameEditor.padding)
    leftPadding: Theme.about.sidePadding
    rightPadding: Theme.about.sidePadding
    bottomPadding: 0

    AboutContent {
        Layout.fillWidth: true
        backgroundColor: Theme.gameEditor.plainBackground
        labelColor: Theme.gameEditor.labelColor
        footerInset: root.leftPadding
        onUpdateRequested: {
            root.close()
            root.updateRequested()
        }
    }
}
