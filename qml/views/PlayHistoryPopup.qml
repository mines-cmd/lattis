import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property string gameId
    property string gameTitle
    property var history: ({})
    property url gameIcon
    property real gameHue: 0
    readonly property var sessions: history.sessions ?? []
    readonly property int undoDuration: 15000

    property var pendingUndo: null

    signal message(string text)
    signal undoHandedOver(string text, var link, int duration)

    function openFor(game) {
        gameId = game.id
        gameTitle = game.title
        gameIcon = game.iconUrl ?? ""
        gameHue = game.hue ?? 0
        refresh()
        open()
    }

    KeyedListModel {
        id: sessionModel
        leaveSpeed: "normal"
    }

    function refresh() {
        history = Library.historyFor(gameId)
        const kept = {}
        for (const e of Object.keys(selection)) {
            if (sessions.some(s => s.end === e))
                kept[e] = true
        }
        selection = kept
        sessionModel.scope = gameId
        sessionModel.source = sessions.map(s => Object.assign({ key: s.end }, s))
    }

    function durationText(minutes: int): string {
        if (minutes < 60)
            return qsTr("%1m").arg(minutes)
        const h = Math.floor(minutes / 60)
        const m = minutes % 60
        return m === 0 ? qsTr("%1h").arg(h) : qsTr("%1h %2m").arg(h).arg(m)
    }

    function dayText(date: date): string {
        const today = new Date()
        today.setHours(0, 0, 0, 0)
        const day = new Date(date)
        day.setHours(0, 0, 0, 0)
        const days = Math.round((today - day) / 86400000)
        if (days === 0)
            return qsTr("Today")
        if (days === 1)
            return qsTr("Yesterday")
        return date.toLocaleDateString(Qt.locale(), qsTr("ddd, d MMM"))
    }

    function timeText(date: date): string {
        // Without the system locale the app is en_US, whose 12 h clock isn't wanted.
        if (!General.systemLocale)
            return date.toLocaleTimeString(Qt.locale(), "HH:mm")
        return date.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
    }

    function sessionDay(s): string {
        return dayText(new Date(s.merged ? s.endMs : s.startMs))
    }

    function sessionTime(s): string {
        if (s.merged)
            return qsTr("Played from Steam while Lattis was closed")
        return qsTr("%1 – %2").arg(timeText(new Date(s.startMs))).arg(timeText(new Date(s.endMs)))
    }

    property var selection: ({})
    readonly property int selectionCount: Object.keys(selection).length
    property string anchorEnd: ""

    function clearSelection() {
        if (selectionCount > 0)
            selection = ({})
    }

    function clickSession(index, modifiers) {
        const end = sessions[index].end
        const ctrl = (modifiers & Qt.ControlModifier) !== 0
        const next = ctrl ? Object.assign({}, selection) : {}
        if ((modifiers & Qt.ShiftModifier) !== 0) {
            const from = Math.max(0, sessions.findIndex(s => s.end === anchorEnd))
            for (let i = Math.min(from, index); i <= Math.max(from, index); ++i)
                next[sessions[i].end] = true
        } else if (ctrl && selection[end] === true) {
            delete next[end]
            anchorEnd = end
        } else if (ctrl) {
            next[end] = true
            anchorEnd = end
        } else {
            anchorEnd = end
        }
        selection = next
    }

    function deleteSession(s) {
        deleteSessions([s.end])
    }

    function deleteSelected() {
        deleteSessions(sessions.filter(s => selection[s.end] === true).map(s => s.end))
    }

    function deleteSessions(ends) {
        const id = gameId
        const deleted = []
        let error = ""
        for (const end of ends) {
            const result = Library.deleteSession(id, end)
            if (result.error !== "") {
                error = result.error
                break
            }
            deleted.push(result.session)
        }
        clearSelection()
        refresh()
        if (error !== "")
            tell(error)
        if (deleted.length === 0)
            return
        const link = {
            text: qsTr("Undo"),
            run: () => {
                pendingUndo = null
                let e = ""
                for (const session of deleted) {
                    e = Library.restoreSession(id, session)
                    if (e !== "")
                        break
                }
                if (e !== "")
                    tell(e)
                if (root.gameId === id)
                    root.refresh()
            }
        }
        const text = deleted.length === 1 ? qsTr("Session deleted") : qsTr("%1 sessions deleted").arg(deleted.length)
        pendingUndo = {text: text, link: link, until: Date.now() + undoDuration}
        toast.show(text, link, undoDuration)
    }

    function tell(text) {
        if (opened)
            toast.show(text)
        else
            message(text)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.playHistory.width, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height * Theme.playHistory.maxHeight)
    modal: true
    focus: true
    // The selection goes first on Esc, as in the game list; the popup is one more away.
    closePolicy: selectionCount > 0 ? Popup.CloseOnPressOutside : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Shortcut {
        sequence: "Esc"
        enabled: root.opened && root.selectionCount > 0
        onActivated: root.clearSelection()
    }
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth

        // Over the Close row's free space. The main window's toast sits under this popup's scrim,
        // and one parented to the overlay leaves the window deaf to clicks once the popup closes.
        Toast {
            id: toast
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.gameEditor.padding
            anchors.bottomMargin: Theme.gameEditor.padding
        }
    }

    Icon {
        parent: root.background
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.gameEditor.padding
        path: Icons.close
        size: Theme.about.closeSize
        color: closeMouse.containsMouse ? Theme.about.closeHoverColor : Theme.about.closeColor

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            anchors.margins: -6
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.close()
        }

        ToolTip.visible: closeMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: qsTr("Close")
    }

    onOpened: closeButton.forceActiveFocus()
    onClosed: {
        if (pendingUndo !== null && pendingUndo.until > Date.now())
            undoHandedOver(pendingUndo.text, pendingUndo.link, pendingUndo.until - Date.now())
        pendingUndo = null
        clearSelection()
        toast.opacity = 0
    }

    Connections {
        target: Library
        function onHistoryChanged(ids) {
            if (root.opened && ids.includes(root.gameId))
                root.refresh()
        }
    }

    component Card: Surface {
        radius: Theme.gameEditor.cardRadius
        fill: Theme.gameEditor.cardBackground
        fillEnd: Theme.gameEditor.cardBackgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.cardBorder
        border.width: Theme.gameEditor.cardBorderWidth
    }

    component Figure: ColumnLayout {
        property string label
        property int minutes
        property bool primary: false

        // Equal columns whatever the text: the widest value must not push the last one out of the card.
        Layout.fillWidth: true
        Layout.preferredWidth: 0
        spacing: 2

        CaptionLabel {
            Layout.fillWidth: true
            text: parent.label
            color: Theme.gameEditor.labelColor
            font.pixelSize: Theme.playHistory.headerSize
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: root.durationText(parent.minutes)
            color: parent.primary ? Theme.playHistory.summaryPrimaryColor
                                  : Theme.playHistory.summaryValueColor
            font.pixelSize: Theme.playHistory.summaryValueSize
            font.weight: Theme.font.strongWeight
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.rightMargin: Theme.about.closeSize + Theme.gameEditor.columnSpacing
            spacing: Theme.gameEditor.columnSpacing

            GameIcon {
                source: root.gameIcon
                title: root.gameTitle
                hue: root.gameHue
                size: Theme.playHistory.iconSize
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: root.gameTitle
                    color: Theme.gameEditor.headingColor
                    font.pixelSize: Theme.gameEditor.headingFontSize
                    font.weight: Theme.font.strongWeight
                    elide: Text.ElideRight
                }
                Label {
                    text: qsTr("Play history")
                    color: Theme.gameEditor.labelColor
                }
            }
        }

        Card {
            Layout.fillWidth: true
            implicitHeight: figures.implicitHeight + 2 * Theme.gameEditor.cardPadding

            RowLayout {
                id: figures
                anchors.fill: parent
                anchors.margins: Theme.gameEditor.cardPadding
                spacing: Theme.gameEditor.columnSpacing

                Figure { label: qsTr("Total"); minutes: root.history.total ?? 0; primary: true }
                Figure { label: qsTr("This week"); minutes: root.history.week ?? 0 }
                Figure { label: qsTr("This month"); minutes: root.history.month ?? 0 }
                Figure {
                    visible: (root.history.before ?? 0) > 0
                    label: qsTr("Before history")
                    minutes: root.history.before ?? 0
                }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property real headerSpace: root.sessions.length > 0 ? header.height + Theme.playHistory.headerGap : 0
            Layout.preferredHeight: Math.max(list.contentHeight, 2 * Theme.playHistory.rowHeight) + headerSpace + 2 * Theme.gameEditor.cardPadding
            Layout.minimumHeight: Math.min(Layout.preferredHeight, 2 * Theme.playHistory.rowHeight + headerSpace + 2 * Theme.gameEditor.cardPadding)

            // In an item, not directly in the popup: a menu declared in a Popup leaves the window
            // ignoring clicks once the popup closes.
            SettingsMenu {
                id: sessionMenu
                pops: true
                property var session: ({})

                SettingsMenuItem {
                    text: qsTr("Delete session")
                    iconPath: Icons.deleteBin6Line
                    danger: true
                    onTriggered: root.deleteSession(sessionMenu.session)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: root.sessions.length === 0
                text: qsTr("No sessions yet")
                color: Theme.playHistory.noteColor
            }

            RowLayout {
                id: header
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: Theme.gameEditor.cardPadding
                anchors.leftMargin: Theme.gameEditor.cardPadding + 10
                anchors.rightMargin: Theme.gameEditor.cardPadding + 10
                spacing: Theme.layout.spacing
                visible: root.sessions.length > 0

                component HeaderLabel: CaptionLabel {
                    color: Theme.gameEditor.labelColor
                    font.pixelSize: Theme.playHistory.headerSize
                }

                HeaderLabel {
                    Layout.preferredWidth: Theme.playHistory.dateColumnWidth
                    text: qsTr("Date")
                }
                HeaderLabel {
                    Layout.fillWidth: true
                    text: qsTr("Time")
                }
                HeaderLabel {
                    Layout.preferredWidth: Theme.playHistory.durationColumnWidth
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Length")
                }
                Item { Layout.preferredWidth: Theme.playHistory.deleteSize }
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: Theme.gameEditor.cardPadding
                anchors.topMargin: root.sessions.length > 0
                    ? Theme.gameEditor.cardPadding + header.height + Theme.playHistory.headerGap
                    : Theme.gameEditor.cardPadding
                clip: true
                model: sessionModel
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: row
                    required property var model
                    required property int index
                    readonly property bool leaving: model.leaving
                    readonly property bool selected: root.selection[model.end] === true
                    readonly property var session: ({ end: model.end, startMs: model.startMs, endMs: model.endMs,
                                                      merged: model.merged, minutes: model.minutes })

                    // A session that comes back while the popup is open unfolds, the way a deleted one
                    // folds away; the rows made on opening, or for another game, are there at once.
                    property bool present: !root.opened || sessionModel.rebuilt
                    Component.onCompleted: present = true

                    width: ListView.view.width
                    height: leaving || !present ? 0 : Theme.playHistory.rowHeight
                    opacity: leaving || !present ? 0 : 1
                    clip: true
                    Behavior on height { enabled: Motion.on; MotionNumber { speed: "normal" } }
                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "normal"; linear: true } }

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.playHistory.rowRadius
                        color: row.selected ? Theme.playHistory.rowSelected
                             : rowHover.hovered && !row.leaving ? Theme.playHistory.rowHover : "transparent"
                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                    }

                    Rectangle {
                        visible: row.index + 1 < sessionModel.count
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.playHistory.separator
                    }

                    HoverHandler { id: rowHover }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !row.leaving
                        acceptedButtons: Qt.LeftButton
                        onClicked: mouse => root.clickSession(row.index, mouse.modifiers)
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: eventPoint => {
                            sessionMenu.session = row.session
                            const p = row.mapToItem(list, eventPoint.position)
                            sessionMenu.popup(list, p.x, p.y)
                            sessionMenu.popDirection = Qt.binding(() => sessionMenu.y < p.y ? "up" : "down")
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: Theme.layout.spacing

                        Label {
                            Layout.preferredWidth: Theme.playHistory.dateColumnWidth
                            text: root.sessionDay(row.session)
                            color: Theme.palette.text
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.sessionTime(row.session)
                            color: row.session.merged ? Theme.playHistory.noteColor : Theme.palette.text
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.preferredWidth: Theme.playHistory.durationColumnWidth
                            horizontalAlignment: Text.AlignRight
                            text: row.session.merged ? "" : root.durationText(row.session.minutes)
                            color: Theme.playHistory.durationColor
                        }
                        Item {
                            Layout.preferredWidth: Theme.playHistory.deleteSize
                            Layout.preferredHeight: Theme.playHistory.deleteSize
                            opacity: rowHover.hovered ? 1 : 0
                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                            Icon {
                                anchors.centerIn: parent
                                path: Icons.close
                                size: Theme.playHistory.deleteIconSize
                                color: deleteMouse.containsMouse ? Theme.playHistory.deleteHoverColor : Theme.playHistory.deleteColor
                            }
                            MouseArea {
                                id: deleteMouse
                                anchors.fill: parent
                                enabled: rowHover.hovered
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.deleteSession(row.session)
                            }
                            ToolTip.visible: deleteMouse.containsMouse
                            ToolTip.delay: Theme.tooltip.delay
                            ToolTip.text: qsTr("Delete session")
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(toast.implicitHeight, deleteSelected.implicitHeight)

                SettingsButton {
                    id: deleteSelected
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Delete selected")
                    iconPath: Icons.deleteBin6Line
                    danger: true
                    visible: opacity > 0
                    opacity: root.selectionCount > 0 ? 1 : 0
                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                    onClicked: root.deleteSelected()
                }
            }
            SettingsButton {
                id: closeButton
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
