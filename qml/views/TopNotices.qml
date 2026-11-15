import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property bool startedUp: false
    readonly property bool hovered: readOnlyNotice.hovered || (importNotice.item?.hovered ?? false)
        || (steamNotice.visible && steamNotice.hovered)

    signal message(string text)
    signal undoRequested()
    signal gameRequested(string id)

    NoticeBar {
        id: readOnlyNotice
        visible: Library.readOnly
        opacity: visible ? 1 : 0
        width: root.width
        height: Theme.layout.toolbarHeight
        text: Library.readOnlyNotice

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }
    }

    // After an Import, below that one when both show. Loaded after the first frame: Backups is
    // made on first use.
    Loader {
        id: importNotice
        active: root.startedUp
        y: Library.readOnly ? Theme.layout.toolbarHeight + Theme.layout.spacing : 0
        width: root.width
        height: Theme.layout.toolbarHeight
        source: "ImportNotice.qml"
    }
    Connections {
        target: importNotice.item
        function onMessage(text) { root.message(text) }
    }

    // Edits waiting for Steam to close, library-wide; below the other two when they show.
    NoticeBar {
        id: steamNotice
        readonly property int waiting: Library.steamWaiting.length
        visible: !Library.readOnly && waiting > 0
        opacity: visible ? 1 : 0
        y: (Library.readOnly ? Theme.layout.toolbarHeight + Theme.layout.spacing : 0)
           + (importNotice.item?.showing ? Theme.layout.toolbarHeight + Theme.layout.spacing : 0)
        width: root.width
        height: Theme.layout.toolbarHeight
        text: waiting === 1
              ? qsTr("Waiting for Steam to close: %1's %2")
                    .arg(Library.steamWaiting[0].title).arg(Library.steamWaiting[0].changes || qsTr("changes"))
              : qsTr("Waiting for Steam to close: %1")
                    .arg(Library.steamWaiting.map(w => w.title).join(", "))

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }

        SettingsButton {
            id: showButton
            Layout.preferredHeight: Theme.notice.buttonHeight
            text: qsTr("Show")
            flat: true
            leftPadding: Theme.layout.spacing
            rightPadding: Theme.layout.spacing
            textColor: Theme.notice.actionText
            font.weight: Theme.font.emphasisWeight
            tip: steamNotice.waiting === 1 ? qsTr("Go to the game") : qsTr("Pick a game to go to, with what changed")
            onClicked: {
                if (steamNotice.waiting === 1)
                    root.gameRequested(Library.steamWaiting[0].id)
                else
                    waitingMenu.popup(showButton.parent, showButton.x, showButton.y + showButton.height)
            }
        }
        SettingsButton {
            Layout.preferredHeight: Theme.notice.buttonHeight
            text: GameRunner.closingSteam ? qsTr("Closing Steam") : qsTr("Close Steam")
            flat: true
            leftPadding: Theme.layout.spacing
            rightPadding: Theme.layout.spacing
            textColor: Theme.notice.actionText
            font.weight: Theme.font.emphasisWeight
            enabled: !GameRunner.closingSteam
            tip: qsTr("Closes Steam, writes these changes to it, and starts Steam again")
            onClicked: {
                const e = GameRunner.closeSteamAndApply()
                if (e !== "")
                    root.message(e)
            }
        }
        SettingsButton {
            Layout.preferredHeight: Theme.notice.buttonHeight
            text: qsTr("Undo")
            flat: true
            leftPadding: Theme.layout.spacing
            rightPadding: Theme.layout.spacing
            textColor: Theme.notice.dangerText
            font.weight: Theme.font.emphasisWeight
            tip: qsTr("Drops these changes and keeps what Steam has")
            onClicked: root.undoRequested()
        }
    }

    SettingsMenu {
        id: waitingMenu

        Instantiator {
            model: Library.steamWaiting
            delegate: SettingsMenuItem {
                required property var modelData
                text: modelData.changes !== "" ? qsTr("%1: %2").arg(modelData.title).arg(modelData.changes) : modelData.title
                onTriggered: root.gameRequested(modelData.id)
            }
            onObjectAdded: (index, object) => waitingMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => waitingMenu.removeItem(object)
        }
    }
}
