import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    readonly property string state: AppUpdate.state
    readonly property bool downloading: state === "downloading"
    readonly property bool ready: state === "ready"
    readonly property bool failed: state === "failed"
    readonly property string sizeText: qsTr("%1 MB").arg(Math.max(1, Math.round(AppUpdate.size / 1e6)))

    signal message(string text)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.updateWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    function skipVersion() {
        AppUpdate.skip()
        close()
    }

    function restartNow() {
        AppUpdate.restart()
        if (AppUpdate.error !== "")
            message(AppUpdate.error)
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: root.ready ? qsTr("Lattis %1 is ready").arg(AppUpdate.latestVersion)
                    : qsTr("Update to Lattis %1").arg(AppUpdate.latestVersion)
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

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: root.ready ? qsTr("It replaced the file you started Lattis from. Restart to use it.")
                : root.downloading ? qsTr("Downloading %1 of %2").arg(Math.max(1, Math.round(AppUpdate.size * Math.max(0, AppUpdate.progress) / 1e6)))
                                       .arg(root.sizeText)
                : qsTr("%1 · you have %2").arg(root.sizeText).arg(Qt.application.version)
        }

        Rectangle {
            visible: !root.downloading && !root.ready
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.gameEditor.updateNotesHeight
            color: Theme.gameEditor.updateNotesBackground
            radius: Theme.gameEditor.updateNotesRadius
            clip: true

            Flickable {
                id: notes
                anchors.fill: parent
                anchors.margins: Theme.gameEditor.updateNotesPadding
                contentWidth: width
                contentHeight: notesText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: SettingsScrollBar {}

                Label {
                    id: notesText
                    width: notes.width - Theme.layout.spacing * 2
                    wrapMode: Text.WordWrap
                    textFormat: Text.MarkdownText
                    color: Theme.gameEditor.updateNotesText
                    text: AppUpdate.notes !== "" ? AppUpdate.notes : qsTr("No release notes.")
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
        }

        InstallProgress {
            visible: root.downloading
            Layout.fillWidth: true
            progress: AppUpdate.progress
        }

        Label {
            visible: root.failed
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.errorColor
            text: AppUpdate.error
        }

        Label {
            visible: !AppUpdate.canReplace && !root.ready
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: AppUpdate.replaceBlocker
        }

        Label {
            visible: root.ready && AppUpdate.runningGame !== ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: qsTr("Close %1 first.").arg(AppUpdate.runningGame)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                visible: !root.downloading && !root.ready
                text: qsTr("Skip this version")
                onClicked: root.skipVersion()
            }
            SettingsButton {
                visible: !root.downloading
                text: qsTr("Later")
                onClicked: root.close()
            }
            SettingsButton {
                visible: root.downloading
                text: qsTr("Cancel")
                onClicked: AppUpdate.cancel()
            }
            SettingsButton {
                visible: !root.downloading && !root.ready && !AppUpdate.canReplace
                text: qsTr("Open release page")
                primary: true
                onClicked: Qt.openUrlExternally(AppUpdate.releasePage)
            }
            SettingsButton {
                visible: !root.downloading && !root.ready && AppUpdate.canReplace
                text: root.failed ? qsTr("Try again") : qsTr("Update")
                primary: true
                onClicked: AppUpdate.update()
            }
            SettingsButton {
                visible: root.ready
                text: qsTr("Restart now")
                primary: true
                enabled: AppUpdate.runningGame === ""
                onClicked: root.restartNow()
            }
        }
    }
}
