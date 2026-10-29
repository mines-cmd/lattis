import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string version: ""
    property string notes: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.updateWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("What's new in Lattis %1").arg(root.version)
                color: Theme.gameEditor.headingColor
                font.weight: Theme.font.strongWeight
                font.pixelSize: Theme.gameEditor.headingFontSize
                elide: Text.ElideRight
            }
            WindowControls {
                Layout.alignment: Qt.AlignTop
                dialog: root
            }
        }

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

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Close")
                primary: true
                onClicked: root.close()
            }
        }
    }
}
