import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components/TagSearch.js" as TagSearch

// See docs/code/qml.md.
ColumnLayout {
    id: root

    property string collectionId
    spacing: Theme.gameEditor.rowSpacing
    // Closes the dialog; the text is what Library.editCollection refused with, if anything.
    signal finished(string error)
    signal cancelled()

    property string name
    property bool rulesOn: false
    property string tab: "rules"
    property string platform
    property bool installedOnly: false
    property bool favoritesOnly: false
    property var statuses: []
    property var tags: []
    property bool matchAllTags: false
    property var steam: []
    property int yearFrom: 0
    property int yearTo: 0
    property real minRating: -1
    property real savedRating: -1
    property var playTimes: []
    property string lastPlayed: ""
    property var achievements: []
    property var added: []
    property var removed: []
    property var dropAdded: []
    property var dropRemoved: []
    property int matches: 0
    property string sort: ""
    property bool sortDescending: false
    property string groupBy: ""

    readonly property var sortChoices: [
        { id: "", name: qsTr("Keep current") },
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
    readonly property var groupChoices: [
        { id: "", name: qsTr("Keep current") },
        { id: "platform", name: qsTr("Platform") },
        { id: "status", name: qsTr("Status") },
        { id: "year", name: qsTr("Year") },
        { id: "rating", name: qsTr("Rating") },
        { id: "last_played", name: qsTr("Last played") },
        { id: "none", name: qsTr("None") },
    ]
    readonly property var directionChoices: [
        { id: false, name: qsTr("Ascending") },
        { id: true, name: qsTr("Descending") },
    ]

    readonly property var rules: ({
        sort: sort, sortDescending: sortDescending, groupBy: groupBy,
        enabled: rulesOn, platform: platform, installedOnly: installedOnly, favoritesOnly: favoritesOnly,
        statuses: statuses, tags: tags, matchAllTags: matchAllTags && tags.length >= 2, steam: steam,
        yearFrom: yearFrom, yearTo: yearTo, minRating: minRating, playTimes: playTimes,
        lastPlayed: lastPlayed, achievements: achievements
    })
    readonly property bool hasRules: platform !== "" || installedOnly || favoritesOnly || statuses.length > 0
        || tags.length > 0 || steam.length > 0 || yearFrom > 0 || yearTo > 0 || minRating >= 0
        || playTimes.length > 0 || lastPlayed !== "" || achievements.length > 0

    readonly property var platformChoices: {
        const choices = [{ id: "", name: qsTr("Any platform") }]
        for (const p of Library.platforms)
            choices.push({ id: p.id, name: p.name })
        if (platform !== "" && !choices.some(c => c.id === platform))
            choices.push({ id: platform, name: platform })
        return choices
    }

    readonly property var onlyChoices: [
        { id: "installed", name: qsTr("Installed") },
        { id: "favorites", name: qsTr("Favorites") },
        { id: "owned", name: qsTr("Owned") },
        { id: "family", name: qsTr("Family") },
    ]
    readonly property var onlyPicked: {
        const on = steam.filter(s => s === "owned" || s === "family")
        if (installedOnly)
            on.push("installed")
        if (favoritesOnly)
            on.push("favorites")
        return on
    }

    readonly property var playTimeNames: ({
        never: qsTr("Never"), under_2h: qsTr("< 2 h"), "2_10h": qsTr("2–10"), "10_50h": qsTr("10–50"),
        "50_100h": qsTr("50–100"), over_100h: qsTr("100 h +")
    })
    readonly property var playTimeModel: Library.playTimeChoices.map(c => ({ id: c.id, name: playTimeNames[c.id] ?? c.name }))

    readonly property var lastPlayedChoices: [
        { id: "", name: qsTr("Any time") },
        { id: "week", name: qsTr("Week") },
        { id: "month", name: qsTr("Month") },
        { id: "year", name: qsTr("Year") },
        { id: "older", name: qsTr("Older"), tip: qsTr("Over a year ago") },
        { id: "never", name: qsTr("Never"), tip: qsTr("Never played") },
    ]

    // A half step nobody's game is rated in only shows while the collection has it saved.
    readonly property var ratingChoices: {
        const histogram = Library.ratingHistogram
        const choices = [{ id: -1, name: qsTr("Any") }]
        for (let step = 0; step <= 10; step++) {
            const r = step / 2
            if (r !== savedRating && r !== minRating && (histogram[step]?.games ?? 0) === 0)
                continue
            choices.push({ id: r, name: step === 10 ? "5" : qsTr("%1+").arg(r),
                           tip: step === 0 ? qsTr("Any game that has a rating") : "" })
        }
        return choices
    }

    // Chosen tags are chips above the field, so the matches only offer the others.
    property int tagHighlight: 0
    readonly property var tagMatches: TagSearch.matching(
        Library.tags.filter(t => t.games > 0 && !tags.includes(t.name)), tagSearch.text, Library.matchesSearch, [])

    function tagColor(name) {
        const palette = Theme.tags.palette
        const tag = Library.tags.find(t => t.name === name)
        return palette.length > 0 && tag ? palette[tag.color % palette.length] : "transparent"
    }

    function addTag(name) {
        if (!name || tags.includes(name))
            return
        tags = tags.concat([name])
        tagHighlight = 0
        tagSearch.forceActiveFocus()
    }

    function toggled(list, value) {
        const next = list.slice()
        const i = next.indexOf(value)
        if (i >= 0)
            next.splice(i, 1)
        else
            next.push(value)
        return next
    }

    function pickOnly(id) {
        if (id === "installed")
            installedOnly = !installedOnly
        else if (id === "favorites")
            favoritesOnly = !favoritesOnly
        else
            steam = toggled(steam, id)
    }

    function load() {
        const d = Library.collectionDetails(collectionId)
        name = d.name ?? ""
        rulesOn = d.enabled ?? false
        tab = "rules"
        sort = d.sort ?? ""
        sortDescending = d.sortDescending ?? false
        groupBy = d.groupBy ?? ""
        platform = d.platform ?? ""
        installedOnly = d.installedOnly ?? false
        favoritesOnly = d.favoritesOnly ?? false
        statuses = d.statuses ?? []
        tags = d.tags ?? []
        matchAllTags = d.matchAllTags ?? false
        steam = d.steam ?? []
        yearFrom = d.yearFrom ?? 0
        yearTo = d.yearTo ?? 0
        minRating = d.minRating ?? -1
        savedRating = minRating
        playTimes = d.playTimes ?? []
        lastPlayed = d.lastPlayed ?? ""
        achievements = d.achievements ?? []
        added = d.added ?? []
        removed = d.removed ?? []
        dropAdded = []
        dropRemoved = []
        recount()
        form.forceActiveFocus()
    }

    function clearRules() {
        platform = ""
        installedOnly = false
        favoritesOnly = false
        statuses = []
        tags = []
        matchAllTags = false
        steam = []
        yearFrom = 0
        yearTo = 0
        minRating = -1
        playTimes = []
        lastPlayed = ""
        achievements = []
    }

    function recount() {
        recountTimer.stop()
        matches = Library.countCollectionMatches(collectionId, rules, dropAdded, dropRemoved)
    }

    function submit() {
        finished(Library.editCollection(collectionId, rules, dropAdded, dropRemoved))
    }

    onRulesChanged: recountTimer.restart()
    onDropAddedChanged: recountTimer.restart()
    onDropRemovedChanged: recountTimer.restart()

    Timer {
        id: recountTimer
        interval: 100
        onTriggered: root.recount()
    }

    // A game that isn't in the library has only its id: Steam's are shown as the app they name.
    function gameRows(games) {
        return games.map(g => {
            const missing = g.title === g.id
            return {
                id: g.id,
                title: !missing ? g.title
                    : g.id.startsWith("steam-") ? qsTr("Steam app %1").arg(g.id.slice(6)) : g.id,
                info: missing ? qsTr("Not in your library") : ""
            }
        })
    }

    function kept(games, drops) {
        return games.map(g => g.id).filter(id => !drops.includes(id))
    }

    component Hint: Label {
        Layout.fillWidth: true
        color: Theme.gameEditor.hintColor
        wrapMode: Text.WordWrap
    }

    // A tab of games with a tick each. It takes the page's height when the tabs are there; without them
    // it is as tall as its list, which the page then sizes the dialog by.
    component ListPage: ColumnLayout {
        id: page
        property var games: []
        property var drops: []
        property string empty
        property string hint
        signal toggled(string id)

        Layout.preferredHeight: root.rulesOn ? 0 : implicitHeight
        spacing: Theme.layout.spacing

        CheckList {
            visible: page.games.length > 0
            Layout.fillWidth: true
            model: root.gameRows(page.games)
            ticked: root.kept(page.games, page.drops)
            maxListHeight: root.rulesOn ? page.height - pageHint.implicitHeight - page.spacing : Theme.collections.editListHeight
            onToggled: id => page.toggled(id)
        }
        Hint {
            visible: page.games.length === 0
            text: page.empty
        }
        Hint {
            id: pageHint
            visible: page.games.length > 0
            text: page.hint
        }
        Item { Layout.fillHeight: true }
    }

    component Group: ColumnLayout {
        property string title
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                text: parent.parent.title
                color: Theme.collections.editSectionHeading
                opacity: Theme.collections.editSectionHeadingOpacity
                font.weight: Theme.font.strongWeight
                font.pixelSize: Theme.filters.headingFontSize
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.collections.editSectionRule
            }
        }
    }

    // The name in a column of its own, the controls beside it; `tip` adds an info mark that explains the rule.
    component Rule: RowLayout {
        id: rule
        property string label
        property string tip
        default property alias content: body.data

        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        RowLayout {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Theme.collections.editLabelWidth
            Layout.minimumWidth: Theme.collections.editLabelWidth
            Layout.maximumWidth: Theme.collections.editLabelWidth
            Layout.preferredHeight: Theme.filters.sortHeight
            spacing: Theme.layout.spacing / 2

            Label {
                id: ruleLabel
                Layout.fillWidth: true
                text: rule.label
                elide: Text.ElideRight
                color: Theme.gameEditor.hintColor
            }
            Icon {
                visible: rule.tip !== ""
                path: Icons.informationLine
                size: Theme.filters.headingFontSize + 3
                color: Theme.gameEditor.hintColor
            }
            HoverHandler { id: ruleHover }
            ToolTip.visible: rule.tip !== "" && ruleHover.hovered
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: rule.tip
        }
        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.layout.spacing
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                text: qsTr("Edit collection")
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.font.smallSize
            }
            Label {
                Layout.fillWidth: true
                text: root.name
                elide: Text.ElideRight
                color: Theme.gameEditor.headingColor
                font.weight: Theme.font.strongWeight
                font.pixelSize: Theme.gameEditor.headingFontSize
            }
        }
        Label {
            text: qsTr("Auto-update")
            color: Theme.gameEditor.hintColor
        }
        SettingsSwitch {
            checked: root.rulesOn
            onToggled: root.rulesOn = checked
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            text: qsTr("Sort")
            color: Theme.gameEditor.hintColor
        }
        SettingsComboBox {
            Layout.fillWidth: true
            model: root.sortChoices.map(c => c.name)
            currentIndex: Math.max(0, root.sortChoices.findIndex(c => c.id === root.sort))
            onActivated: index => {
                const id = root.sortChoices[index].id
                root.sort = id
                root.sortDescending = id !== "" && id !== "title" && id !== "platform"
            }
        }
        SettingsComboBox {
            visible: root.sort !== ""
            model: root.directionChoices.map(c => c.name)
            currentIndex: root.sortDescending ? 1 : 0
            onActivated: index => root.sortDescending = index === 1
        }
        Label {
            text: qsTr("Group by")
            color: Theme.gameEditor.hintColor
        }
        SettingsComboBox {
            Layout.fillWidth: true
            model: root.groupChoices.map(c => c.name)
            currentIndex: Math.max(0, root.groupChoices.findIndex(c => c.id === root.groupBy))
            onActivated: index => root.groupBy = root.groupChoices[index].id
        }
    }

    TabRow {
        visible: root.rulesOn
        Layout.fillWidth: true
        model: [
            { id: "rules", name: qsTr("Rules") },
            { id: "added", name: qsTr("Added by hand"), count: root.kept(root.added, root.dropAdded).length },
            { id: "removed", name: qsTr("Left out"), count: root.kept(root.removed, root.dropRemoved).length },
        ]
        current: root.tab
        onPicked: id => root.tab = id
    }
    Label {
        visible: !root.rulesOn
        text: qsTr("Games")
        color: Theme.gameEditor.headingColor
        font.weight: Theme.font.strongWeight
    }

    // The page sizes the dialog by the Rules page; with the rules off it is the list's own size.
    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: !root.rulesOn ? 1 : root.tab === "added" ? 1 : root.tab === "removed" ? 2 : 0

        Flickable {
            id: form
            Layout.preferredHeight: root.rulesOn ? formColumn.implicitHeight : 0
            contentHeight: formColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar { id: formBar; trackless: true }
            Keys.onEscapePressed: root.cancelled()

            ColumnLayout {
                id: formColumn
                width: form.width - (formBar.visible ? formBar.thickness + Theme.layout.spacing : 0)
                spacing: Theme.collections.editSectionGap

                ColumnLayout {
                    visible: root.rulesOn
                    Layout.fillWidth: true
                    spacing: Theme.collections.editSectionGap

                    Group {
                        title: qsTr("Library")

                        Rule {
                            label: qsTr("Platform")
                            SettingsComboBox {
                                Layout.fillWidth: true
                                model: root.platformChoices.map(c => c.name)
                                currentIndex: Math.max(0, root.platformChoices.findIndex(c => c.id === root.platform))
                                onActivated: index => root.platform = root.platformChoices[index].id
                            }
                        }
                        Rule {
                            label: qsTr("Only")
                            ChoiceChips {
                                Layout.fillWidth: true
                                multiple: true
                                model: root.onlyChoices
                                current: root.onlyPicked
                                onPicked: id => root.pickOnly(id)
                            }
                        }
                    }
                    Group {
                        title: qsTr("Progress")

                        Rule {
                            label: qsTr("Status")
                            ChoiceChips {
                                Layout.fillWidth: true
                                multiple: true
                                model: Library.statuses
                                current: root.statuses
                                onPicked: id => root.statuses = root.toggled(root.statuses, id)
                            }
                        }
                        Rule {
                            label: qsTr("Play time")
                            ChoiceChips {
                                Layout.fillWidth: true
                                multiple: true
                                model: root.playTimeModel
                                current: root.playTimes
                                onPicked: id => root.playTimes = root.toggled(root.playTimes, id)
                            }
                        }
                        Rule {
                            label: qsTr("Last played")
                            ChoiceChips {
                                Layout.fillWidth: true
                                model: root.lastPlayedChoices
                                current: root.lastPlayed
                                onPicked: id => root.lastPlayed = id
                            }
                        }
                        Rule {
                            label: qsTr("Achievements")
                            tip: qsTr("Achievements are read from Steam; other games have none to earn.")
                            ChoiceChips {
                                Layout.fillWidth: true
                                multiple: true
                                model: Library.achievementChoices
                                current: root.achievements
                                onPicked: id => root.achievements = root.toggled(root.achievements, id)
                            }
                        }
                    }
                    Group {
                        title: qsTr("Details")

                        Rule {
                            label: qsTr("Released")
                            tip: qsTr("Games with no year are left out while a year is set.")
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing

                                SettingsTextField {
                                    Layout.preferredWidth: Theme.collections.editYearWidth
                                    placeholderText: qsTr("From")
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
                                    text: root.yearFrom > 0 ? String(root.yearFrom) : ""
                                    onTextEdited: root.yearFrom = text === "" ? 0 : parseInt(text)
                                }
                                Label {
                                    text: qsTr("to")
                                    color: Theme.gameEditor.hintColor
                                }
                                SettingsTextField {
                                    Layout.preferredWidth: Theme.collections.editYearWidth
                                    placeholderText: qsTr("To")
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
                                    text: root.yearTo > 0 ? String(root.yearTo) : ""
                                    onTextEdited: root.yearTo = text === "" ? 0 : parseInt(text)
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }
                        Rule {
                            label: qsTr("Rating")
                            ChoiceChips {
                                Layout.fillWidth: true
                                model: root.ratingChoices
                                current: root.minRating
                                onPicked: id => root.minRating = id
                            }
                        }
                        Rule {
                            label: qsTr("Tags")
                            visible: Library.tags.length > 0 || root.tags.length > 0

                            Flow {
                                visible: root.tags.length > 0
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing

                                Repeater {
                                    model: root.tags

                                    FilterChip {
                                        required property string modelData
                                        text: modelData
                                        tagColor: root.tagColor(modelData)
                                        onClicked: root.tags = root.toggled(root.tags, modelData)
                                        onRemoved: root.tags = root.toggled(root.tags, modelData)
                                    }
                                }
                            }
                            SettingsSearchField {
                                id: tagSearch
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.filters.tagSearchHeight
                                placeholderText: qsTr("Search tags to add")
                                onTextChanged: root.tagHighlight = 0
                                Keys.onDownPressed: root.tagHighlight = Math.min(root.tagMatches.length - 1, root.tagHighlight + 3)
                                Keys.onUpPressed: root.tagHighlight = Math.max(0, root.tagHighlight - 3)
                                Keys.onReturnPressed: root.addTag(root.tagMatches[root.tagHighlight]?.name)
                                Keys.onEnterPressed: root.addTag(root.tagMatches[root.tagHighlight]?.name)
                                Keys.onEscapePressed: event => {
                                    event.accepted = text !== ""
                                    if (event.accepted)
                                        clear()
                                }
                            }
                            Label {
                                visible: tagSearch.text.trim() !== "" && root.tagMatches.length === 0
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("No other tag called \"%1\".").arg(tagSearch.text.trim())
                                color: Theme.gameEditor.hintColor
                            }
                            GridLayout {
                                visible: (tagSearch.activeFocus || tagSearch.text.trim() !== "") && root.tagMatches.length > 0
                                Layout.fillWidth: true
                                columns: 3
                                rowSpacing: Theme.filters.tagSpacing
                                columnSpacing: Theme.filters.tagSpacing

                                Repeater {
                                    model: parent.visible ? root.tagMatches : []

                                    AbstractButton {
                                        id: pick
                                        required property var modelData
                                        required property int index

                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        implicitHeight: Theme.tags.height
                                        focusPolicy: Qt.NoFocus
                                        hoverEnabled: true
                                        onClicked: root.addTag(modelData.name)
                                        ToolTip.visible: hovered && chip.truncated
                                        ToolTip.delay: Theme.tooltip.delay
                                        ToolTip.text: modelData.name
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }

                                        background: null
                                        contentItem: TagChip {
                                            id: chip
                                            name: pick.modelData.name
                                            spread: true
                                            plus: false
                                            detail: pick.modelData.games
                                            textColor: Qt.hsla(chip.tagColor.hslHue, chip.tagColor.hslSaturation, Theme.filters.tagTextLightness, 1)
                                            detailColor: Qt.alpha(textColor, Theme.filters.tagCountOpacity)
                                            colorIndex: pick.modelData.color
                                            outlined: true
                                            outlineWidth: Theme.filters.tagBorderWidth
                                            fillOpacity: pick.hovered || pick.index === root.tagHighlight ? Theme.filters.tagHoverOpacity : Theme.filters.tagOffOpacity
                                            lit: pick.hovered || pick.index === root.tagHighlight
                                            raised: pick.hovered
                                            raiseBy: Theme.filters.tagHoverLift
                                        }
                                    }
                                }
                            }
                            RowLayout {
                                visible: root.tags.length >= 2
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing

                                Label {
                                    text: qsTr("Match")
                                    color: Theme.gameEditor.hintColor
                                }
                                ChoiceChips {
                                    model: [{ id: "any", name: qsTr("Any") }, { id: "all", name: qsTr("All") }]
                                    current: root.matchAllTags ? "all" : "any"
                                    onPicked: id => root.matchAllTags = id === "all"
                                }
                            }
                        }
                    }
                }
            }
        }

        ListPage {
            games: root.added
            drops: root.dropAdded
            empty: root.rulesOn ? qsTr("No games were added by hand.") : qsTr("No games.")
            hint: qsTr("Uncheck a game to take it out of the collection.")
            onToggled: id => root.dropAdded = root.toggled(root.dropAdded, id)
        }
        ListPage {
            games: root.removed
            drops: root.dropRemoved
            empty: qsTr("No games are left out.")
            hint: qsTr("Uncheck a game to let the rules include it again.")
            onToggled: id => root.dropRemoved = root.toggled(root.dropRemoved, id)
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: footer.implicitHeight + Theme.gameEditor.rowSpacing
        color: Theme.collections.editFooterBackground

        RowLayout {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: Theme.layout.spacing

            Label {
                // Only the number keeps the heading color; the rest of the sentence is muted.
                readonly property string sentence: root.matches === 1 ? qsTr("1 game matches") : qsTr("%n game(s) match", "", root.matches)
                textFormat: Text.StyledText
                text: sentence.replace(/^(\d+)/, "<font color=\"" + Theme.gameEditor.headingColor + "\"><b>$1</b></font>")
                color: Theme.collections.editFooterText
                Accessible.name: sentence
            }
            Label {
                visible: root.rulesOn && root.hasRules
                text: "·"
                color: Theme.collections.editFooterText
            }
            AbstractButton {
                id: clearButton
                visible: root.rulesOn && root.hasRules
                text: qsTr("Clear rules")
                padding: 0
                hoverEnabled: true
                onClicked: root.clearRules()
                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: null
                contentItem: Label {
                    text: clearButton.text
                    color: clearButton.hovered ? Theme.gameEditor.headingColor : Theme.collections.editFooterText
                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                }
            }
            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.cancelled()
            }
            SettingsButton {
                primary: true
                text: qsTr("Save")
                onClicked: root.submit()
            }
        }
    }
}
