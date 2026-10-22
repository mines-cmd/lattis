import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("What the game's page shows and in what order. The look of each part is in Customize.")

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

    // The game selected in the main window, for the hints' examples.
    readonly property var game: Window.window?.game ?? ({})

    GameFields {
        id: fields
        game: root.game
        needsHistory: true
        // Its game is set from the start, so the key it reads on never changes for the first one.
        Component.onCompleted: readPlayed()
    }

    readonly property var periods: ["week", "two_weeks", "month", "year"]
    readonly property var playTimeFormats: ["hours", "hours_minutes"]
    // The selected game's play time when it has one, so the choices show its own numbers.
    readonly property int formatExample: (root.game.playMinutes ?? 0) > 0 ? root.game.playMinutes : 863

    function valueOf(name) {
        const field = fields.all[name]
        return field === undefined ? "" : field.value
    }

    // An example from the selected game when it has a value, what the entry is otherwise.
    function hint(name, about, note) {
        const value = valueOf(name)
        const shown = value === "" ? about : name === "rating" ? qsTr("%1 out of 5").arg(value) : value
        return note !== undefined ? shown + " · " + note : shown
    }

    readonly property var fieldDefs: ({
        play_time: { icon: Icons.timeLine,
                     hint: hint("play_time", qsTr("The time played in all"), qsTr("click opens play history")) },
        last_played: { icon: Icons.history,
                       hint: hint("last_played", qsTr("The day of the last session")) },
        recent_play_time: { icon: Icons.timerLine,
                            hint: [valueOf("recent_play_time"), qsTr("%1, set below").arg(fields.periodLabels[General.recentPeriod])]
                                  .filter(t => t !== "").join(" · ") },
        times_played: { icon: Icons.loopRightLine,
                        hint: hint("times_played", qsTr("The number of sessions")) },
        first_played: { icon: Icons.signpostLine,
                        hint: hint("first_played", qsTr("The day of the earliest session the launcher knows of")) },
        release_date: { icon: Icons.calendarEventLine,
                        hint: hint("release_date", qsTr("The year it came out")) },
        platform: { icon: Icons.gamepadLine,
                    hint: hint("platform", qsTr("The platform it's on"), qsTr("click shows only that platform")) },
        status: { icon: Icons.flagLine,
                  hint: hint("status", qsTr("Playing, finished, and so on")) },
        install_size: { icon: Icons.hardDrive2Line,
                        hint: hint("install_size", qsTr("The space it takes on disk")) },
        rating: { icon: Icons.starLine, hint: hint("rating", qsTr("Out of 5")) },
        date_added: { icon: Icons.calendarCheckLine,
                      hint: hint("date_added", qsTr("The day it joined the library")) }
    })

    readonly property var chipDefs: Object.assign({
        steam_waiting: { icon: Icons.info,
                         hint: qsTr("Only while the game's edits wait for Steam to close, with Close Steam and apply, and Undo. Hidden, they still show on Properties and the Launch button.") },
        steam_update: { icon: Icons.download2Line,
                        hint: qsTr("Only while Steam has an update for the game. Hidden, it still shows on the Launch button and in the list.") }
    }, fieldDefs)

    readonly property var buttonDefs: ({
        favorite: { icon: Icons.star, hint: qsTr("Add to or remove from favorites") },
        open_platform: { icon: Icons.openExternal,
                         hint: qsTr("Hidden for Linux games") },
        folder: { icon: Icons.folder, hint: qsTr("The game, ROM or program folder") },
        save_folder: { icon: Icons.save2Fill, hint: qsTr("Hidden when the game has none") },
        settings: { icon: Icons.gear, hint: qsTr("Edit the game") },
        menu: { icon: Icons.more, hint: qsTr("The game's menu, as on right-click") }
    })

    GamePageNames { id: names }

    function named(defs, labels) {
        const out = {}
        for (const key of Object.keys(defs))
            out[key] = Object.assign({ name: labels[key] }, defs[key])
        return out
    }
    readonly property var fieldEntries: named(fieldDefs, names.fields)
    readonly property var chipEntries: named(chipDefs, names.chips)
    readonly property var buttonEntries: named(buttonDefs, names.buttons)

    readonly property string query: search.text.trim().toLowerCase()
    function matches(...texts) {
        return query === "" || texts.some(t => t.toLowerCase().includes(query))
    }

    function report(err) {
        if (err !== "")
            error.text = err
    }

    // `spec` is <place>.<name>, as chips.last_played.
    function dragForScreenshot(spec, offset) {
        const [place, name] = spec.split(".")
        const list = { chips: chips, buttons: buttons, info_rows: infoRows }[place]
        list?.dragForScreenshot(name, offset)
    }

    component SectionHeader: Label {
        Layout.topMargin: Theme.settingsWindow.sectionSpacing / 2
        Layout.bottomMargin: Theme.layout.spacing / 2
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.weight: Theme.font.strongWeight
    }

    component SectionNote: SettingsNote {
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.layout.spacing
    }

    component LabeledRow: RowLayout {
        property alias label: name.text
        property alias note: noteLabel.text

        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                id: name
                Layout.fillWidth: true
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                wrapMode: Text.WordWrap
            }
            SettingsNote {
                id: noteLabel
                Layout.fillWidth: true
                visible: text !== ""
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: Theme.layout.spacing

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
            spacing: 10

            SettingsSearchField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search the game page settings")
            }

            ProblemsButton {
                problems: General.gamePageProblems
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
                width: flick.width - scrollBar.width - Theme.layout.spacing
                spacing: 0

                SectionHeader { visible: chips.visible; text: qsTr("Chips") }
                SectionNote {
                    visible: chips.visible
                    text: qsTr("Beside the Launch button. A chip with nothing to show for the game is left out. The eye hides or shows a chip's icon.")
                }
                GamePageList {
                    id: chips
                    place: "chips"
                    entries: root.chipEntries
                    query: root.query
                    flickable: flick
                    onFailed: text => root.report(text)
                }

                SectionHeader { visible: buttons.visible; text: qsTr("Buttons") }
                SectionNote { visible: buttons.visible; text: qsTr("At the right of the action row.") }
                GamePageList {
                    id: buttons
                    place: "buttons"
                    entries: root.buttonEntries
                    query: root.query
                    flickable: flick
                    onFailed: text => root.report(text)
                }

                SectionHeader { visible: infoRows.visible; text: qsTr("Info panel") }
                SectionNote {
                    visible: infoRows.visible
                    text: qsTr("Beside the description, or below it in a narrow window. With no rows, the panel is hidden.")
                }
                GamePageList {
                    id: infoRows
                    place: "info_rows"
                    entries: root.fieldEntries
                    query: root.query
                    flickable: flick
                    onFailed: text => root.report(text)
                }

                readonly property bool formatShown: root.matches(qsTr("Play time"), qsTr("Format"))
                readonly property bool periodShown: root.matches(qsTr("Play time"), qsTr("Recent play time period"))
                SectionHeader { visible: content.formatShown || content.periodShown; text: qsTr("Play time") }
                SettingsCard {
                    visible: content.formatShown || content.periodShown
                    spacing: Theme.layout.spacing * 2
                    LabeledRow {
                        visible: content.formatShown
                        label: qsTr("Format")
                        note: qsTr("How the Play time and Recent play time chips and rows write it.")
                        SettingsComboBox {
                            model: root.playTimeFormats.map(f => Library.playTime(root.formatExample, f))
                            currentIndex: root.playTimeFormats.indexOf(General.playTimeFormat)
                            onActivated: root.report(General.setPlayTimeFormat(root.playTimeFormats[currentIndex]))
                        }
                    }
                    LabeledRow {
                        visible: content.periodShown
                        label: qsTr("Recent play time period")
                        note: qsTr("The time the Recent play time chip and row count. Weeks start on your locale's first day.")
                        SettingsComboBox {
                            model: root.periods.map(p => fields.periodLabels[p])
                            currentIndex: root.periods.indexOf(General.recentPeriod)
                            onActivated: root.report(General.setRecentPeriod(root.periods[currentIndex]))
                        }
                    }
                }

                readonly property bool descriptionShown: root.matches(qsTr("Description and tags"), qsTr("Show the description"))
                readonly property bool tagsShown: root.matches(qsTr("Description and tags"), qsTr("Show tags"))
                SectionHeader {
                    visible: content.descriptionShown || content.tagsShown
                    text: qsTr("Description and tags")
                }
                SettingsCard {
                    visible: content.descriptionShown || content.tagsShown
                    spacing: Theme.layout.spacing * 2
                    LabeledRow {
                        visible: content.descriptionShown
                        label: qsTr("Show the description")
                        SettingsSwitch {
                            checked: General.showDescription
                            onToggled: root.report(General.setShowDescription(checked))
                        }
                    }
                    LabeledRow {
                        visible: content.tagsShown
                        label: qsTr("Show tags")
                        SettingsSwitch {
                            checked: General.showTags
                            onToggled: root.report(General.setShowTags(checked))
                        }
                    }
                }

                Label {
                    visible: !chips.visible && !buttons.visible && !infoRows.visible && !content.formatShown
                             && !content.periodShown
                             && !content.descriptionShown && !content.tagsShown
                    Layout.fillWidth: true
                    text: qsTr("No game page settings match \"%1\".").arg(search.text.trim())
                    color: Theme.controls.descriptionColor
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: Theme.settingsWindow.sectionSpacing / 2 }
            }
        }
    }
}
