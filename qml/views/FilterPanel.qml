import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components/TagSearch.js" as TagSearch

// See docs/code/qml.md.
MotionPopup {
    id: root

    signal closedByKey()
    signal collectionDialogRequested(string mode, string id, string name)

    readonly property var directionNames: ({
        title: [qsTr("A to Z"), qsTr("Z to A")],
        last_played: [qsTr("Oldest first"), qsTr("Newest first")],
        play_time: [qsTr("Least played first"), qsTr("Most played first")],
        rating: [qsTr("Lowest first"), qsTr("Highest first")],
        year: [qsTr("Oldest first"), qsTr("Newest first")],
        achievements: [qsTr("Least complete first"), qsTr("Most complete first")],
        added: [qsTr("Oldest first"), qsTr("Newest first")],
        size: [qsTr("Smallest first"), qsTr("Largest first")],
        platform: [qsTr("Rail order"), qsTr("Reverse order")],
    })
    readonly property var sortOrders: [
        { id: "title", name: qsTr("Title") },
        { id: "last_played", name: qsTr("Last played") },
        { id: "play_time", name: qsTr("Play time") },
        { id: "rating", name: qsTr("Rating") },
        { id: "year", name: qsTr("Release") },
        { id: "added", name: qsTr("Added") },
        { id: "achievements", name: qsTr("Achievements") },
        { id: "size", name: qsTr("Size") },
        { id: "platform", name: qsTr("Platform") },
    ]
    readonly property var sortValueNames: ({
        last_played: qsTr("Last played"),
        play_time: qsTr("Play time"),
        rating: qsTr("Rating"),
        year: qsTr("Year"),
        achievements: qsTr("Achievements"),
        added: qsTr("Added"),
        size: qsTr("Size"),
    })
    readonly property var groupings: [
        { id: "platform", name: qsTr("Platform") },
        { id: "status", name: qsTr("Status") },
        { id: "year", name: qsTr("Year") },
        { id: "rating", name: qsTr("Rating") },
        { id: "last_played", name: qsTr("Last played") },
        { id: "none", name: qsTr("None") },
    ]
    readonly property var selectedTags: Library.tagFilter.filter(t => Library.tags.some(tag => tag.name === t))
    readonly property var usableTags: Library.tags.filter(t => t.games > 0 || Library.tagFilter.includes(t.name))
    property string tagQuery: ""
    // A selected tag stays shown, so a filter that is on never hides.
    readonly property var shownTags: TagSearch.matching(usableTags, tagQuery, Library.matchesSearch, Library.tagFilter)

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

    readonly property int baseWidth: General.railHidden ? Theme.filters.panelWidthRailHidden : Theme.filters.panelWidth
    readonly property int fullWidth: Math.max(baseWidth, Theme.filters.moreWidth)
    property real shownWidth: Library.moreOpen ? fullWidth : baseWidth
    Behavior on shownWidth { enabled: Motion.on && root.visible; MotionNumber { speed: "normal" } }
    width: shownWidth
    readonly property int scrollRoom: scrollBar.visible ? scrollBar.width + Theme.layout.spacing : 0
    readonly property int firstWidth: baseWidth - 2 * Theme.filters.padding - (Library.moreOpen ? 0 : scrollRoom)
    readonly property int moreWidth: fullWidth - baseWidth - Theme.filters.columnGap - scrollRoom

    readonly property var lastPlayedChoices: [
        { id: "", name: qsTr("Any") },
        { id: "week", name: qsTr("This week") },
        { id: "month", name: qsTr("This month") },
        { id: "year", name: qsTr("This year") },
        { id: "older", name: qsTr("Over a year ago") },
        { id: "never", name: qsTr("Never") },
    ]
    readonly property int moreActive: (Library.yearFrom > 0 || Library.yearTo > 0 ? 1 : 0)
        + (Library.minRating >= 0 ? 1 : 0) + Library.playTimeFilter.length
        + (Library.lastPlayedFilter !== "" ? 1 : 0) + Library.achievementFilter.length
    readonly property var yearCounts: Library.yearHistogram.map(b => b.games)
    readonly property int yearMin: Library.yearHistogram.length > 0 ? Library.yearHistogram[0].year : 0
    readonly property int yearMax: yearMin + Math.max(0, yearCounts.length - 1)
    readonly property int yearLow: Library.yearFrom > 0 ? Math.max(yearMin, Math.min(yearMax, Library.yearFrom)) : yearMin
    readonly property int yearHigh: Library.yearTo > 0 ? Math.max(yearMin, Math.min(yearMax, Library.yearTo)) : yearMax
    readonly property bool yearOn: Library.yearFrom > 0 || Library.yearTo > 0

    function togglePlayTime(id) {
        const times = Library.playTimeFilter.slice()
        const i = times.indexOf(id)
        if (i >= 0)
            times.splice(i, 1)
        else
            times.push(id)
        Library.playTimeFilter = times
    }

    function toggleAchievement(id) {
        const states = Library.achievementFilter.slice()
        const i = states.indexOf(id)
        if (i >= 0)
            states.splice(i, 1)
        else
            states.push(id)
        Library.achievementFilter = states
    }

    readonly property int swellRoom: Math.min(4, Theme.filters.padding)
    padding: Theme.filters.padding - swellRoom
    modal: false
    focus: true
    closePolicy: Popup.CloseOnPressOutsideParent

    onOpened: root.forceActiveFocus(Qt.TabFocusReason)
    onClosed: tagQuery = ""

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

    component Segment: ToggleChip {
        Layout.fillWidth: true
        Layout.preferredWidth: 1
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
        implicitHeight: Math.max(content.implicitHeight, more.implicitHeight) + 2 * root.swellRoom
        leftMargin: root.swellRoom
        rightMargin: root.swellRoom
        topMargin: root.swellRoom
        bottomMargin: root.swellRoom
        contentWidth: width
        contentHeight: Math.max(content.implicitHeight, more.implicitHeight)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
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
            width: root.firstWidth
            spacing: Theme.filters.spacing

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
                GridLayout {
                    id: sortRow
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

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
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    Repeater {
                        model: root.groupings

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
                        enabled: Library.sort in root.sortValueNames
                        text: root.sortValueNames[Library.sort] ?? qsTr("Last played")
                        tip: qsTr("Show what the list is sorted by on rows")
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
                SettingsSearchField {
                    id: tagSearch
                    Layout.preferredHeight: Theme.filters.tagSearchHeight
                    visible: Library.tagsOpen && root.usableTags.length > 0
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search tags (%1)").arg(root.usableTags.length)
                    text: root.tagQuery
                    onTextChanged: root.tagQuery = text
                    Keys.onEscapePressed: event => {
                        event.accepted = text !== ""
                        if (event.accepted)
                            clear()
                    }
                }
                Label {
                    visible: Library.tagsOpen && root.usableTags.length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("No game has a tag yet. Add them in a game's settings.")
                    color: Theme.filters.hintColor
                }
                Label {
                    visible: Library.tagsOpen && root.usableTags.length > 0 && root.shownTags.length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("No tag called \"%1\".").arg(root.tagQuery.trim())
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

            AbstractButton {
                id: moreButton
                Layout.fillWidth: true
                implicitHeight: moreRow.implicitHeight + 2 * Theme.layout.spacing
                leftPadding: Theme.layout.spacing
                rightPadding: Theme.layout.spacing
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                onClicked: Library.moreOpen = !Library.moreOpen
                PressDip { pressed: moreButton.pressed }

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Theme.filters.sortRadius
                    color: moreButton.hovered ? Theme.filters.sortHover : Theme.filters.sortBackground
                }
                contentItem: RowLayout {
                    id: moreRow
                    spacing: Theme.layout.spacing

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            text: qsTr("More filters")
                            color: Theme.filters.text
                            font.weight: Theme.font.emphasisWeight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Year, rating, play time, achievements")
                            elide: Text.ElideRight
                            color: Theme.filters.hintColor
                            font.pixelSize: Theme.filters.headingFontSize
                        }
                    }
                    Rectangle {
                        visible: root.moreActive > 0
                        implicitWidth: Math.max(implicitHeight, countLabel.implicitWidth + Theme.filters.chipPadding)
                        implicitHeight: Theme.filters.chipHeight
                        radius: Theme.filters.chipRadius
                        color: Theme.filters.chipBackground

                        Label {
                            id: countLabel
                            anchors.centerIn: parent
                            text: root.moreActive
                            color: Theme.filters.chipText
                            font.pixelSize: Theme.filters.chipFontSize
                        }
                    }
                    Icon {
                        path: Icons.chevronRight
                        size: Theme.filters.headingFontSize + 3
                        color: Theme.filters.hintColor
                    }
                }
                FocusRing {}
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

        ColumnLayout {
            id: more
            visible: Library.moreOpen || root.shownWidth > root.baseWidth + 1
            x: root.firstWidth + Theme.filters.columnGap
            width: root.moreWidth
            spacing: Theme.filters.spacing

            AbstractButton {
                id: fewer
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                onClicked: Library.moreOpen = false
                background: null
                contentItem: RowLayout {
                    spacing: 4
                    Icon {
                        path: Icons.chevronLeft
                        size: Theme.filters.headingFontSize + 3
                        color: fewerLabel.color
                    }
                    Label {
                        id: fewerLabel
                        text: qsTr("Fewer")
                        color: fewer.hovered ? Theme.filters.text : Theme.filters.hintColor
                        font.pixelSize: Theme.filters.headingFontSize
                    }
                }
                FocusRing {}
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                RowLayout {
                    Layout.fillWidth: true
                    Heading {
                        Layout.fillWidth: true
                        title: qsTr("Release year")
                    }
                    Label {
                        visible: root.yearCounts.length > 0
                        text: !root.yearOn ? qsTr("Any")
                            : root.yearLow === root.yearHigh ? String(root.yearLow) : root.yearLow + " – " + root.yearHigh
                        color: root.yearOn ? Theme.filters.text : Theme.filters.hintColor
                        font.weight: Theme.font.emphasisWeight
                    }
                }
                RangeHistogram {
                    visible: root.yearCounts.length > 0
                    Layout.fillWidth: true
                    counts: root.yearCounts
                    minValue: root.yearMin
                    range: true
                    low: root.yearLow
                    high: root.yearHigh
                    lowName: qsTr("From year")
                    minText: String(root.yearMin)
                    maxText: String(root.yearMax)
                    tipText: (year, games) => qsTr("%1 · %n game(s)", "", games).arg(year)
                    highName: qsTr("To year")
                    onMoved: (lo, hi) => {
                        const all = lo <= root.yearMin && hi >= root.yearMax
                        Library.yearFrom = all ? 0 : lo
                        Library.yearTo = all ? 0 : hi
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.filters.hintColor
                    text: root.yearCounts.length === 0 ? qsTr("No game has a release year yet.")
                        : Library.gamesWithoutYear > 0
                          ? qsTr("%n game(s) with no year are left out while a range is set.", "", Library.gamesWithoutYear)
                          : ""
                    visible: text !== ""
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                RowLayout {
                    Layout.fillWidth: true
                    Heading {
                        Layout.fillWidth: true
                        title: qsTr("Rating")
                    }
                    Label {
                        text: Library.minRating < 0 ? qsTr("Any") : qsTr("%1 or more").arg(Library.minRating)
                        color: Library.minRating < 0 ? Theme.filters.hintColor : Theme.filters.text
                        font.weight: Theme.font.emphasisWeight
                    }
                }
                RangeHistogram {
                    Layout.fillWidth: true
                    counts: Library.ratingHistogram.map(b => b.games)
                    step: 0.5
                    low: Math.max(0, Library.minRating)
                    lowName: qsTr("Lowest rating")
                    minText: "0"
                    maxText: "5"
                    tipText: (rating, games) => qsTr("%1 · %n game(s)", "", games).arg(rating)
                    onMoved: lo => Library.minRating = lo <= 0 ? -1 : lo
                }
                Label {
                    visible: Library.gamesWithoutRating > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.filters.hintColor
                    text: qsTr("%n game(s) unrated are left out while a minimum is set.", "", Library.gamesWithoutRating)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Play time") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    Repeater {
                        model: Library.playTimeChoices

                        Segment {
                            required property var modelData
                            text: modelData.name
                            detail: modelData.games
                            selected: Library.playTimeFilter.includes(modelData.id)
                            onClicked: root.togglePlayTime(modelData.id)
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Last played") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    Repeater {
                        model: root.lastPlayedChoices

                        Segment {
                            required property var modelData
                            text: modelData.name
                            selected: Library.lastPlayedFilter === modelData.id
                            onClicked: Library.lastPlayedFilter = modelData.id
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Heading { title: qsTr("Achievements") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 4

                    Repeater {
                        model: Library.achievementChoices

                        Segment {
                            required property var modelData
                            text: modelData.name
                            detail: modelData.games
                            selected: Library.achievementFilter.includes(modelData.id)
                            onClicked: root.toggleAchievement(modelData.id)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.filters.hintColor
                    text: qsTr("Achievements are read from Steam. Other games count as None to earn.")
                }
            }
        }
    }
}
