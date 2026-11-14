import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    // Rows are { id, title, info, enabled }; info is the muted text at the row's right, enabled
    // defaults to true. A disabled row can't be ticked and is drawn dimmed.
    property var model: []
    property var ticked: []
    property string label
    // The count line's noun with its number, e.g. qsTr("%n game(s)"); no count line without one.
    property string noun
    // "auto" shows the search row once the rows don't fit in the box; true and false force it.
    property var searchable: "auto"
    property string searchPlaceholder: qsTr("Search games")
    // True when the caller sets `model` from `query` itself, so the list doesn't filter by title.
    property bool customSearch: false
    property bool withSelectAll: false
    property string emptyText
    property int maxListHeight: Theme.collections.importListHeight

    readonly property string query: search.text.trim()
    readonly property alias onlySelected: onlyTicked.checked
    readonly property bool searchShown: searchable === true || (searchable === "auto" && model.length > rowsThatFit) || query !== ""

    signal toggled(string id)
    signal toggledMany(var ids, bool on)

    readonly property int rowsThatFit: Math.max(1, Math.floor(maxListHeight / Theme.collections.checklistRowHeight))
    // A lookup instead of `ticked.includes()` per row: libraries have a thousand games.
    readonly property var tickedSet: {
        const set = {}
        for (const id of ticked)
            set[id] = true
        return set
    }
    // What was ticked when the search or Selected last changed. Rows ticked or unticked after that stay
    // where they are, so a row doesn't leave from under the pointer.
    property var shownTicked: []
    readonly property var shown: {
        const needle = customSearch ? "" : query.toLowerCase()
        const only = onlySelected
        const keep = shownTicked
        return model.filter(r => (needle === "" || r.title.toLowerCase().includes(needle)) && (!only || keep.includes(r.id)))
    }
    readonly property var shownSelectable: shown.filter(r => r.enabled !== false).map(r => r.id)

    property string anchorId
    property bool anchorOn: true

    onQueryChanged: shownTicked = ticked
    Component.onCompleted: shownTicked = ticked

    function isTicked(id) {
        return tickedSet[id] === true
    }

    function indexOfShown(id) {
        return shown.findIndex(r => r.id === id)
    }

    function toggleRow(index) {
        const id = shown[index].id
        const anchor = indexOfShown(anchorId)
        if ((Qt.application.keyboardModifiers & Qt.ShiftModifier) && anchor >= 0) {
            anchorOn = !isTicked(id)
            tickRange(anchor, index, anchorOn)
        } else {
            anchorOn = !isTicked(id)
            toggled(id)
        }
        anchorId = id
        list.currentIndex = index
        list.forceActiveFocus()
    }

    function tickRange(from, to, on) {
        const ids = shown.slice(Math.min(from, to), Math.max(from, to) + 1)
            .filter(r => r.enabled !== false && isTicked(r.id) !== on).map(r => r.id)
        if (ids.length > 0)
            toggledMany(ids, on)
    }

    function extend(step) {
        const next = Math.max(0, Math.min(shown.length - 1, list.currentIndex + step))
        if (indexOfShown(anchorId) < 0) {
            anchorId = shown[list.currentIndex].id
            anchorOn = !isTicked(anchorId)
        }
        list.currentIndex = next
        tickRange(indexOfShown(anchorId), next, anchorOn)
    }

    function selectAllShown() {
        const on = shownSelectable.some(id => !isTicked(id))
        const ids = shownSelectable.filter(id => isTicked(id) !== on)
        if (ids.length > 0)
            toggledMany(ids, on)
    }

    function focusSearch(text) {
        if (!searchShown)
            return
        search.forceActiveFocus()
        search.insert(search.length, text)
    }

    spacing: Theme.layout.spacing

    component Tally: Label {
        anchors.right: parent.right
        anchors.rightMargin: Theme.collections.checklistRowPadding
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.collections.rowCountColor
        font.pixelSize: Theme.controls.labelFontSize
    }

    RowLayout {
        visible: root.label !== "" || root.noun !== ""
        Layout.fillWidth: true
        spacing: Theme.controls.controlSpacing

        Label {
            visible: root.label !== ""
            text: root.label
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            font.weight: Theme.font.strongWeight
        }
        Item { Layout.fillWidth: true }
        Label {
            visible: root.noun !== ""
            readonly property bool narrowed: root.query !== "" || root.onlySelected
            text: (narrowed ? qsTr("%1 of %2 shown").arg(root.shown.length).arg(root.model.length)
                            : root.noun.replace("%n", root.model.length))
                  + " · " + qsTr("%1 selected").arg(root.ticked.length)
            color: Theme.collections.checklistCountColor
            font.pixelSize: Theme.controls.descriptionFontSize
        }
    }

    RowLayout {
        visible: root.searchShown
        Layout.fillWidth: true
        spacing: Theme.controls.controlSpacing

        SettingsSearchField {
            id: search
            Layout.fillWidth: true
            implicitHeight: Theme.controls.inputHeight
            placeholderText: root.searchPlaceholder
            Keys.onEscapePressed: event => {
                event.accepted = text !== ""
                if (event.accepted)
                    clear()
            }
            Keys.onShortcutOverride: event => {
                if (event.key === Qt.Key_Escape && text !== "")
                    event.accepted = true
            }
        }
        SettingsButton {
            id: onlyTicked
            implicitHeight: Theme.controls.inputHeight
            checkable: true
            enabled: root.ticked.length > 0 || checked
            text: qsTr("Selected")
            tip: qsTr("Show only the ones you've checked")
            textColor: checked ? Theme.settingsWindow.markerColor : foreground
            onToggled: root.shownTicked = root.ticked
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: box.implicitHeight + 2 * Theme.collections.checklistPadding
        radius: Theme.controls.inputRadius
        color: Theme.controls.inputBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth

        ColumnLayout {
            id: box
            anchors.fill: parent
            anchors.margins: Theme.collections.checklistPadding
            spacing: 0

            SettingsCheckBox {
                id: selectAll
                readonly property int count: root.shownSelectable.filter(id => root.isTicked(id)).length
                visible: root.withSelectAll
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.collections.checklistRowHeight
                leftPadding: Theme.collections.checklistRowPadding
                rightPadding: Theme.collections.checklistRowPadding + tally.implicitWidth + spacing
                bordered: false
                enabled: root.shownSelectable.length > 0
                text: qsTr("Select all")
                checked: count > 0 && count === root.shownSelectable.length
                partial: count > 0 && count < root.shownSelectable.length
                nextCheckState: () => {
                    root.selectAllShown()
                    return checkState
                }

                Tally {
                    id: tally
                    text: qsTr("%1 of %2 selected").arg(selectAll.count).arg(root.shownSelectable.length)
                }
            }

            Rectangle {
                visible: root.withSelectAll
                Layout.fillWidth: true
                implicitHeight: 1
                color: Theme.controls.inputBorder
            }

            Label {
                visible: root.shown.length === 0
                Layout.fillWidth: true
                Layout.preferredHeight: 2 * Theme.collections.checklistRowHeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.query !== "" || root.onlySelected ? qsTr("No games match") : root.emptyText
                color: Theme.controls.descriptionColor
            }

            ListView {
                id: list
                visible: root.shown.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, root.rowsThatFit * Theme.collections.checklistRowHeight)
                clip: true
                // A curve-rendered check mark in a row cut off by the edge isn't clipped, only a layer is.
                layer.enabled: interactive
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height
                keyNavigationEnabled: false
                activeFocusOnTab: true
                model: root.shown
                ScrollBar.vertical: SettingsScrollBar { id: bar; trackless: true }

                // The window's list shortcuts and Ctrl+A would otherwise take these keys.
                Keys.onShortcutOverride: event => {
                    const ctrl = event.modifiers & Qt.ControlModifier
                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Space
                            || (ctrl && (event.key === Qt.Key_A || event.key === Qt.Key_F))
                            || (!ctrl && event.text.length === 1 && event.text.trim() !== ""))
                        event.accepted = true
                }
                Keys.onPressed: event => {
                    const ctrl = event.modifiers & Qt.ControlModifier
                    const shift = event.modifiers & Qt.ShiftModifier
                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                        const step = event.key === Qt.Key_Up ? -1 : 1
                        if (shift)
                            root.extend(step)
                        else
                            currentIndex = Math.max(0, Math.min(count - 1, currentIndex + step))
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    } else if (event.key === Qt.Key_Space && currentIndex >= 0 && root.shown[currentIndex].enabled !== false) {
                        root.toggleRow(currentIndex)
                    } else if (ctrl && event.key === Qt.Key_A) {
                        root.selectAllShown()
                    } else if (ctrl && event.key === Qt.Key_F) {
                        root.focusSearch("")
                    } else if (!ctrl && event.text.length === 1 && event.text.trim() !== "") {
                        root.focusSearch(event.text)
                    } else {
                        return
                    }
                    event.accepted = true
                }

                delegate: SettingsCheckBox {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool off: modelData.enabled === false
                    width: list.width - (bar.visible ? bar.thickness + Theme.layout.spacing : 0)
                    height: Theme.collections.checklistRowHeight
                    leftPadding: Theme.collections.checklistRowPadding
                    rightPadding: Theme.collections.checklistRowPadding + info.implicitWidth + spacing
                    bordered: off
                    enabled: !off
                    focusPolicy: Qt.NoFocus
                    opacity: off ? 0.5 : 1
                    text: modelData.title
                    checked: root.isTicked(modelData.id)
                    nextCheckState: () => {
                        root.toggleRow(index)
                        return checkState
                    }

                    background: Rectangle {
                        radius: Theme.collections.rowRadius
                        color: row.hovered ? Theme.collections.rowHover : "transparent"
                        border.color: Theme.controls.inputBorderFocus
                        border.width: list.activeFocus && row.index === list.currentIndex ? Math.max(1, Theme.controls.inputBorderWidth) : 0
                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                    }

                    Tally {
                        id: info
                        text: row.modelData.info ?? ""
                    }
                }
            }
        }
    }
}
