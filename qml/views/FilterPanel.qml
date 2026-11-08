import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    signal closedByKey()
    signal collectionDialogRequested(string mode, string id, string name)
    signal collectionDeleted(string id)

    readonly property var directionNames: ({
        title: [qsTr("A to Z"), qsTr("Z to A")],
        last_played: [qsTr("Oldest first"), qsTr("Newest first")],
        play_time: [qsTr("Least played first"), qsTr("Most played first")],
    })
    readonly property var sortOrders: [
        { id: "title", name: qsTr("Title") },
        { id: "last_played", name: qsTr("Last played") },
        { id: "play_time", name: qsTr("Play time") },
    ]
    readonly property var selectedTags: Library.tagFilter.filter(t => Library.tags.some(tag => tag.name === t))
    readonly property var shownTags: Library.tags.filter(t => t.games > 0 || Library.tagFilter.includes(t.name))

    readonly property var platformCounts: ({
        total: General.railHidden ? Library.gameCounts.reduce((sum, c) => sum + c.count, 0) : 0
    })

    function toggleTag(name) {
        const tags = Library.tagFilter.slice()
        const i = tags.indexOf(name)
        if (i >= 0)
            tags.splice(i, 1)
        else
            tags.push(name)
        Library.tagFilter = tags
    }

    function toggleStatus(id) {
        const statuses = Library.statusFilter.slice()
        const i = statuses.indexOf(id)
        if (i >= 0)
            statuses.splice(i, 1)
        else
            statuses.push(id)
        Library.statusFilter = statuses
    }

    function toggleSteam(kind) {
        const kinds = Library.steamFilter.slice()
        const i = kinds.indexOf(kind)
        if (i >= 0)
            kinds.splice(i, 1)
        else
            kinds.push(kind)
        Library.steamFilter = kinds
    }

    width: General.railHidden ? Theme.filters.panelWidthRailHidden : Theme.filters.panelWidth
    readonly property int swellRoom: Math.min(4, Theme.filters.padding)
    padding: Theme.filters.padding - swellRoom
    modal: false
    focus: true
    closePolicy: Popup.CloseOnPressOutsideParent

    onOpened: root.forceActiveFocus(Qt.TabFocusReason)

    background: Surface {
        radius: Theme.filters.radius
        fill: Theme.filters.background
        fillEnd: Theme.filters.backgroundEnd
        direction: Theme.filters.gradientDirection
        border.color: Theme.filters.border
        border.width: Theme.filters.borderWidth
    }

    component PressDip: Item {
        id: dip
        property bool pressed
        property real press: pressed ? Theme.motion.pressScale : 1
        Behavior on press { enabled: Motion.on; MotionNumber {} }
        Binding { target: dip.parent; property: "scale"; value: dip.press }
    }

    component FocusRing: Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Theme.filters.sortRadius + 3
        color: "transparent"
        border.color: Theme.filters.focusRing
        border.width: Theme.filters.focusRingWidth
        property var control: parent
        visible: control?.visualFocus ?? false
    }

    component Heading: Label {
        property string title
        text: Theme.filters.headingUppercase ? title.toUpperCase() : title
        color: Theme.filters.headingColor
        font.weight: Theme.font.strongWeight
        font.pixelSize: Theme.filters.headingFontSize
        font.letterSpacing: Theme.filters.headingUppercase ? 0.8 : 0
    }

    component CollectionRow: AbstractButton {
        id: row
        property bool selected: false
        property string detail
        signal menuRequested(real x, real y)

        width: parent?.width ?? 0
        implicitHeight: Theme.collections.rowHeight
        leftPadding: Theme.layout.spacing
        rightPadding: Theme.layout.spacing
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        PressDip { pressed: row.pressed }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: eventPoint => row.menuRequested(eventPoint.position.x, eventPoint.position.y)
        }
        Keys.onMenuPressed: row.menuRequested(row.width / 2, row.height)
        ToolTip.visible: hovered && nameLabel.truncated
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: row.text

        background: Rectangle {
            radius: Theme.collections.rowRadius
            color: row.selected ? Theme.collections.rowSelected
                 : row.hovered ? Theme.collections.rowHover : Theme.collections.rowBackground
        }
        contentItem: RowLayout {
            spacing: Theme.layout.spacing

            Label {
                id: nameLabel
                Layout.fillWidth: true
                text: row.text
                elide: Text.ElideRight
                font.weight: row.selected ? Theme.font.onAccentWeight : Font.Normal
                color: row.selected ? Theme.collections.rowSelectedText : Theme.collections.rowText
            }
            Label {
                text: row.detail
                font.weight: Theme.font.emphasisWeight
                color: row.selected ? Theme.collections.rowSelectedText : Theme.collections.rowCountColor
                opacity: row.selected ? 0.75 : 1
            }
        }
        FocusRing {}
    }

    component Segment: AbstractButton {
        id: segment
        property bool selected: false
        property string detail
        property bool leftAligned: detail !== ""
        property string tip
        property bool showCheck: false

        PressDip { pressed: segment.pressed }

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: Theme.filters.sortHeight
        leftPadding: Theme.layout.spacing
        rightPadding: Theme.layout.spacing
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        ToolTip.visible: hovered && (segment.tip !== "" || nameLabel.truncated)
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: segment.tip !== "" ? segment.tip : segment.text

        background: Rectangle {
            radius: Theme.filters.sortRadius
            color: segment.selected ? Theme.filters.sortSelected
                 : segment.hovered ? Theme.filters.sortHover : Theme.filters.sortBackground
        }
        contentItem: RowLayout {
            spacing: Theme.layout.spacing

            Label {
                id: nameLabel
                Layout.fillWidth: true
                text: segment.text
                horizontalAlignment: segment.leftAligned ? Text.AlignLeft : Text.AlignHCenter
                elide: Text.ElideRight
                font.weight: segment.selected ? Theme.font.onAccentWeight : Font.Normal
                color: segment.selected ? Theme.filters.sortSelectedText : Theme.filters.hintColor
            }
            Label {
                visible: segment.detail !== ""
                text: segment.detail
                font.weight: Theme.font.emphasisWeight
                color: Theme.filters.hintColor
                opacity: segment.selected ? 0.75 : 1
                // On the selected color the muted text would vanish, so it follows the selected text instead.
                Binding on color { when: segment.selected; value: Theme.filters.sortSelectedText }
            }
            MarkPop {
                shown: segment.showCheck && segment.selected
                implicitWidth: checkIcon.implicitWidth
                implicitHeight: checkIcon.implicitHeight
                Icon {
                    id: checkIcon
                    path: Icons.check
                    size: Theme.filters.headingFontSize + 3
                    color: Theme.filters.sortSelectedText
                }
            }
        }
        FocusRing {}
    }

    component ToggleRow: Segment {
        property bool isOn: false
        signal flipped()
        selected: isOn
        leftAligned: true
        showCheck: true
        opacity: enabled ? 1 : 0.5
        onClicked: flipped()
    }

    height: Math.min(implicitHeight, (parent?.Window.height ?? 0) - 2 * Theme.layout.margin)

    contentItem: Flickable {
        id: flick
        implicitHeight: content.implicitHeight + 2 * root.swellRoom
        leftMargin: root.swellRoom
        rightMargin: root.swellRoom
        topMargin: root.swellRoom
        bottomMargin: root.swellRoom
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SettingsScrollBar {
            id: scrollBar
            thickness: Theme.gameEditor.bulkScrollbarWidth
            trackless: true
        }

        Keys.onEscapePressed: {
            root.close()
            root.closedByKey()
        }

        ColumnLayout {
            id: content
            width: flick.width - 2 * root.swellRoom - (scrollBar.visible ? scrollBar.width + Theme.layout.spacing : 0)
            spacing: Theme.filters.spacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Collections") }
                Flickable {
                    id: collectionsFlick
                    visible: Library.collections.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(collectionRows.implicitHeight, Theme.collections.listMaxHeight)
                    contentHeight: collectionRows.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height

                    Column {
                        id: collectionRows
                        width: collectionsFlick.width
                        spacing: Theme.collections.rowSpacing

                        Repeater {
                            model: Library.collections

                            CollectionRow {
                                required property var modelData
                                text: modelData.name
                                detail: modelData.count
                                selected: Library.collectionId === modelData.id
                                onClicked: Library.openCollection(modelData.id)
                                onMenuRequested: (x, y) => {
                                    collectionMenu.collection = modelData
                                    collectionMenu.popup(this, x, y)
                                }
                            }
                        }
                    }
                }
                CollectionRow {
                    text: qsTr("New collection")
                    onClicked: root.collectionDialogRequested("new", "", "")
                }
            }

            SettingsMenu {
                id: collectionMenu
                property var collection: ({})
                atPointer: true

                SettingsMenuItem {
                    iconPath: Icons.editLine
                    text: qsTr("Rename")
                    onTriggered: root.collectionDialogRequested("rename", collectionMenu.collection.id,
                                                                collectionMenu.collection.name)
                }
                SettingsMenuItem {
                    iconPath: Icons.gear
                    text: qsTr("Edit")
                    onTriggered: root.collectionDialogRequested("edit", collectionMenu.collection.id,
                                                                collectionMenu.collection.name)
                }
                SettingsMenuItem {
                    iconPath: Icons.deleteBin6Line
                    text: qsTr("Delete")
                    danger: true
                    onTriggered: root.collectionDeleted(collectionMenu.collection.id)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Heading {
                        Layout.fillWidth: true
                        title: qsTr("Sort")
                    }
                    AbstractButton {
                        id: direction
                        focusPolicy: Qt.StrongFocus
                        hoverEnabled: true
                        onClicked: Library.sortDescending = !Library.sortDescending

                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("Reverse the order")

                        background: null
                        contentItem: Row {
                            spacing: 4

                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                path: Library.sortDescending ? Icons.arrowDown : Icons.arrowUp
                                size: Theme.filters.headingFontSize + 3
                                color: directionLabel.color
                            }
                            Label {
                                id: directionLabel
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.directionNames[Library.sort][Library.sortDescending ? 1 : 0]
                                color: direction.hovered ? Theme.filters.text : Theme.filters.hintColor
                                font.pixelSize: Theme.filters.headingFontSize
                            }
                        }
                        FocusRing {}
                    }
                }
                RowLayout {
                    id: sortRow
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: root.sortOrders

                        Segment {
                            required property var modelData
                            text: modelData.name
                            selected: Library.sort === modelData.id
                            onClicked: Library.sort = modelData.id
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Group by") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: [
                            { id: "platform", name: qsTr("Platform") },
                            { id: "status", name: qsTr("Status") },
                            { id: "none", name: qsTr("None") },
                        ]

                        Segment {
                            required property var modelData
                            text: modelData.name
                            selected: Library.groupBy === modelData.id
                            onClicked: Library.groupBy = modelData.id
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Show") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 4
    
                    ToggleRow {
                        text: qsTr("Installed")
                        isOn: Library.installedOnly
                        onFlipped: Library.installedOnly = !Library.installedOnly
                    }
                    // Disabled rather than hidden for the title sort, so the panel doesn't change height.
                    ToggleRow {
                        enabled: Library.sort !== "title"
                        text: Library.sort === "play_time" ? qsTr("Play time") : qsTr("Last played")
                        tip: qsTr("Show last played or play time on rows")
                        isOn: Library.showSortValue
                        onFlipped: Library.showSortValue = !Library.showSortValue
                    }
                    ToggleRow {
                        text: qsTr("Favorites")
                        isOn: Library.favoritesOnly
                        onFlipped: Library.favoritesOnly = !Library.favoritesOnly
                    }
                    ToggleRow {
                        enabled: Library.hiddenGames.length > 0 || Library.showHidden
                        text: qsTr("Hidden")
                        detail: Library.hiddenGames.length
                        showCheck: false
                        isOn: Library.showHidden
                        onFlipped: Library.showHidden = !Library.showHidden
                    }
                }
            }

            ColumnLayout {
                visible: General.railHidden
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Platforms") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    Segment {
                        text: qsTr("All")
                        detail: root.platformCounts.total
                        selected: Library.platformFilter === ""
                        onClicked: Library.platformFilter = ""
                    }
                    Repeater {
                        model: !General.railHidden ? []
                         : Theme.filters.platformsAlphabetical
                           ? Library.gameCounts.slice().sort((a, b) => a.name.localeCompare(b.name))
                           : Library.gameCounts

                        Segment {
                            required property var modelData
                            text: modelData.name
                            detail: modelData.count
                            selected: Library.platformFilter === modelData.id
                            onClicked: Library.platformFilter = modelData.id
                        }
                    }
                }
            }

            ColumnLayout {
                visible: Library.steam.familyGames > 0 || Library.steamFilter.length > 0
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Steam") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    Segment {
                        text: qsTr("Owned")
                        detail: Library.steam.ownedGames
                        selected: Library.steamFilter.includes("owned")
                        onClicked: root.toggleSteam("owned")
                    }
                    Segment {
                        text: qsTr("Family")
                        detail: Library.steam.familyGames
                        selected: Library.steamFilter.includes("family")
                        onClicked: root.toggleSteam("family")
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Status") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    Repeater {
                        model: Library.statuses

                        Segment {
                            required property var modelData
                            text: modelData.name
                            detail: modelData.games
                            selected: Library.statusFilter.includes(modelData.id)
                            onClicked: root.toggleStatus(modelData.id)
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                AbstractButton {
                    id: tagsHeader
                    Layout.fillWidth: true
                    implicitHeight: tagsHeading.implicitHeight
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    onClicked: Library.tagsOpen = !Library.tagsOpen

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: Library.tagsOpen ? qsTr("Hide the tags") : qsTr("Show the tags")

                    background: null
                    contentItem: RowLayout {
                        spacing: Theme.layout.spacing / 2

                        Icon {
                            path: Icons.chevronRight
                            size: Theme.filters.headingFontSize + 3
                            color: tagsHeading.color
                            rotation: Library.tagsOpen ? 90 : 0
                            Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                        }
                        Heading {
                            id: tagsHeading
                            title: root.selectedTags.length > 0
                                ? qsTr("Tags (%1)").arg(root.selectedTags.length) : qsTr("Tags")
                        }
                        Item { Layout.fillWidth: true }
                    }
                    FocusRing {}
                }
                Label {
                    visible: Library.tagsOpen && root.shownTags.length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("No game has a tag yet. Add them in a game's settings.")
                    color: Theme.filters.hintColor
                }
                GridLayout {
                    visible: Library.tagsOpen && root.shownTags.length > 0
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: Theme.filters.tagSpacing
                    columnSpacing: Theme.filters.tagSpacing

                    Repeater {
                        model: root.shownTags

                        AbstractButton {
                            id: tagButton
                            required property var modelData
                            readonly property bool selected: Library.tagFilter.includes(modelData.name)

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: Theme.tags.height
                            ToolTip.visible: hovered && chip.truncated
                            ToolTip.delay: Theme.tooltip.delay
                            ToolTip.text: modelData.name
                            focusPolicy: Qt.StrongFocus
                            onClicked: root.toggleTag(tagButton.modelData.name)
                            hoverEnabled: true
                            PressDip { pressed: tagButton.pressed }

                            HoverHandler { cursorShape: Qt.PointingHandCursor }

                            background: null
                            contentItem: TagChip {
                                id: chip
                                name: tagButton.modelData.name
                                spread: true
                                plus: false
                                detail: tagButton.modelData.games
                                textColor: tagButton.selected ? chip.fillText
                                          : Qt.hsla(chip.tagColor.hslHue, chip.tagColor.hslSaturation, Theme.filters.tagTextLightness, 1)
                                detailColor: Qt.alpha(textColor, Theme.filters.tagCountOpacity)
                                colorIndex: tagButton.modelData.color
                                outlined: !tagButton.selected
                                outlineWidth: Theme.filters.tagBorderWidth
                                fillOpacity: tagButton.hovered ? Theme.filters.tagHoverOpacity : Theme.filters.tagOffOpacity
                                lit: tagButton.hovered
                                raised: tagButton.hovered
                                raiseBy: Theme.filters.tagHoverLift
                                Behavior on fillOpacity { enabled: Motion.fades; MotionNumber { fade: true } }
                            }
                            FocusRing { radius: Theme.tags.radius + 3 }
                        }
                    }
                }
                ToggleRow {
                    visible: Library.tagsOpen && (root.selectedTags.length >= 2 || Library.tagMatchAll)
                    text: qsTr("Match all selected tags")
                    isOn: Library.tagMatchAll
                    onFlipped: Library.tagMatchAll = !Library.tagMatchAll
                }
            }

            SettingsButton {
                visible: Library.filtersActive && Library.collectionId === ""
                Layout.fillWidth: true
                text: qsTr("Save as collection")
                focusPolicy: Qt.StrongFocus
                onClicked: root.collectionDialogRequested("save", "", "")
                FocusRing {}
            }

            SettingsButton {
                Layout.fillWidth: true
                text: qsTr("Clear filters")
                enabled: Library.filtersActive || (General.railHidden && Library.platformFilter !== "")
                focusPolicy: Qt.StrongFocus
                onClicked: {
                    Library.clearFilters()
                    if (General.railHidden)
                        Library.platformFilter = ""
                }
                FocusRing {}
            }
        }
    }
}
