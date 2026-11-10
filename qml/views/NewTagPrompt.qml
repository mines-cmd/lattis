import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
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

    carded: false
    title: qsTr("New tag for %1 games").arg(ids.length)

    onOpened: {
        input.text = ""
        input.forceActiveFocus()
    }

    SettingsTextField {
        id: input
        Layout.fillWidth: true
        placeholderText: qsTr("Name of the new tag")
        onAccepted: root.add()
    }

    footer: [
        SettingsButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            primary: true
            text: qsTr("Add")
            enabled: root.typed !== ""
            onClicked: root.add()
        }
    ]
}
