import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property var game: ({})

    signal message(string text)

    function ask(g) {
        game = g
        open()
    }

    function closeGame() {
        close()
        const e = GameRunner.forceClose(game.id)
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
            text: qsTr("Force close %1?").arg(root.game.title ?? "")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: qsTr("The game is ended at once, and what it hasn't saved is lost.")
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
                text: qsTr("Force close")
                danger: true
                onClicked: root.closeGame()
            }
        }
    }
}
