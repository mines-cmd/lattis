import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    property string version: ""
    property string notes: ""
    // Replaces the "What's new in Lattis" title, e.g. for emulator updates.
    property string heading: ""
    // [{title, notes}]: when set, each one is shown under its own title instead of `notes`.
    property var sections: []

    carded: true
    size: "medium"
    title: heading !== "" ? heading : qsTr("What's new in Lattis %1").arg(version)
    width: Math.min(Theme.gameEditor.whatsNewWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.round(parent.height * Theme.gameEditor.whatsNewHeightPercent / 100)

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: root.sections.length === 0 ? Theme.gameEditor.updateNotesBackground : "transparent"
        radius: Theme.gameEditor.updateNotesRadius
        clip: true

        Flickable {
            id: notesView
            anchors.fill: parent
            anchors.margins: root.sections.length === 0 ? Theme.gameEditor.updateNotesPadding : 0
            contentWidth: width
            contentHeight: column.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {}

            ColumnLayout {
                id: column
                width: notesView.width - Theme.layout.spacing * 2
                spacing: root.sections.length === 0 ? 0 : Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    visible: root.sections.length === 0
                    wrapMode: Text.WordWrap
                    textFormat: Text.RichText
                    color: Theme.gameEditor.updateNotesText
                    text: AppUpdate.notesHtml(root.notes !== "" ? root.notes : qsTr("No release notes."), Qt.alpha(Theme.gameEditor.updateNotesText, 0.25))
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }

                Repeater {
                    model: root.sections

                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: card.implicitHeight + 2 * Theme.gameEditor.updateNotesPadding
                        color: Theme.gameEditor.updateNotesBackground
                        radius: Theme.gameEditor.updateNotesRadius

                        ColumnLayout {
                            id: card
                            anchors.fill: parent
                            anchors.margins: Theme.gameEditor.updateNotesPadding
                            spacing: Theme.layout.spacing

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.bold: true
                                font.pixelSize: Theme.font.size * 1.8
                                color: Theme.gameEditor.updateNotesText
                                text: modelData.title
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                textFormat: Text.RichText
                                color: Theme.gameEditor.updateNotesText
                                text: AppUpdate.notesHtml(modelData.notes !== "" ? modelData.notes : qsTr("No release notes."), Qt.alpha(Theme.gameEditor.updateNotesText, 0.25))
                                onLinkActivated: link => Qt.openUrlExternally(link)
                            }
                        }
                    }
                }
            }
        }
    }
}
