import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("In rail order. A platform shows in the rail once it has games.")

    readonly property var platforms: Library.platformSettings
    readonly property var defined: platforms.filter(p => p.defined)
    readonly property var problems: Library.problems.filter(p => p.includes("platforms.toml"))
    property string expandedId: ""
    property string movedId: ""
    property bool movedTo: false

    readonly property string query: search.text.trim().toLowerCase()
    readonly property var shown: platforms.filter(
        p => query === "" || [p.name, p.id, p.emulatorNames.join(" ")].some(t => t.toLowerCase().includes(query)))

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }
    function clearSearch() {
        if (search.text === "")
            return false
        search.clear()
        return true
    }

    signal message(string text)

    function reveal(id) {
        expandedId = id
    }

    function move(id, step, typed) {
        movedId = id
        movedTo = typed === true
        error.text = Window.window.change(() => Library.movePlatform(id, step))
    }

    onPlatformsChanged: Qt.callLater(() => { movedId = "" })

    // A row dragged by its handle. dragFrom and dragTo are its place among the defined platforms before
    // and after, and the move is written when it's dropped. Only without a search, when the rows shown
    // are all of them (the defined ones first, as in the file).
    property string dragId: ""
    property int dragFrom: -1
    property int dragTo: -1
    property real dragPressY: 0
    property real dragDy: 0
    property bool dragSettling: false
    // The order when the drag began: once the list reloads in the new order the drag is over, so the rebuilt
    // rows start in place instead of animating from where the drag had put them.
    property string dragOrder: ""
    readonly property bool dragging: dragId !== "" && shown.map(p => p.id).join(",") === dragOrder

    function entryAt(i) {
        return entryRepeater.itemAt(i)
    }

    function shiftOf(index) {
        if (!dragging || index === dragFrom)
            return 0
        const height = entryAt(dragFrom).height
        if (dragTo > dragFrom && index > dragFrom && index <= dragTo)
            return -height
        if (dragTo < dragFrom && index >= dragTo && index < dragFrom)
            return height
        return 0
    }

    function settledOffset() {
        let offset = 0
        if (dragTo > dragFrom) {
            for (let i = dragFrom + 1; i <= dragTo; ++i)
                offset += entryAt(i).height
        } else {
            for (let i = dragTo; i < dragFrom; ++i)
                offset -= entryAt(i).height
        }
        return offset
    }

    function startDrag(handle, index, x, y) {
        dragId = shown[index].id
        dragOrder = shown.map(p => p.id).join(",")
        dragFrom = index
        dragTo = index
        dragPressY = handle.mapToItem(cardColumn, x, y).y
        dragDy = 0
        dragSettling = false
    }

    function moveDrag(handle, x, y) {
        if (!dragging || dragSettling)
            return
        const row = entryAt(dragFrom)
        const first = entryAt(0)
        const last = entryAt(defined.length - 1)
        const dy = handle.mapToItem(cardColumn, x, y).y - dragPressY
        dragDy = Math.max(first.y - row.y, Math.min(last.y + last.height - row.y - row.height, dy))
        const center = row.y + row.height / 2 + dragDy
        let to = 0
        for (let i = 0; i < defined.length; ++i) {
            if (i !== dragFrom && entryAt(i).y + entryAt(i).height / 2 < center)
                ++to
        }
        dragTo = to
    }

    function endDrag(dropped) {
        if (!dragging || dragSettling)
            return
        if (!dropped || dragTo === dragFrom) {
            resetDrag()
            return
        }
        const others = defined.filter((p, i) => i !== dragFrom)
        const failed = Window.window.change(() => Library.movePlatformBefore(dragId, others[dragTo]?.id ?? ""))
        if (failed !== "") {
            resetDrag()
            error.text = failed
            return
        }
        dragSettling = true
        settleTimer.restart()
    }

    function resetDrag() {
        settleTimer.stop()
        dragId = ""
        dragFrom = -1
        dragTo = -1
        dragDy = 0
        dragSettling = false
    }

    function dragForScreenshot(id, offset) {
        const from = defined.findIndex(p => p.id === id)
        if (from < 0)
            return
        const handle = entryAt(from).handle
        startDrag(handle, from, 0, 0)
        dragPressY = handle.mapToItem(cardColumn, 0, 0).y
        moveDrag(handle, 0, offset)
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.dragging && !root.dragSettling
        onActivated: root.resetDrag()
    }

    Connections {
        target: Library
        function onPlatformSettingsChanged() { root.resetDrag() }
    }

    Timer {
        id: settleTimer
        interval: 800
        onTriggered: root.resetDrag()
    }

    component DialogBackground: Rectangle {
        color: Theme.menu.popupBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth
        radius: Theme.controls.inputRadius
    }

    MotionPopup {
        id: addDialog

        readonly property string id: Library.platformIdFor(nameField.text)
        readonly property var existing: root.platforms.find(p => p.id === id)
        readonly property bool canAdd: nameField.text.trim() !== "" && id !== "" && !(existing?.defined ?? false)

        function openFresh() {
            nameField.text = ""
            addError.text = ""
            open()
            nameField.forceActiveFocus()
        }
        function add() {
            if (!canAdd)
                return
            const id = addDialog.id
            addError.text = Library.addPlatform(nameField.text)
            if (addError.text === "") {
                root.expandedId = id
                close()
            }
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.menu.dialogPadding
        width: 380
        background: DialogBackground {}

        ColumnLayout {
            width: parent.width
            spacing: 10

            Label { text: qsTr("Add platform"); color: Theme.controls.labelColor; font.bold: true }

            Label { text: qsTr("Name"); color: Theme.controls.descriptionColor }
            SettingsTextField {
                id: nameField
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                placeholderText: qsTr("e.g. GameCube")
                onAccepted: addDialog.add()
            }

            Label {
                Layout.fillWidth: true
                text: nameField.text.trim() === "" ? qsTr("Games refer to their platform by an id made from its name.")
                    : addDialog.id === "" ? qsTr("The id is made from the letters a-z and digits in the name; this one has none.")
                    : addDialog.existing?.defined ? qsTr("There's already a platform with the id %1.").arg(addDialog.id)
                    : addDialog.existing ? qsTr("Id: %1. %2 already use it, and will show on this platform.")
                                               .arg(addDialog.id)
                                               .arg(addDialog.existing.games === 1 ? qsTr("1 game") : qsTr("%1 games").arg(addDialog.existing.games))
                    : qsTr("Id: %1. Games refer to their platform by id, so it can't change later; the name can.").arg(addDialog.id)
                color: addDialog.canAdd || nameField.text.trim() === "" ? Theme.controls.descriptionColor
                                                                        : Theme.controls.problemsText
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            Label {
                id: addError
                visible: text !== ""
                Layout.fillWidth: true
                color: Theme.controls.problemsText
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.controls.controlSpacing
                SettingsButton { text: qsTr("Cancel"); onClicked: addDialog.close() }
                SettingsButton {
                    primary: true
                    text: qsTr("Add")
                    enabled: addDialog.canAdd
                    onClicked: addDialog.add()
                }
            }
        }
    }

    MotionPopup {
        id: removeDialog

        property var platform: ({ id: "", name: "", games: 0, romFolders: [] })

        function openFor(platform) {
            removeDialog.platform = platform
            removeError.text = ""
            open()
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.menu.dialogPadding
        width: 380
        background: DialogBackground {}

        ColumnLayout {
            width: parent.width
            spacing: 10

            Label {
                text: qsTr("Remove %1?").arg(removeDialog.platform.name)
                color: Theme.controls.labelColor
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: (removeDialog.platform.games > 0
                       ? qsTr("Its games stay in the library, with their favorites and play time, under the id %1. Add a platform with that id to show them in the rail again.")
                             .arg(removeDialog.platform.id)
                       : qsTr("It has no games."))
                      + (removeDialog.platform.romFolders.length > 0 ? " " + qsTr("Its ROM folders won't be scanned.") : "")
                color: Theme.controls.descriptionColor
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            Label {
                id: removeError
                visible: text !== ""
                Layout.fillWidth: true
                color: Theme.controls.problemsText
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.controls.controlSpacing
                SettingsButton { text: qsTr("Cancel"); onClicked: removeDialog.close() }
                SettingsButton {
                    danger: true
                    text: qsTr("Remove")
                    onClicked: {
                        removeError.text = Library.removePlatform(removeDialog.platform.id)
                        if (removeError.text === "")
                            removeDialog.close()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
            spacing: 10

            SettingsSearchField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search platforms")
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                text: qsTr("Add platform")
                iconPath: Icons.add
                onClicked: addDialog.openFresh()
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                // Any file change refreshes the library, which counts as scanning; the label only
                // says so for one that lasts, so an edit here doesn't make it flicker.
                id: scanButton
                property bool slow: false
                text: slow ? qsTr("Scanning") : qsTr("Scan now")
                enabled: !slow
                Timer {
                    interval: 400
                    running: Library.scanning
                    onTriggered: scanButton.slow = true
                }
                Connections {
                    target: Library
                    function onScanningChanged() { if (!Library.scanning) scanButton.slow = false }
                }
                tip: qsTr("Look for new and missing games in the ROM folders")
                onClicked: Library.scanNow()
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                rightPadding: leftPadding
                tip: qsTr("Open platforms.toml")
                iconPath: Icons.openExternal
                onClicked: {
                    const err = GameRunner.openFile(Library.platformsFile())
                    if (err !== "")
                        message(qsTr("Could not open — path: %1").arg(err))
                }
            }

            ProblemsButton {
                problems: root.problems
            }
        }

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            color: Theme.controls.problemsText
            font.pixelSize: Theme.controls.descriptionFontSize
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: content
                width: flick.width - scrollBar.width - 8
                spacing: 0

                Label {
                    visible: root.platforms.length === 0
                    Layout.fillWidth: true
                    text: qsTr("No platforms yet. Add one, then give it its emulators and the folders its ROMs are in.")
                    color: Theme.controls.descriptionColor
                    wrapMode: Text.WordWrap
                }

                Label {
                    visible: root.platforms.length > 0 && root.shown.length === 0
                    Layout.fillWidth: true
                    text: qsTr("No platforms match \"%1\".").arg(search.text.trim())
                    color: Theme.controls.descriptionColor
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    visible: root.shown.length > 0
                    Layout.fillWidth: true
                    implicitHeight: cardColumn.implicitHeight + Theme.settingsWindow.cardPadding * 2
                    radius: Theme.settingsWindow.cardRadius
                    color: Theme.settingsWindow.cardBackground
                    border.color: Theme.settingsWindow.cardBorder
                    border.width: Theme.settingsWindow.cardBorderWidth

                    Rectangle {
                        visible: root.dragging
                        x: cardColumn.x
                        y: cardColumn.y + (root.dragging ? (root.dragTo > root.dragFrom
                                                            ? root.entryAt(root.dragTo).y + root.entryAt(root.dragTo).height
                                                            : root.entryAt(root.dragTo).y) : 0) - height / 2
                        width: cardColumn.width
                        height: Theme.settingsWindow.dropIndicatorWidth
                        radius: height / 2
                        color: Theme.settingsWindow.dropIndicator
                        z: 2
                    }

                    ColumnLayout {
                        id: cardColumn
                        anchors.fill: parent
                        anchors.margins: Theme.settingsWindow.cardPadding
                        spacing: 0

                        Repeater {
                            id: entryRepeater
                            model: root.shown

                            ColumnLayout {
                                id: entry
                                required property var modelData
                                required property int index
                                readonly property bool dragged: root.dragging && root.dragId === modelData.id
                                // Not at creation: a reloaded list's new rows first see the drag's offsets and drop them in the same event.
                                property bool ready: false
                                Component.onCompleted: Qt.callLater(() => { ready = true })
                                readonly property Item handle: row.handle

                                Layout.fillWidth: true
                                spacing: 0
                                opacity: dragged && !root.dragSettling ? Theme.settingsWindow.dragOpacity : 1
                                z: dragged ? 1 : 0
                                transform: Translate {
                                    y: entry.dragged ? (root.dragSettling ? root.settledOffset() : root.dragDy)
                                                     : root.shiftOf(entry.index)
                                    Behavior on y {
                                        enabled: Motion.on && entry.ready && !(entry.dragged && !root.dragSettling)
                                        MotionNumber {}
                                    }
                                }

                                Rectangle {
                                    visible: Theme.settingsWindow.rowDividers && entry.index > 0
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Theme.settingsWindow.rowPadding
                                    Layout.rightMargin: Theme.settingsWindow.rowPadding
                                    implicitHeight: 1
                                    color: Theme.settingsWindow.rowDivider
                                }

                                PlatformRow {
                                    id: row
                                    Layout.fillWidth: true
                                    draggable: root.query === ""
                                    onDragStarted: (handle, x, y) => root.startDrag(handle, entry.index, x, y)
                                    onDragMoved: (handle, x, y) => root.moveDrag(handle, x, y)
                                    onDragEnded: dropped => root.endDrag(dropped)
                                    platform: entry.modelData
                                    expanded: root.expandedId === entry.modelData.id
                                    firstInCard: entry.index === 0
                                    lastInCard: entry.index === root.shown.length - 1
                                    position: root.defined.findIndex(p => p.id === entry.modelData.id) + 1
                                    count: root.defined.length
                                    focusPosition: root.movedId === entry.modelData.id && root.movedTo
                                    onToggleRequested: root.expandedId = expanded ? "" : entry.modelData.id
                                    onMoveToRequested: to => root.move(entry.modelData.id, to - position, true)
                                    onRemoveRequested: removeDialog.openFor(entry.modelData)
                                    onDefineRequested: {
                                        error.text = Library.addPlatform(entry.modelData.id, entry.modelData.id)
                                        if (error.text === "")
                                            root.expandedId = entry.modelData.id
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
