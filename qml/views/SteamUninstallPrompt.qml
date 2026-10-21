import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property var game: ({})
    property bool scrim: false

    signal failed(string text)
    signal done()

    function ask(g) {
        game = g
        open()
    }

    function uninstall() {
        close()
        const e = GameRunner.uninstall(game.id)
        if (e !== "")
            failed(e)
        else
            done()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.confirmWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim { color: root.scrim ? Theme.gameEditor.scrim : "transparent" }
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
            text: qsTr("Uninstall %1?").arg(root.game.title ?? "")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: qsTr("Steam will ask to confirm and remove the files. The game stays in your library as "
                       + "not installed, with its play time, tags and art.")
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
                text: qsTr("Uninstall")
                danger: true
                onClicked: root.uninstall()
            }
        }
    }
}
