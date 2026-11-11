import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The search box, the collections and filter buttons, the filter chips and the open collection's
// banner: the top of both the game list and the cover grid. See docs/code/qml.md.
ColumnLayout {
    id: root

    property real reach: 0
    property real extraTop: 0
    property bool ready: false
    property bool showChips: true
    // Grid view: the open collection's banner sits in the button row, `inlineWidth` being the width
    // the search box and the buttons keep.
    property real inlineWidth: 0
    readonly property bool bannerInline: inlineWidth > 0
    property alias field: search

    signal message(string text)
    signal focusRequested()

    spacing: Theme.layout.spacing

    function focusList() { focusRequested() }

    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }


    function openFilters() {
        filterPanel.active = true
        if (filterPanel.item.visible) {
            filterPanel.item.close()
            focusList()
        } else
            filterPanel.item.open()
    }

    function openCollections() {
        collectionsPopup.active = true
        if (collectionsPopup.item.visible) {
            collectionsPopup.item.close()
            focusList()
        } else
            collectionsPopup.item.open()
    }

    readonly property var activeFilters: {
        const out = []
        const platform = General.railHidden && Library.platformFilter !== ""
            ? Library.gameCounts.find(p => p.id === Library.platformFilter) : undefined
        if (platform !== undefined)
            out.push({ key: "platform", text: platform.name, off: () => Library.platformFilter = "" })
        // Installed off is how a collection opens, so there it's Installed on that is the filter.
        if (Library.collectionId !== "" && Library.installedOnly)
            out.push({ key: "installed", text: qsTr("Installed only"), off: () => Library.installedOnly = false })
        else if (Library.collectionId === "" && !Library.installedOnly)
            out.push({ key: "installed", text: qsTr("Not installed shown"), off: () => Library.installedOnly = true })
        if (Library.favoritesOnly)
            out.push({ key: "favorites", text: qsTr("Favorites"), off: () => Library.favoritesOnly = false })
        if (Library.showHidden)
            out.push({ key: "hidden", text: qsTr("Hidden shown"), off: () => Library.showHidden = false })
        for (const status of Library.statuses) {
            if (Library.statusFilter.includes(status.id))
                out.push({ key: "status:" + status.id, text: status.name,
                           off: () => Library.statusFilter = Library.statusFilter.filter(s => s !== status.id) })
        }
        if (Library.yearFrom > 0 || Library.yearTo > 0) {
            const range = Library.yearFrom > 0 && Library.yearTo > 0
                ? (Library.yearFrom === Library.yearTo ? String(Library.yearFrom) : Library.yearFrom + " – " + Library.yearTo)
                : (Library.yearFrom > 0 ? qsTr("From %1").arg(Library.yearFrom) : qsTr("Up to %1").arg(Library.yearTo))
            out.push({ key: "year", text: range, off: () => { Library.yearFrom = 0; Library.yearTo = 0 } })
        }
        if (Library.minRating >= 0)
            out.push({ key: "rating", text: qsTr("Rated %1 or more").arg(Library.minRating),
                       off: () => Library.minRating = -1 })
        for (const time of Library.playTimeChoices) {
            if (Library.playTimeFilter.includes(time.id))
                out.push({ key: "playtime:" + time.id, text: qsTr("Played: %1").arg(time.name),
                           off: () => Library.playTimeFilter = Library.playTimeFilter.filter(t => t !== time.id) })
        }
        if (Library.lastPlayedFilter !== "") {
            const names = { week: qsTr("Played this week"), month: qsTr("Played this month"),
                            year: qsTr("Played this year"), older: qsTr("Played over a year ago"), never: qsTr("Never played") }
            out.push({ key: "lastplayed", text: names[Library.lastPlayedFilter], off: () => Library.lastPlayedFilter = "" })
        }
        for (const state of Library.achievementChoices) {
            if (Library.achievementFilter.includes(state.id))
                out.push({ key: "achievement:" + state.id, text: qsTr("Achievements: %1").arg(state.name),
                           off: () => Library.achievementFilter = Library.achievementFilter.filter(a => a !== state.id) })
        }
        const palette = Theme.tags.palette
        let tags = 0
        for (const name of Library.tagFilter) {
            const tag = Library.tags.find(t => t.name === name)
            if (tag === undefined)
                continue
            ++tags
            out.push({
                key: "tag:" + name,
                text: name,
                tagColor: palette.length > 0 ? palette[tag.color % palette.length] : Theme.filters.chipBackground,
                off: () => Library.tagFilter = Library.tagFilter.filter(t => t !== name)
            })
        }
        if (Library.tagMatchAll && tags >= 2)
            out.push({ key: "all-tags", text: qsTr("All tags"), off: () => Library.tagMatchAll = false })
        return out
    }

    // Without the functions, which a model can't hold: a chip finds its `off` by key.
    KeyedListModel {
        id: chipModel
        leaveSpeed: "fast"
        source: root.activeFilters.map(f => ({ key: f.key, text: f.text, tagColor: String(f.tagColor ?? "transparent") }))
    }

    function askCollection(mode, id, name, ids) {
        collectionDialog.active = true
        const dialog = collectionDialog.item
        dialog.mode = mode
        dialog.collectionId = id
        dialog.startName = name
        dialog.ids = ids
        dialog.open()
    }
    RowLayout {
        id: topRow
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        TextField {
            id: search
            Layout.fillWidth: !root.bannerInline
            Layout.preferredWidth: root.bannerInline ? root.inlineWidth - 2 * (Theme.filterButton.size + Theme.layout.spacing) : -1
            implicitHeight: Theme.search.height
            leftPadding: Theme.search.iconSize + 19
            rightPadding: Theme.search.iconSize + 19
            placeholderText: Theme.search.placeholder
            placeholderTextColor: Theme.search.placeholderColor
            color: Theme.search.text
            selectionColor: Theme.palette.accent
            selectedTextColor: Theme.palette.onAccent
            background: Surface {
                radius: Theme.search.radius
                fill: Theme.search.background
                fillEnd: Theme.search.backgroundEnd
                direction: Theme.search.gradientDirection
                border.color: search.activeFocus ? Theme.search.borderFocus : Theme.search.border
                border.width: search.activeFocus ? Theme.search.borderWidthFocus : Theme.search.borderWidth

                Behavior on border.color { enabled: Motion.fades; MotionColor {} }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: Theme.search.hoverTint
                    opacity: search.hovered ? 1 : 0
                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                }
            }

            onTextChanged: Library.search = text
            Keys.onEscapePressed: root.focusList()
            HoverHandler { id: searchPointer }
            ContextMenu.menu: TextContextMenu { field: search; pointer: searchPointer }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.layout.spacing + Theme.layout.spacingSmall
                size: Theme.search.iconSize
                path: Icons.search
                color: Theme.search.icon
            }

            Icon {
                visible: search.text.length > 0
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: Theme.layout.spacing + Theme.layout.spacingSmall
                size: Theme.search.iconSize
                path: Icons.close
                color: searchClearMouse.containsMouse ? Theme.palette.text : Theme.search.icon

                MouseArea {
                    id: searchClearMouse
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        search.clear()
                        search.forceActiveFocus()
                    }
                }

                ToolTip.visible: searchClearMouse.containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("Clear search")
            }
        }

        Item {
            id: collectionsHost
            visible: Theme.search.showFilterButton
            implicitWidth: Theme.filterButton.size
            implicitHeight: Theme.filterButton.size

            IconButton {
                anchors.fill: parent
                icon: Icons.bookmarks
                iconSize: Theme.filterButton.iconSize
                iconColor: Theme.filterButton.icon
                radius: Theme.filterButton.radius
                fill: Theme.filterButton.background
                fillEnd: Theme.filterButton.backgroundEnd
                hoverTint: Theme.filterButton.hoverTint
                direction: Theme.filterButton.gradientDirection
                border.color: Theme.filterButton.border
                border.width: Theme.filterButton.borderWidth
                tip: Actions.tip(qsTr("Collections"), Actions.keys.open_collections)
                onClicked: root.openCollections()
            }

            Loader {
                id: collectionsPopup
                active: false
                source: "CollectionsPopup.qml"
                onLoaded: item.parent = collectionsHost
            }
            Binding {
                target: collectionsPopup.item
                property: "x"
                // Past the filter button too, so the popup stays clear of the list.
                value: collectionsHost.width + filterHost.width + 2 * Theme.layout.spacing
            }
            Connections {
                target: collectionsPopup.item
                function onClosedByKey() { root.focusList() }
                function onCollectionDialogRequested(mode, id, name) {
                    collectionsPopup.item.close()
                    root.askCollection(mode, id, name, [])
                }
                function onSteamImportRequested() {
                    collectionsPopup.item.close()
                    steamCollections.active = true
                    steamCollections.item.open()
                }
                function onCollectionDeleted(id) {
                    const error = Library.deleteCollection(id)
                    if (error !== "")
                        root.message(error)
                }
            }
        }

        // The panel's parent: the button dips when pressed, and a popup follows its parent's
        // scale, so the panel would shift as the button springs back.
        Item {
            id: filterHost
            visible: Theme.search.showFilterButton
            implicitWidth: Theme.filterButton.size
            implicitHeight: Theme.filterButton.size

            IconButton {
                anchors.fill: parent
                icon: Icons.filter
                iconSize: Theme.filterButton.iconSize
                iconColor: Theme.filterButton.icon
                radius: Theme.filterButton.radius
                fill: Theme.filterButton.background
                fillEnd: Theme.filterButton.backgroundEnd
                hoverTint: Theme.filterButton.hoverTint
                direction: Theme.filterButton.gradientDirection
                border.color: Theme.filterButton.border
                border.width: Theme.filterButton.borderWidth
                badgeColor: Theme.filterButton.badge
                badge: Library.filtersActive || (General.railHidden && Library.platformFilter !== "")
                tip: Actions.tip(root.activeFilters.length > 0
                                 ? qsTr("Filters: %1").arg(root.activeFilters.map(f => f.text).join(", "))
                                 : qsTr("Filters"),
                                 Actions.keys.open_filters)
                onClicked: root.openFilters()
            }

            Loader {
                id: filterPanel
                active: false
                source: "FilterPanel.qml"
                onLoaded: item.parent = filterHost
            }
            Binding {
                target: filterPanel.item
                property: "x"
                // Slides left when the widened panel would leave the window.
                value: {
                    const wanted = filterHost.width + Theme.layout.spacing
                    const hostX = root.width >= 0 ? filterHost.mapToItem(null, 0, 0).x : 0
                    const room = root.Window.width - Theme.layout.margin - hostX - (filterPanel.item?.width ?? 0)
                    return Math.max(Theme.layout.margin - hostX, Math.min(wanted, room))
                }
            }
            Connections {
                target: filterPanel.item
                function onClosedByKey() { root.focusList() }
                function onCollectionDialogRequested(mode, id, name) {
                    filterPanel.item.close()
                    root.askCollection(mode, id, name, [])
                }
            }
        }
    }

    // The filters that are on; × turns one off, the chip opens the panel.
    // The row's space opens at once, as the first chip starts popping in. It closes in a second
    // step: the last chip pops out first, then the list eases up.
    Item {
        id: chipRow

        // Here and not beside the row, where each idle Loader would be a cell and add a gap.
        Loader {
            id: collectionDialog
            active: false
            source: "CollectionDialog.qml"
        }
        Connections {
            target: collectionDialog.item
            function onMessage(text) { root.message(text) }
            function onClosed() { root.focusList() }
        }

        Loader {
            id: steamCollections
            active: false
            source: "SteamCollectionsDialog.qml"
        }
        Connections {
            target: steamCollections.item
            function onMessage(text) { root.message(text) }
            function onClosed() { root.focusList() }
        }

        readonly property real wanted: root.showChips && Theme.search.showFilterChips && (root.activeFilters.length > 0 || chipModel.count > 0) ? 1 : 0
        property real space: wanted
        property real flowHeight: 0
        Binding on flowHeight {
            when: chipFlow.implicitHeight > 0
            value: chipFlow.implicitHeight
            restoreMode: Binding.RestoreNone
        }

        onWantedChanged: {
            closeAnimation.stop()
            if (wanted === 1 || !Motion.on || !root.ready)
                space = wanted
            else
                closeAnimation.start()
        }
        MotionNumber {
            id: closeAnimation
            target: chipRow
            property: "space"
            to: 0
            speed: "normal"
        }

        Layout.fillWidth: true
        // Always in the layout, so nothing shifts when it empties. A cell can't be smaller than
        // nothing, so the extra gap it adds when empty is taken back by the list's top margin.
        Layout.preferredHeight: (flowHeight + Theme.layout.spacing) * space

    Flow {
        id: chipFlow
        width: parent.width
        spacing: Theme.filters.chipSpacing

        add: Transition {
            enabled: Motion.on && root.ready && !chipModel.rebuilt
            MotionNumber { property: "opacity"; from: 0; to: 1 }
            MotionPop { property: "scale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
        }
        move: Transition {
            enabled: Motion.on && root.ready && !chipModel.rebuilt
            MotionNumber { properties: "x,y"; speed: "normal" }
        }

        Repeater {
            model: chipModel

            FilterChip {
                id: chip
                required property var model
                readonly property bool leaving: model.leaving
                text: model.text
                tagColor: model.tagColor
                enabled: !leaving
                onClicked: root.openFilters()
                onRemoved: root.activeFilters.find(f => f.key === model.key)?.off()

                PopLeave { target: chip; leaving: chip.leaving; animated: Motion.on && root.ready }
            }
        }
    }
    }

    // The open collection: its name and size, and × to leave it.
    Rectangle {
        parent: root.bannerInline ? topRow : root
        visible: Library.collectionId !== ""
        Layout.fillWidth: true
        Layout.leftMargin: -root.reach
        Layout.topMargin: root.bannerInline ? 0 : root.extraTop - Theme.layout.spacing
        Layout.preferredHeight: root.bannerInline ? Theme.filterButton.size : Theme.collections.bannerHeight
        radius: Theme.collections.bannerRadius
        color: Theme.collections.bannerBackground

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.layout.spacing
            anchors.rightMargin: Theme.layout.spacingSmall
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: Library.collection.name ?? ""
                color: Theme.collections.bannerText
                font.weight: Theme.font.strongWeight
                elide: Text.ElideRight
            }
            Label {
                text: qsTr("%n game(s)", "", Library.collection.count ?? 0)
                color: Theme.collections.bannerCountColor
                font.pixelSize: Theme.list.sortValueFontSize
            }
            AbstractButton {
                id: leaveButton
                Layout.rightMargin: Theme.layout.spacingSmall
                padding: Theme.layout.spacingSmall
                text: qsTr("Leave the collection")
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                onClicked: Library.closeCollection()

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: text

                background: null
                contentItem: Icon {
                    id: leaveIcon
                    path: Icons.close
                    size: Theme.list.sortValueFontSize + 2
                    color: Theme.collections.bannerText
                    opacity: leaveButton.hovered ? 1 : 0.7
                }
            }
        }
    }
}
