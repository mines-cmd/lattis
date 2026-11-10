import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    carded: true
    size: "medium"
    title: qsTr("Which game keeps these saves?")

    property string gameId
    property string gameTitle
    property string chosen
    signal message(string text)

    property var results: []

    function pick(name) {
        const error = GameSaves.choose(gameId, name)
        if (error === "")
            close()
        else
            message(error)
    }

    onOpened: {
        input.text = gameTitle
        results = GameSaves.search(input.text)
        input.selectAll()
        input.forceActiveFocus()
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Search the Ludusavi list for the game %1 is, and its save folders are backed up.").arg(root.gameTitle)
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
        wrapMode: Text.WordWrap
    }
    SettingsTextField {
        id: input
        Layout.fillWidth: true
        placeholderText: qsTr("Search by name")
        onTextEdited: root.results = GameSaves.search(text)
        onAccepted: {
            if (root.results.length > 0)
                root.pick(root.results[0])
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 8 * rowHeight)
        readonly property real rowHeight: Theme.controls.buttonHeight
        visible: root.results.length > 0
        clip: true
        model: root.results
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SettingsScrollBar {}

        delegate: Rectangle {
            id: row
            required property string modelData
            width: ListView.view.width
            height: list.rowHeight
            radius: Theme.gameEditor.cardRadius
            color: hover.hovered ? Theme.gameEditor.cardBackground : "transparent"

            Label {
                anchors { fill: parent; leftMargin: Theme.layout.spacing; rightMargin: Theme.layout.spacing }
                verticalAlignment: Text.AlignVCenter
                text: row.modelData
                color: row.modelData === root.chosen ? Theme.gameEditor.headingColor : Theme.gameEditor.labelColor
                elide: Text.ElideRight
            }
            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.pick(row.modelData) }
        }
    }
    Label {
        visible: input.text.trim() !== "" && root.results.length === 0
        Layout.fillWidth: true
        text: GameSaves.state.hasManifest ? qsTr("Nothing in the list is called that.") : qsTr("The Ludusavi list isn't downloaded yet.")
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
    }

    footerStart: [
        SettingsButton {
            visible: root.chosen !== ""
            text: qsTr("Use the automatic match")
            onClicked: root.pick("")
        }
    ]
    footer: [
        SettingsButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    ]
}
