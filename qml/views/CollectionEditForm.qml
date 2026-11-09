import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

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

    readonly property var rules: ({
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

    component Group: ColumnLayout {
        property string title
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            text: parent.title
            color: Theme.collections.editSectionHeading
            font.weight: Theme.font.strongWeight
            font.pixelSize: Theme.filters.headingFontSize
            font.letterSpacing: Theme.filters.headingUppercase ? 0.8 : 0
            font.capitalization: Theme.filters.headingUppercase ? Font.AllUppercase : Font.MixedCase
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.collections.editSectionRule
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

    Flickable {
        id: form
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: formColumn.implicitHeight
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
                        visible: Library.tags.length > 0
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing * 2

                            Repeater {
                                model: Library.tags

                                SettingsCheckBox {
                                    required property var modelData
                                    bordered: false
                                    text: modelData.name
                                    checked: root.tags.includes(modelData.name)
                                    onToggled: root.tags = root.toggled(root.tags, modelData.name)
                                }
                            }
                        }
                        SettingsCheckBox {
                            visible: root.tags.length >= 2
                            bordered: false
                            text: qsTr("Match all selected tags")
                            checked: root.matchAllTags
                            onToggled: root.matchAllTags = checked
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing
                visible: root.added.length > 0

                Label {
                    text: root.rulesOn ? qsTr("Added by hand") : qsTr("Games")
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                }
                CheckList {
                    Layout.fillWidth: true
                    model: root.gameRows(root.added)
                    ticked: root.kept(root.added, root.dropAdded)
                    maxListHeight: Theme.collections.editListHeight
                    onToggled: id => root.dropAdded = root.toggled(root.dropAdded, id)
                }
                Hint { text: qsTr("Uncheck a game to take it out of the collection.") }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing
                visible: root.rulesOn && root.removed.length > 0

                Label {
                    text: qsTr("Left out")
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                }
                CheckList {
                    Layout.fillWidth: true
                    model: root.gameRows(root.removed)
                    ticked: root.kept(root.removed, root.dropRemoved)
                    maxListHeight: Theme.collections.editListHeight
                    onToggled: id => root.dropRemoved = root.toggled(root.dropRemoved, id)
                }
                Hint { text: qsTr("Uncheck a game to let the rules include it again.") }
            }
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
                text: root.matches === 1 ? qsTr("1 game matches") : qsTr("%n game(s) match", "", root.matches)
                color: Theme.gameEditor.headingColor
                font.weight: Theme.font.strongWeight
            }
            SettingsButton {
                visible: root.rulesOn && root.hasRules
                text: qsTr("Clear rules")
                onClicked: root.clearRules()
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
