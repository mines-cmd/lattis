import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    signal message(string text)

    function undo() {
        close()
        const e = Library.undoAllSteamEdits()
        if (e !== "")
            message(e)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.confirmWidth, parent.width - 2 * Theme.gameEditor.padding)
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

    onOpened: cancelButton.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Undo the changes waiting for Steam?")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: qsTr("Steam keeps what it has now, and these games take it back:")
        }
        Repeater {
            model: Library.steamWaiting

            Label {
                required property var modelData
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.labelColor
                text: "•  " + (modelData.adds
                               ? qsTr("%1: not in Steam yet, so it leaves the library").arg(modelData.title)
                               : modelData.title)
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                id: cancelButton
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                text: qsTr("Undo")
                danger: true
                onClicked: root.undo()
            }
        }
    }
}
