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
        if (!Library.readOnly && Library.steamWaiting.length > 0)
            list.push("steam")
        if (Library.readOnly)
            list.push("readOnly")
        if (importNotice.item?.showing ?? false)
            list.push("import")
        return list
    }
    readonly property int total: ranked.length
    readonly property bool menuOpen: waitingMenu.visible || (importNotice.item?.menuOpen ?? false)
    readonly property bool keepOpen: stackHover.hovered && total >= 2 || menuOpen
    // Set from the hover, closed a moment after it ends; a menu opened from a notice holds it.
    property bool open: false
    readonly property bool fanned: open && total >= 2
    readonly property bool hovered: stackHover.hovered || fanned

    onKeepOpenChanged: {
        if (keepOpen) {
            closeTimer.stop()
            open = true
        } else if (open) {
            closeTimer.restart()
        }
    }
    onTotalChanged: if (total < 2) {
        closeTimer.stop()
        open = false
    }

    Timer {
        id: closeTimer
        interval: Theme.notice.fanCloseDelay
        onTriggered: root.open = false
    }

    function depthOf(key: string): int { return ranked.indexOf(key) }

    signal message(string text)
    signal undoRequested()
    signal gameRequested(string id)

    // The whole deck, the gaps between rows too, so the pointer crossing them keeps it open.
    Item {
        id: stack
        width: root.width
        height: root.total === 0 ? 0
                : root.fanned ? (root.total - 1) * (Theme.layout.toolbarHeight + Theme.layout.spacing) + Theme.layout.toolbarHeight
                : Theme.layout.toolbarHeight + Math.min(root.total - 1, Theme.notice.peekCount) * Theme.notice.peekOffset

        HoverHandler { id: stackHover }

        DeckSlot {
            id: readOnlySlot
            depth: root.depthOf("readOnly")
            fanned: root.fanned
            animated: root.startedUp
            width: root.width
            height: Theme.layout.toolbarHeight

            NoticeBar {
                anchors.fill: parent
                warning: true
                count: root.fanned ? 1 : root.total
                peeking: !root.fanned && readOnlySlot.depth > 0
                text: Library.readOnlyNotice
            }
        }

        // Loaded after the first frame: Backups is made on first use.
        DeckSlot {
            depth: root.depthOf("import")
            fanned: root.fanned
            animated: root.startedUp
            width: root.width
            height: Theme.layout.toolbarHeight

            Loader {
                id: importNotice
                anchors.fill: parent
                active: root.startedUp
                source: "ImportNotice.qml"
                onLoaded: {
                    item.count = Qt.binding(() => root.fanned ? 1 : root.total)
                    item.peeking = Qt.binding(() => !root.fanned && root.depthOf("import") > 0)
                }
            }
        }
        Connections {
            target: importNotice.item
            function onMessage(text) { root.message(text) }
        }

        // Edits waiting for Steam to close, library-wide. Never together with read-only (nothing
        // is saved then), so it ranks first without it.
        DeckSlot {
            id: steamSlot
            depth: root.depthOf("steam")
            fanned: root.fanned
            animated: root.startedUp
            width: root.width
            height: Theme.layout.toolbarHeight

            NoticeBar {
                id: steamNotice
                readonly property int waiting: Library.steamWaiting.length
                anchors.fill: parent
                warning: true
                count: root.fanned ? 1 : root.total
                peeking: !root.fanned && steamSlot.depth > 0
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
                    textColor: Theme.notice.warningActionText
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
                    textColor: Theme.notice.warningActionText
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
