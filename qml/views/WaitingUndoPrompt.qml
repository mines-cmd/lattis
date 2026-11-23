import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    readonly property bool steam: Library.steamWaiting.length > 0
    readonly property bool heroic: Library.heroicWaiting.length > 0
    readonly property var rows: Library.steamWaiting.map(w => ({
            id: "steam:" + w.id,
            title: w.title,
            info: w.adds ? qsTr("Steam · leaves the library") : (w.changes ? qsTr("Steam · %1").arg(w.changes) : qsTr("Steam"))
        })).concat(Library.heroicWaiting.map(w => ({
            id: "heroic:" + w.id,
            title: w.title,
            info: w.changes ? qsTr("Heroic · %1").arg(w.changes) : qsTr("Heroic")
        })))
    readonly property bool many: rows.length > 1
    property var ticked: []

    signal message(string text)

    function toggle(id, on) {
        const rest = ticked.filter(t => t !== id)
        ticked = on ? rest.concat([id]) : rest
    }

    function undo() {
        close()
        const chosen = many ? ticked : rows.map(r => r.id)
        const e = Library.undoWaitingEdits(chosen.filter(t => t.startsWith("steam:")).map(t => t.slice(6)),
                                           chosen.filter(t => t.startsWith("heroic:")).map(t => t.slice(7)))
        if (e !== "")
            message(e)
    }

    carded: false
    title: steam && heroic ? qsTr("Undo the waiting changes?")
         : heroic ? qsTr("Undo the changes waiting for Heroic?")
         : qsTr("Undo the changes waiting for Steam?")

    onAboutToShow: ticked = rows.map(r => r.id)
    onOpened: cancelButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: root.many
              ? (root.steam && root.heroic ? qsTr("The ticked games drop their waiting changes, and Steam and Heroic keep what they have now.")
                 : root.heroic ? qsTr("The ticked games drop their waiting changes, and Heroic keeps the settings it has.")
                 : qsTr("Steam keeps what it has now, and the ticked games take it back."))
              : root.heroic ? qsTr("Heroic keeps its settings for this game, and its waiting changes are dropped:")
              : qsTr("Steam keeps what it has now, and this game takes it back:")
    }

    CheckList {
        visible: root.many
        Layout.fillWidth: true
        withSelectAll: true
        noun: qsTr("%n game(s)", "", root.rows.length)
        model: root.rows
        ticked: root.ticked
        onToggled: id => root.toggle(id, !root.ticked.includes(id))
        onToggledMany: (ids, on) => {
            const rest = root.ticked.filter(t => !ids.includes(t))
            root.ticked = on ? rest.concat(ids) : rest
        }
    }

    Repeater {
        model: root.many ? [] : root.rows

        PlainLabel {
            required property var modelData
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: "•  " + qsTr("%1 (%2)").arg(modelData.title).arg(modelData.info)
        }
    }

    footer: [
        SettingsButton {
            id: cancelButton
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            text: qsTr("Undo")
            danger: true
            enabled: !root.many || root.ticked.length > 0
            onClicked: root.undo()
        }
    ]
}
