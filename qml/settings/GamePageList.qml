import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    // "chips", "buttons" or "info_rows".
    required property string place
    // name → { name, icon, hint }
    required property var entries
    property string query: ""
    property Flickable flickable: null

    signal failed(string text)

    // Every choice, hidden ones included, in the page's order. Replaced only when the order changes, so
    // a switch doesn't rebuild the rows.
    property var order: General.gamePageOrder(place)
    Connections {
        target: General
        function onGamePageChanged() {
            const now = General.gamePageOrder(root.place)
            if (now.join(",") !== root.order.join(","))
                root.order = now
        }
    }
    readonly property var shown: place === "chips" ? General.gamePageChips
                               : place === "buttons" ? General.gamePageButtons : General.gamePageInfoRows

    readonly property bool searching: query !== ""
    function matches(name) {
        const entry = entries[name]
        return !searching || [entry.name, entry.hint].some(t => t.toLowerCase().includes(query))
    }
    readonly property var matching: order.filter(matches)

    function dragForScreenshot(name, offset) {
        const from = order.indexOf(name)
        if (from >= 0)
            drag.dragForScreenshot(from, offset)
    }

    visible: matching.length > 0
    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + Theme.settingsWindow.cardPadding * 2
    radius: Theme.settingsWindow.cardRadius
    color: Theme.settingsWindow.cardBackground
    border.color: Theme.settingsWindow.cardBorder
    border.width: Theme.settingsWindow.cardBorderWidth

    DragReorder {
        id: drag
        column: column
        repeater: rows
        ids: root.order
        flickable: root.flickable
        // The row settles first and the order is written after, as writing reloads the list at once.
        onDropped: drag.settle()
        onSettled: (id, from, to) => {
            const err = General.setGamePageOrder(root.place, drag.movedIds(from, to))
            if (err !== "")
                root.failed(err)
        }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.cardPadding
        spacing: 0

        Repeater {
            id: rows
            model: root.order

            ColumnLayout {
                id: entry
                required property string modelData
                required property int index
                readonly property var info: root.entries[modelData]
                readonly property bool on: root.shown.includes(modelData)
                readonly property Item handle: grip.area
                // Not at creation: a reloaded list's new rows first see the drag's offsets and drop them in the same event.
                property bool ready: false
                Component.onCompleted: Qt.callLater(() => { ready = true })

                visible: root.matches(modelData)
                Layout.fillWidth: true
                spacing: 0
                opacity: drag.follows(index) ? Theme.settingsWindow.dragOpacity : 1
                z: drag.isDragged(index) ? 1 : 0
                transform: Translate {
                    y: drag.offsetOf(entry.index)
                    Behavior on y {
                        enabled: Motion.on && entry.ready && !drag.follows(entry.index)
                        MotionNumber {}
                    }
                }

                Rectangle {
                    visible: Theme.settingsWindow.rowDividers && entry.modelData !== root.matching[0]
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.settingsWindow.rowPadding
                    Layout.rightMargin: Theme.settingsWindow.rowPadding
                    implicitHeight: 1
                    color: Theme.settingsWindow.rowDivider
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Theme.settingsWindow.rowPadding
                    spacing: 10

                    DragGrip {
                        id: grip
                        draggable: !root.searching
                        opacity: draggable ? 1 : 0.4
                        tip: draggable ? qsTr("Drag to change its place") : qsTr("Clear the search to drag")
                        onDragStarted: (handle, x, y) => drag.start(handle, entry.index, x, y)
                        onDragMoved: (handle, x, y) => drag.move(handle, x, y)
                        onDragEnded: dropped => drag.end(dropped)
                    }

                    Icon {
                        path: entry.info.icon
                        size: Theme.controls.iconSize
                        color: Theme.controls.labelColor
                        opacity: entry.on ? 1 : Theme.settingsWindow.offEntryOpacity

                        Behavior on opacity { enabled: Motion.fades; MotionNumber {} }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        opacity: entry.on ? 1 : Theme.settingsWindow.offEntryOpacity

                        Behavior on opacity { enabled: Motion.fades; MotionNumber {} }

                        Label {
                            Layout.fillWidth: true
                            text: entry.info.name
                            color: Theme.controls.labelColor
                            font.pixelSize: Theme.controls.labelFontSize
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: entry.info.hint
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                            wrapMode: Text.WordWrap
                        }
                    }

                    SettingsSwitch {
                        checked: entry.on
                        onToggled: {
                            const err = General.setGamePageShown(root.place, entry.modelData, checked)
                            if (err !== "")
                                root.failed(err)
                        }
                    }
                }
            }
        }
    }
}
