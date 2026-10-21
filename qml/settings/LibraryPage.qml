import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Backups, hidden games, tags, and anything wrong in the library's files.")

    readonly property string currentSection: ""

    readonly property string query: search.text.trim().toLowerCase()
    readonly property bool searching: query !== ""
    function matches(...texts) {
        return !searching || texts.some(t => t.toLowerCase().includes(query))
    }
    readonly property var tags: Library.tags.filter(t => matches(t.name))
    readonly property var hiddenGames: Library.hiddenGames.filter(g => matches(g.title, g.platformName))
    readonly property var problems: Library.problems.filter(p => matches(p))

    signal showColors(string section)
    signal suggestTagsFromIgdb()

    property bool paletteCustomized: Theme.isCustomized("tags", "palette")

    Connections {
        target: Theme
        function onValueChanged() { root.paletteCustomized = Theme.isCustomized("tags", "palette") }
        function onActiveThemeChanged() { root.paletteCustomized = Theme.isCustomized("tags", "palette") }
    }
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
    readonly property bool backupShown: matches(qsTr("Backup"), qsTr("Back up library"), qsTr("Back up saves"),
                                                qsTr("Import"), qsTr("Moved aside"))
    // Backups is made on first use: not before this page is shown.
    readonly property var lastBackups: visible ? Backups.last : ({})
    readonly property var movedAside: visible ? Backups.movedAside : []

    function backUp(kind) {
        backupDialog.active = true
        backupDialog.item.ask(kind)
    }

    Loader {
        id: backupDialog
        active: false
        sourceComponent: BackupDialog {}
    }

    function importBackup() {
        importDialog.active = true
        importDialog.item.pick()
    }

    Loader {
        id: importDialog
        active: false
        sourceComponent: ImportDialog {}
    }

    function reveal(item) {
        if (item === "backup-library" || item === "backup-saves") {
            backUp(item.slice("backup-".length))
            item = "backup"
        } else if (item === "import") {
            importBackup()
            item = "backup"
        }
        const header = { tags: tagsHeader, hidden: hiddenHeader, backup: backupHeader, problems: problemsHeader }[item]
        if (header !== undefined)
            Qt.callLater(() => flick.contentY = Math.max(0, Math.min(header.y, flick.contentHeight - flick.height)))
    }

    property int pickedColor: -1
    readonly property int newTagColor: pickedColor >= 0 ? pickedColor
        : (Library.tags, Library.nextTagColor(Theme.tags.palette.length))

    function addTag() {
        const name = newTagField.text.trim()
        if (name === "")
            return
        error.text = Library.addTag(name, newTagColor)
        if (error.text === "") {
            newTagField.text = ""
            pickedColor = -1
        }
    }

    component Swatch: Rectangle {
        id: swatchItem
        property int colorIndex: 0
        property string tip
        signal clicked()

        implicitWidth: Theme.colorPicker.swatchSize
        implicitHeight: Theme.colorPicker.swatchSize
        radius: Theme.controls.inputRadius
        color: Theme.tags.palette.length > 0 ? Theme.tags.palette[colorIndex % Theme.tags.palette.length]
                                              : Theme.palette.surfaceRaised
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth

        MouseArea {
            id: swatchMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: swatchItem.clicked()
        }
        ToolTip.visible: swatchMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: tip
    }

    property string renaming: ""

    function finishRename(from, to) {
        renaming = ""
        if (to.trim() !== "" && to.trim() !== from)
            error.text = Library.renameTag(from, to.trim())
    }

    component DialogBackground: Rectangle {
        color: Theme.menu.popupBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth
        radius: Theme.controls.inputRadius
    }

    component SectionHeader: Label {
        Layout.topMargin: Theme.settingsWindow.sectionSpacing / 2
        Layout.bottomMargin: Theme.layout.spacing
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.weight: Theme.font.strongWeight
    }

    component Card: Rectangle {
        default property alias rows: column.data
        Layout.fillWidth: true
        implicitHeight: column.implicitHeight + Theme.settingsWindow.cardPadding * 2
        radius: Theme.settingsWindow.cardRadius
        color: Theme.settingsWindow.cardBackground
        border.color: Theme.settingsWindow.cardBorder
        border.width: Theme.settingsWindow.cardBorderWidth

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: Theme.settingsWindow.cardPadding
            spacing: 0
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.settingsWindow.rowDivider
        visible: Theme.settingsWindow.rowDividers
    }

    component Muted: Label {
        color: Theme.controls.descriptionColor
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }

    MotionPopup {
        id: deleteDialog

        property var tag: ({ name: "", games: 0 })

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.settingsWindow.contentPadding / 2
        width: Theme.gameEditor.confirmWidth
        background: DialogBackground {}

        ColumnLayout {
            width: parent.width
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Delete the tag %1?").arg(deleteDialog.tag.name)
                color: Theme.controls.labelColor
                font.weight: Theme.font.strongWeight
                wrapMode: Text.WordWrap
            }
            Muted {
                Layout.fillWidth: true
                text: deleteDialog.tag.games > 0
                      ? (deleteDialog.tag.games === 1 ? qsTr("1 game loses it. The game itself stays.")
                         : qsTr("%1 games lose it. The games themselves stay.").arg(deleteDialog.tag.games))
                      : qsTr("No game has it.")
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.layout.spacing
                SettingsButton { text: qsTr("Cancel"); onClicked: deleteDialog.close() }
                SettingsButton {
                    danger: true
                    text: qsTr("Delete")
                    onClicked: {
                        error.text = Window.window.change(() => Library.removeTag(deleteDialog.tag.name))
                        deleteDialog.close()
                    }
                }
            }
        }
    }

    MotionPopup {
        id: trashDialog

        property var entry: ({ index: -1, places: "" })

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.settingsWindow.contentPadding / 2
        width: Theme.gameEditor.confirmWidth
        background: DialogBackground {}

        ColumnLayout {
            width: parent.width
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Move %1 to the trash?").arg(trashDialog.entry.places)
                color: Theme.controls.labelColor
                font.weight: Theme.font.strongWeight
                wrapMode: Text.WordWrap
            }
            Muted {
                Layout.fillWidth: true
                text: qsTr("What you had before the import. What was restored stays.")
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.layout.spacing
                SettingsButton { text: qsTr("Cancel"); onClicked: trashDialog.close() }
                SettingsButton {
                    danger: true
                    text: qsTr("Move to trash")
                    onClicked: {
                        error.text = Backups.deleteMovedAside(trashDialog.entry.index)
                        trashDialog.close()
                    }
                }
            }
        }
    }

    TagPalettePopup {
        id: paletteMenu
        property string tag
        onPicked: index => {
            if (tag === "")
                root.pickedColor = index
            else
                error.text = Library.setTagColor(tag, index)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: Theme.layout.spacing

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
            spacing: Theme.layout.spacing

            SettingsSearchField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search tags, hidden games and problems")
            }
            ProblemsButton {
                problems: Library.problems
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

                Muted {
                    visible: root.searching && root.tags.length + root.hiddenGames.length + root.problems.length === 0 && !root.backupShown
                    Layout.fillWidth: true
                    text: qsTr("Nothing matches \"%1\".").arg(search.text.trim())
                }

                SectionHeader { id: backupHeader; visible: root.backupShown; text: qsTr("Backup") }
                Card {
                    visible: root.backupShown

                    Repeater {
                        model: [
                            { kind: "library", name: qsTr("Library"), button: qsTr("Back up library"),
                              what: qsTr("Games, art, themes, settings and the API keys. Never ROMs.") },
                            { kind: "saves", name: qsTr("Saves"), button: qsTr("Back up saves"),
                              what: qsTr("Emulator saves, keys and firmware, saves next to ROMs, and Proton games' saves.") },
                        ]

                        ColumnLayout {
                            id: backupRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            Divider { visible: backupRow.index > 0 && Theme.settingsWindow.rowDividers }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.margins: Theme.settingsWindow.rowPadding
                                spacing: Theme.layout.spacing

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        text: backupRow.modelData.name
                                        color: Theme.controls.labelColor
                                        font.pixelSize: Theme.controls.labelFontSize
                                    }
                                    Muted {
                                        Layout.fillWidth: true
                                        text: backupRow.modelData.what
                                    }
                                    Muted {
                                        Layout.fillWidth: true
                                        text: root.lastBackups[backupRow.modelData.kind]?.text ?? qsTr("Not backed up yet.")
                                    }
                                }
                                SettingsButton {
                                    text: backupRow.modelData.button
                                    iconPath: Icons.download
                                    onClicked: root.backUp(backupRow.modelData.kind)
                                }
                            }
                        }
                    }

                    Divider {}
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Theme.settingsWindow.rowPadding
                        spacing: Theme.layout.spacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                text: qsTr("Import")
                                color: Theme.controls.labelColor
                                font.pixelSize: Theme.controls.labelFontSize
                            }
                            Muted {
                                Layout.fillWidth: true
                                text: qsTr("Restores either backup. What it replaces is moved aside, not deleted.")
                            }
                        }
                        SettingsButton {
                            text: qsTr("Import")
                            iconPath: Icons.upload
                            onClicked: root.importBackup()
                        }
                    }

                    Repeater {
                        model: root.movedAside

                        ColumnLayout {
                            id: asideRow
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 0

                            Divider {}

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.margins: Theme.settingsWindow.rowPadding
                                spacing: Theme.layout.spacing

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        Layout.fillWidth: true
                                        text: asideRow.modelData.title
                                        color: Theme.controls.labelColor
                                        font.pixelSize: Theme.controls.labelFontSize
                                        elide: Text.ElideMiddle
                                    }
                                    Muted {
                                        Layout.fillWidth: true
                                        text: asideRow.modelData.text
                                    }
                                    Repeater {
                                        model: asideRow.modelData.paths
                                        Muted {
                                            required property string modelData
                                            Layout.fillWidth: true
                                            text: modelData
                                            wrapMode: Text.NoWrap
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                    Muted {
                                        visible: asideRow.modelData.more > 0
                                        text: qsTr("and %1 more").arg(asideRow.modelData.more)
                                    }
                                }
                                SettingsButton {
                                    text: qsTr("Show")
                                    iconPath: Icons.folder
                                    onClicked: Qt.openUrlExternally(asideRow.modelData.folder)
                                }
                                SettingsButton {
                                    text: qsTr("Delete")
                                    danger: true
                                    onClicked: {
                                        trashDialog.entry = asideRow.modelData
                                        trashDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }

                SectionHeader { id: hiddenHeader; visible: !root.searching || root.hiddenGames.length > 0; text: qsTr("Hidden games") }
                Muted {
                    visible: !root.searching && Library.hiddenGames.length === 0
                    Layout.fillWidth: true
                    text: qsTr("No hidden games. \"Remove from library\" in a game's settings hides it.")
                }
                Card {
                    visible: root.hiddenGames.length > 0

                    Repeater {
                        model: root.hiddenGames

                        ColumnLayout {
                            id: hiddenRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            Divider { visible: hiddenRow.index > 0 && Theme.settingsWindow.rowDividers }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.margins: Theme.settingsWindow.rowPadding
                                spacing: Theme.layout.spacing

                                Label {
                                    Layout.fillWidth: true
                                    text: hiddenRow.modelData.title
                                    color: Theme.controls.labelColor
                                    elide: Text.ElideRight
                                }
                                Muted { text: hiddenRow.modelData.platformName }
                                SettingsButton {
                                    text: qsTr("Unhide")
                                    onClicked: error.text = Library.unhideGame(hiddenRow.modelData.id)
                                }
                            }
                        }
                    }
                }

                SectionHeader { id: tagsHeader; visible: !root.searching || root.tags.length > 0; text: qsTr("Tags") }

                RowLayout {
                    visible: !root.searching
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    spacing: Theme.layout.spacing

                    Swatch {
                        id: newTagSwatch
                        colorIndex: root.newTagColor
                        tip: qsTr("The new tag's color")
                        onClicked: {
                            paletteMenu.tag = ""
                            paletteMenu.openFor(newTagSwatch, root.newTagColor)
                        }
                    }
                    SettingsTextField {
                        id: newTagField
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.controls.inputHeight
                        placeholderText: qsTr("New tag")
                        onAccepted: root.addTag()
                    }
                    SettingsButton {
                        text: qsTr("Add tag")
                        iconPath: Icons.add
                        enabled: newTagField.text.trim() !== ""
                        onClicked: root.addTag()
                    }
                }
                Label {
                    // Tag names are case-sensitive: say so before making a near-twin.
                    readonly property string typed: newTagField.text.trim()
                    readonly property string twin: typed === "" || Library.tags.some(t => t.name === typed) ? ""
                        : Library.tags.find(t => t.name.toLowerCase() === typed.toLowerCase())?.name ?? ""
                    visible: !root.searching && twin !== ""
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("There's already a tag called %1; tags are case-sensitive, so this adds another one.")
                              .arg(twin)
                    color: Theme.controls.problemsText
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }

                Muted {
                    visible: !root.searching && Library.tags.length === 0
                    Layout.fillWidth: true
                    text: qsTr("No tags yet. Add one here, or to a game in its settings.")
                }
                Card {
                    visible: root.tags.length > 0

                    GridLayout {
                        id: tagGrid
                        Layout.fillWidth: true
                        flow: GridLayout.TopToBottom
                        rows: Math.ceil(root.tags.length / 2)
                        columnSpacing: Theme.settingsWindow.rowPadding
                        rowSpacing: 0

                        Repeater {
                            model: root.tags

                            ColumnLayout {
                                id: tagRow
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                Layout.alignment: Qt.AlignTop
                                spacing: 0

                                Divider { visible: tagRow.index % tagGrid.rows !== 0 && Theme.settingsWindow.rowDividers }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.margins: Theme.settingsWindow.rowPadding
                                    spacing: Theme.layout.spacing

                                    Swatch {
                                        id: swatch
                                        colorIndex: tagRow.modelData.color
                                        tip: qsTr("Change the color")
                                        onClicked: {
                                            paletteMenu.tag = tagRow.modelData.name
                                            paletteMenu.openFor(swatch, tagRow.modelData.color)
                                        }
                                    }

                                    Label {
                                        visible: root.renaming !== tagRow.modelData.name
                                        Layout.fillWidth: true
                                        text: tagRow.modelData.name
                                        color: Theme.controls.labelColor
                                        elide: Text.ElideRight
                                    }
                                    SettingsTextField {
                                        id: nameField
                                        visible: root.renaming === tagRow.modelData.name
                                        Layout.fillWidth: true
                                        onVisibleChanged: if (visible) {
                                            text = tagRow.modelData.name
                                            forceActiveFocus()
                                            selectAll()
                                        }
                                        onAccepted: root.finishRename(tagRow.modelData.name, text)
                                        onActiveFocusChanged: if (!activeFocus && visible) root.renaming = ""
                                    }

                                    Muted {
                                        visible: !tagRow.modelData.listed
                                        text: qsTr("not in tags.toml")
                                    }
                                    Muted {
                                        text: tagRow.modelData.games === 1 ? qsTr("1 game")
                                                                           : qsTr("%1 games").arg(tagRow.modelData.games)
                                    }
                                    SettingsButton {
                                        leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                                        rightPadding: leftPadding
                                        iconPath: Icons.edit
                                        tip: qsTr("Rename %1").arg(tagRow.modelData.name)
                                        onClicked: root.renaming = tagRow.modelData.name
                                    }
                                    SettingsButton {
                                        leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                                        rightPadding: leftPadding
                                        iconPath: Icons.close
                                        tip: qsTr("Delete %1").arg(tagRow.modelData.name)
                                        onClicked: {
                                            deleteDialog.tag = tagRow.modelData
                                            deleteDialog.open()
                                        }
                                    }
                                }
                            }
                        }

                        // An odd number: the right column's empty last place has the line
                        // above it too, level with the left column's.
                        Divider {
                            visible: root.tags.length % 2 === 1 && root.tags.length > 1 && Theme.settingsWindow.rowDividers
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                        }
                    }
                }

                RowLayout {
                    visible: !root.searching
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.layout.spacing
                    spacing: Theme.layout.spacing

                    Muted {
                        Layout.fillWidth: true
                        text: Library.missingDefaultTags.length > 0
                              ? qsTr("Not in your tags: %1.").arg(Library.missingDefaultTags.join(", "))
                              : Metadata.hasIgdbKeys ? "" : qsTr("Add your IGDB keys in Settings → Metadata to suggest tags from IGDB.")
                    }
                    SettingsButton {
                        text: qsTr("Suggest tags from IGDB")
                        iconPath: Icons.search
                        enabled: Metadata.hasIgdbKeys
                        tip: qsTr("Look every game up on IGDB and review the tags that fit it before adding them")
                        onClicked: root.suggestTagsFromIgdb()
                    }
                    SettingsButton {
                        visible: Library.missingDefaultTags.length > 0
                        text: qsTr("Add default tags")
                        iconPath: Icons.add
                        tip: Library.missingDefaultTags.length === 1 ? qsTr("Adds 1 tag")
                             : qsTr("Adds %1 tags").arg(Library.missingDefaultTags.length)
                        onClicked: error.text = Library.addDefaultTags()
                    }
                }

                Card {
                    visible: !root.searching
                    Layout.topMargin: Theme.layout.spacing * 2

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: Theme.settingsWindow.rowPadding
                        spacing: Theme.layout.spacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    text: qsTr("Colors")
                                    color: Theme.controls.labelColor
                                    font.pixelSize: Theme.controls.labelFontSize
                                }
                                Muted {
                                    Layout.fillWidth: true
                                    text: qsTr("Saved to your colors, %1, so each set of colors can have its own. A tag remembers its place in this list, so changing a color here recolors every tag that uses it. Click a color to change or remove it; right-click to move it.")
                                          .arg(Theme.activeFile("tags", "palette"))
                                }
                            }
                            SettingsButton {
                                visible: root.paletteCustomized
                                leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                                rightPadding: leftPadding
                                iconPath: Icons.close
                                tip: qsTr("Reset (remove from %1)").arg(Theme.activeFile("tags", "palette"))
                                onClicked: Theme.resetValue("tags", "palette")
                            }
                            SettingsButton {
                                text: qsTr("Edit in Colors")
                                tip: qsTr("The Tags section of Colors: text, outline and the circle behind the ×. Sizes and corners are in Style.")
                                onClicked: root.showColors("tags")
                            }
                        }

                        ColorListEditor {
                            id: paletteEditor
                            Layout.fillWidth: true
                            section: "tags"
                            key: "palette"
                            value: Theme.tags.palette
                        }
                    }
                }

                SectionHeader { id: problemsHeader; visible: !root.searching || root.problems.length > 0; text: qsTr("Problems") }
                Muted {
                    visible: !root.searching && Library.problems.length === 0
                    Layout.fillWidth: true
                    text: qsTr("Nothing wrong in the library's files.")
                }
                Card {
                    visible: root.problems.length > 0

                    Repeater {
                        model: root.problems

                        ColumnLayout {
                            id: problemRow
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            Divider { visible: problemRow.index > 0 && Theme.settingsWindow.rowDividers }

                            Label {
                                Layout.fillWidth: true
                                Layout.margins: Theme.settingsWindow.rowPadding
                                text: problemRow.modelData
                                color: Theme.controls.problemsText
                                wrapMode: Text.WordWrap
                                textFormat: Text.PlainText
                            }
                        }
                    }
                }
            }
        }
    }
}
