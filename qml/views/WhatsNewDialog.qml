import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    property string version: ""
    property string notes: ""

    carded: true
    size: "medium"
    title: qsTr("What's new in Lattis %1").arg(version)
    width: Math.min(Theme.gameEditor.updateWidth, parent.width - 2 * Theme.gameEditor.padding)

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.gameEditor.updateNotesHeight
        color: Theme.gameEditor.updateNotesBackground
        radius: Theme.gameEditor.updateNotesRadius
        clip: true

        Flickable {
            id: notesView
            anchors.fill: parent
            anchors.margins: Theme.gameEditor.updateNotesPadding
            contentWidth: width
            contentHeight: notesText.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {}

            Label {
                id: notesText
                width: notesView.width - Theme.layout.spacing * 2
                wrapMode: Text.WordWrap
                textFormat: Text.MarkdownText
                color: Theme.gameEditor.updateNotesText
                text: root.notes !== "" ? root.notes : qsTr("No release notes.")
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }
    }
}
