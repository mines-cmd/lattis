import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    carded: true
    size: "medium"
    title: qsTr("Choose games to link")

    property string gameId
    // Ids of the games already in the group or waiting to join it.
    property var taken: []
    property var chosen: []
    signal picked(var ids)

    function toggle(id, on) {
        chosen = on ? chosen.concat([id]) : chosen.filter(c => c !== id)
    }

    onOpened: {
        chosen = []
        list.clearSearch()
        list.focusSearch("")
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("A copy can have a different title. Search for the games it is the same as.")
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
        wrapMode: Text.WordWrap
    }

    CheckList {
        id: list
        Layout.fillWidth: true
        searchable: true
        customSearch: true
        withSelectAll: query !== ""
        searchPlaceholder: qsTr("Search your library")
        noun: qsTr("%n game(s)", "", model.length)
        emptyText: qsTr("Type a title to find the games")
        maxListHeight: Theme.collections.editListHeight
        ticked: root.chosen
        model: query === "" ? [] : Library.findGames(query, 200)
            .filter(g => g.id !== root.gameId && !root.taken.includes(g.id))
            .map(g => ({ id: g.id, title: g.title, info: g.platformName }))
        onToggled: id => root.toggle(id, !root.chosen.includes(id))
        onToggledMany: (ids, on) => ids.forEach(id => root.toggle(id, on))
    }

    footer: [
        SettingsButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            primary: true
            enabled: root.chosen.length > 0
            text: root.chosen.length > 1 ? qsTr("Link %n games", "", root.chosen.length) : qsTr("Link")
            onClicked: {
                root.picked(root.chosen)
                root.close()
            }
        }
    ]
}
