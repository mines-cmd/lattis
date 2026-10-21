import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property var ids: []
    signal message(string text)

    readonly property string typed: input.text.trim()

    function add() {
        if (typed === "")
            return
        let error = Library.tags.some(t => t.name === typed) ? ""
            : Library.addTag(typed, Library.nextTagColor(Theme.tags.palette.length))
        if (error === "")
            error = Library.retagGames(ids, [typed], [])
        close()
        if (error !== "")
            message(error)
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

    onOpened: {
        input.text = ""
        input.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("New tag for %1 games").arg(root.ids.length)
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }
        SettingsTextField {
            id: input
            Layout.fillWidth: true
            placeholderText: qsTr("Name of the new tag")
            onAccepted: root.add()
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                text: qsTr("Add")
                enabled: root.typed !== ""
                onClicked: root.add()
            }
        }
    }
}
