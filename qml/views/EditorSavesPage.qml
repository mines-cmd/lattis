import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A tab of GameEditor; the editor keeps the state and reads these controls.
ColumnLayout {
    id: page

    required property var editor
    property alias autoBackupBox: autoBackupBox
    property alias noSavesSwitch: noSavesSwitch
    // The found ones; the game's own are listed above them.
    readonly property var savePaths: (editor.saves.paths ?? []).filter(
        p => !editor.savePathsShown.some(own => p === own || p.startsWith(own + "/")))
    readonly property bool epicFolder: editor.saves.match === "epic"
    readonly property bool matchShown: ((editor.saves.matched ?? "") !== "" || epicFolder)
                                       && !(editor.saves.fallback ?? false)
    // Epic's own folder is used instead of the manifest, so choosing a manifest game would change nothing.
    readonly property bool canChangeMatch: (editor.saves.hasManifest ?? false) && !epicFolder
    readonly property bool foundShown: editor.backupGame && !savesPanel.empty

    function matchText() {
        switch (editor.saves.match) {
        case "steam": return qsTr("Found by its Steam ID")
        case "title": return qsTr("Found by its title")
        case "chosen": return qsTr("The game you chose")
        case "epic": return qsTr("Epic's save folder")
        }
        return ""
    }

    // A setting with its control at the right edge, as in the Settings window, so the controls of
    // a card line up whatever their labels' length.
    component OptionRow: Item {
        id: optionRow

        property alias text: optionLabel.text
        property string tip: ""
        property bool clickable: false
        default property alias control: controlSlot.data
        signal labelClicked()

        Layout.fillWidth: true
        implicitHeight: Math.max(Theme.controls.inputHeight, controlSlot.implicitHeight)

        HoverHandler { id: rowHover }
        // Past the row, into the card's padding, so the label keeps the card's left edge.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.gameEditor.optionRowBleed
            radius: Theme.settingsWindow.rowRadius
            color: rowHover.hovered ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)

            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }
        RowLayout {
            anchors.fill: parent
            spacing: Theme.settingsWindow.rowLabelSpacing

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Label {
                    id: optionLabel
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, parent.width)
                    color: Theme.gameEditor.labelColor
                    elide: Text.ElideRight

                    HoverHandler { id: labelHover }
                    ToolTip.visible: optionRow.tip !== "" && labelHover.hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: optionRow.tip
                }
                HoverHandler { cursorShape: optionRow.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor }
                TapHandler {
                    enabled: optionRow.clickable
                    onTapped: optionRow.labelClicked()
                }
            }
            Item {
                id: controlSlot
                implicitWidth: childrenRect.width
                implicitHeight: childrenRect.height
            }
        }
    }

    Layout.fillWidth: true
    Layout.fillHeight: false // as tall as its cards; layouts fill by default
    Layout.alignment: Qt.AlignTop
    spacing: Theme.gameEditor.cardSpacing

    EditorCard {
        title: qsTr("Saves")
        visible: !editor.adding

        SavesPanel {
            id: savesPanel
            visible: editor.backupGame
            game: editor.game
            noSaves: noSavesSwitch.checked
            onMessage: text => editor.message(text)
            onChooseRequested: editor.chooseSavesGame()
            onChooseFolderRequested: editor.folderPickerLoader.pickFor("saves")
        }
        Rectangle {
            visible: editor.backupGame
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.settingsWindow.rowDivider
        }

        EditorField {
            heading: editor.backupGame
            label: qsTr("Save locations")
            help: qsTr("Where the game's saves are: backups take every one, and Open save folder opens "
                       + "the first. They're found from the Ludusavi manifest, Epic, or the emulator; add "
                       + "a folder they miss.")
            actions: EditorTextAction {
                text: qsTr("Add")
                iconPath: Icons.add
                tip: qsTr("Add a folder the game saves in")
                onClicked: editor.folderPickerLoader.pickFor("saves")
            }
        }
        Repeater {
            model: editor.savePathsShown

            PathField {
                required property string modelData
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.controls.inputHeight
                path: editor.homeAbsolute(modelData)
                fontSize: Theme.gameEditor.pathFontSize
                onCopied: editor.message(qsTr("Copied"))

                SettingsButton {
                    iconPath: Icons.close
                    tip: qsTr("Remove this location")
                    onClicked: editor.newSavePaths = editor.savePathsShown.filter(p => p !== modelData)
                }
            }
        }
        EditorHint {
            open: !editor.backupGame && editor.savePathsShown.length === 0
            text: qsTr("None yet. Open save folder opens the emulator's or the Proton prefix's folder.")
        }

        RowLayout {
            id: matchRow
            visible: page.foundShown && (page.matchShown || page.canChangeMatch)
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                visible: page.matchShown && !page.epicFolder
                Layout.maximumWidth: matchRow.width / 2
                text: editor.saves.matched ?? ""
                color: Theme.palette.text
                font.pixelSize: Theme.gameEditor.hintFontSize
                elide: Text.ElideRight
            }
            Label {
                visible: page.matchShown
                Layout.fillWidth: true
                text: page.matchText()
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
                elide: Text.ElideRight
            }
            Item {
                visible: !page.matchShown
                Layout.fillWidth: true
            }
            SettingsButton {
                text: qsTr("Change match")
                flat: true
                visible: page.canChangeMatch
                tip: qsTr("Choose which game in the Ludusavi list this one's saves are taken from")
                onClicked: editor.chooseSavesGame()
            }
        }
        Repeater {
            model: page.savePaths.slice(0, 3)

            PathField {
                required property string modelData
                visible: page.foundShown
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.controls.inputHeight
                path: editor.homeAbsolute(modelData)
                fontSize: Theme.gameEditor.pathFontSize
                onCopied: editor.message(qsTr("Copied"))
            }
        }
        Unfold {
            id: moreFold
            visible: page.foundShown && (open || progress > 0)
            Layout.fillWidth: true

            Repeater {
                model: page.savePaths.slice(3)

                PathField {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.minimumHeight: Theme.controls.inputHeight
                    path: editor.homeAbsolute(modelData)
                    fontSize: Theme.gameEditor.pathFontSize
                    onCopied: editor.message(qsTr("Copied"))
                }
            }
        }
        SettingsButton {
            visible: page.foundShown && page.savePaths.length > 3
            Layout.alignment: Qt.AlignLeft
            flat: true
            text: moreFold.open ? qsTr("Show fewer")
                : page.savePaths.length === 4 ? qsTr("1 more") : qsTr("%1 more").arg(page.savePaths.length - 3)
            iconPath: moreFold.open ? Icons.arrowUp : Icons.arrowDown
            onClicked: moreFold.open = !moreFold.open
        }
    }

    EditorCard {
        title: qsTr("Backup options")
        visible: !editor.adding && editor.backupGame

        // First, since it decides whether the menu below applies: what replaces the menu changes
        // under it rather than moving the switch out from under the pointer.
        OptionRow {
            text: qsTr("This game has no saves")
            clickable: true
            onLabelClicked: noSavesSwitch.toggle()

            SettingsSwitch { id: noSavesSwitch }
        }
        // The menu and the note crossfade in one place and only the card's bottom edge moves;
        // folding one while unfolding the other drew both half-clipped over each other.
        Item {
            id: autoSlot

            readonly property bool skipped: noSavesSwitch.checked
            property real slotHeight: skipped ? skipNote.implicitHeight : autoRow.implicitHeight
            Behavior on slotHeight { enabled: Motion.on; MotionNumber { speed: "normal" } }

            Layout.fillWidth: true
            implicitHeight: slotHeight
            // Only while it changes height: at rest, the rows' hover highlight reaches past it.
            clip: slotHeight !== (skipped ? skipNote.implicitHeight : autoRow.implicitHeight)

            OptionRow {
                id: autoRow
                width: parent.width
                enabled: !autoSlot.skipped
                opacity: autoSlot.skipped ? 0 : 1
                visible: opacity > 0
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                text: qsTr("Backup when the game closes")
                tip: editor.romGame
                    ? qsTr("The emulator's save folders are backed up to the folder in Settings, Library, Game saves.")
                    : qsTr("Saves are found from the Ludusavi manifest, or from the save locations you add in Saves, and "
                           + "backed up to the folder in Settings, Library, Game saves.")

                Label {
                    id: widest
                    visible: false
                    text: qsTr("Platform default (%1)").arg(qsTr("off"))
                }
                SettingsComboBox {
                    id: autoBackupBox
                    width: autoBackupBox.leftPadding + autoBackupBox.rightPadding + Math.ceil(widest.implicitWidth)
                    model: [qsTr("Platform default (%1)").arg(editor.game.platformAutoBackup ? qsTr("on") : qsTr("off")),
                            qsTr("On"), qsTr("Off")]
                }
            }
            Label {
                id: skipNote
                width: parent.width
                opacity: autoSlot.skipped ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
                text: qsTr("Backups skip it, and the Library overview doesn't count it as never backed up.")
            }
        }
    }

    EditorCard {
        title: qsTr("Mod managers")
        visible: editor.managerGame
        actions: Row {
            spacing: Theme.layout.spacing

            EditorTextAction {
                id: linkAction
                visible: editor.unlinkedManagers.length > 0
                text: qsTr("Link")
                iconPath: Icons.link
                tip: qsTr("Link a mod manager you already added")
                onClicked: editor.managerMenuItem.popup(linkAction, 0, linkAction.height)
            }
            EditorTextAction {
                text: qsTr("Add")
                iconPath: Icons.add
                tip: qsTr("Add a mod manager for this game")
                onClicked: editor.addModManagerRequested(editor.game.id)
            }
        }

        Label {
            visible: editor.linkedManagers.length === 0
            Layout.fillWidth: true
            text: qsTr("No mod manager is linked to this game.")
            color: Theme.gameEditor.hintColor
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: editor.linkedManagers

            RowLayout {
                id: managerRow
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing

                Item {
                    Layout.preferredWidth: Theme.controls.iconSize
                    Layout.preferredHeight: Theme.controls.iconSize

                    Image {
                        anchors.fill: parent
                        visible: status === Image.Ready
                        source: managerRow.modelData.icon
                        sourceSize.width: parent.width * 2
                        sourceSize.height: parent.height * 2
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        mipmap: true
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: managerRow.modelData.icon === ""
                        path: Icons.puzzle
                        size: parent.width
                        color: Theme.controls.inputText
                    }
                }
                PlainLabel {
                    Layout.fillWidth: true
                    text: managerRow.modelData.name
                    color: Theme.controls.inputText
                    elide: Text.ElideRight
                }
                Label {
                    text: managerRow.modelData.proton ? qsTr("Proton") : qsTr("Native")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                }
                ListButton {
                    flat: true
                    iconPath: Icons.close
                    textColor: hovered ? foreground : Theme.controls.descriptionColor
                    tip: qsTr("Unlink from this game")
                    onClicked: editor.setManagerLinked(managerRow.modelData.id, false)
                }
            }
        }
    }
}
