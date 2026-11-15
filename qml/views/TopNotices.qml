import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property bool startedUp: false

    // Most urgent first; a notice joins the deck by taking a place in this list.
    readonly property var ranked: {
        const list = []
        if (steamNotice.wanted)
            list.push("steam")
        if (readOnlyNotice.wanted)
            list.push("readOnly")
        if (importNotice.item?.showing ?? false)
            list.push("import")
        return list
    }
    readonly property bool hovered: (readOnlyNotice.depth >= 0 && readOnlyNotice.hovered)
        || (importNotice.item?.hovered ?? false) || (steamNotice.depth >= 0 && steamNotice.hovered)

    function depthOf(key: string): int { return ranked.indexOf(key) }
    function peekOpacity(depth: int): real { return depth <= 0 ? 1 : Math.pow(Theme.notice.peekOpacity, depth) }
    function peekScale(depth: int): real { return depth <= 0 ? 1 : Math.pow(Theme.notice.peekScale, depth) }
    function peekShown(depth: int): bool { return depth >= 0 && depth <= Theme.notice.peekCount }

    signal message(string text)
    signal undoRequested()
    signal gameRequested(string id)

    NoticeBar {
        id: readOnlyNotice
        readonly property bool wanted: Library.readOnly
        readonly property int depth: root.depthOf("readOnly")
        warning: true
        count: root.ranked.length
        peeking: depth > 0
        visible: root.peekShown(depth)
        z: -depth
        opacity: root.peekOpacity(depth)
        y: Math.max(0, depth) * Theme.notice.peekOffset
        scale: root.peekScale(depth)
        transformOrigin: Item.Top
        width: root.width
        height: Theme.layout.toolbarHeight
        text: Library.readOnlyNotice

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }
    }

    // Loaded after the first frame: Backups is made on first use.
    Loader {
        id: importNotice
        readonly property int depth: root.depthOf("import")
        active: root.startedUp
        visible: depth < 0 ? (item?.progress ?? 0) > 0 : root.peekShown(depth)
        z: -Math.max(0, depth)
        opacity: root.peekOpacity(depth)
        y: Math.max(0, depth) * Theme.notice.peekOffset
        scale: root.peekScale(depth)
        transformOrigin: Item.Top
        width: root.width
        height: Theme.layout.toolbarHeight
        source: "ImportNotice.qml"
        onLoaded: {
            item.count = Qt.binding(() => root.ranked.length)
            item.peeking = Qt.binding(() => depth > 0)
        }
    }
    Connections {
        target: importNotice.item
        function onMessage(text) { root.message(text) }
    }

    // Edits waiting for Steam to close, library-wide. Never together with read-only (nothing
    // is saved then), so it ranks first without it.
    NoticeBar {
        id: steamNotice
        readonly property int waiting: Library.steamWaiting.length
        readonly property bool wanted: !Library.readOnly && waiting > 0
        readonly property int depth: root.depthOf("steam")
        warning: true
        count: root.ranked.length
        peeking: depth > 0
        visible: root.peekShown(depth)
        z: -depth
        opacity: root.peekOpacity(depth)
        y: Math.max(0, depth) * Theme.notice.peekOffset
        scale: root.peekScale(depth)
        transformOrigin: Item.Top
        width: root.width
        height: Theme.layout.toolbarHeight
        text: waiting === 1
              ? qsTr("Waiting for Steam to close: %1's %2")
                    .arg(Library.steamWaiting[0].title).arg(Library.steamWaiting[0].changes || qsTr("changes"))
              : qsTr("Waiting for Steam to close: %1")
                    .arg(Library.steamWaiting.map(w => w.title).join(", "))

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
