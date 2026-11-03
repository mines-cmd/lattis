import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property var game: ({})
    readonly property var saves: GameSaves.state.requested === game.id ? GameSaves.state : ({})
    readonly property var backups: saves.backups ?? []
    readonly property var paths: saves.paths ?? []
    readonly property bool matched: (saves.matched ?? "") !== ""
    readonly property bool fallback: saves.fallback ?? false
    readonly property bool restoring: Backups.importing

    signal message(string text)
    signal chooseRequested()

    function matchText() {
        switch (saves.match) {
        case "steam": return qsTr("Found by its Steam ID")
        case "title": return qsTr("Found by its title")
        case "chosen": return qsTr("The game you chose")
        }
        return ""
    }

    function restore(archive) {
        const error = Backups.restoreGameSaves(archive)
        if (error !== "")
            message(error)
    }

    function rename(archive, name) {
        const error = GameSaves.rename(game.id, archive, name)
        if (error !== "")
            message(error)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.backupWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    onAboutToShow: GameSaves.load(game.id)

    Connections {
        target: Backups
        function onImportFinished(kind, error) {
            if (kind === "game-saves" && error === "")
                GameSaves.load(root.game.id)
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Save backups of %1").arg(root.game.title ?? "")
                color: Theme.gameEditor.headingColor
                font.weight: Theme.font.strongWeight
                wrapMode: Text.WordWrap
            }
            WindowControls {
                Layout.alignment: Qt.AlignTop
                dialog: root
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: -Theme.gameEditor.rowSpacing / 2
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: root.fallback ? qsTr("No save folders are known for this game, so its whole Proton prefix is backed up, without Proton's own files.")
                : root.matched ? qsTr("Saves of %1 (%2)").arg(root.saves.matched).arg(root.matchText())
                : root.paths.length > 0 ? qsTr("Your own save folders")
                : qsTr("No saves found for this game. Choose the game it is in the Ludusavi list, or set its save folder in Properties.")
        }
        Label {
            visible: root.matched && !root.fallback && root.paths.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("None of its save files exist on this computer yet, or the list has none for it. Set a save folder in Properties to back up your own.")
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
        }

        Label {
            visible: GameSaves.message !== ""
            Layout.fillWidth: true
            text: GameSaves.message
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
            elide: Text.ElideRight
        }

        Label {
            visible: root.backups.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("No backups yet. Back up now, or one is made when the game closes.")
            color: Theme.gameEditor.labelColor
        }

        Rectangle {
            visible: root.backups.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(list.contentHeight, 4 * (Theme.controls.buttonHeight + 2 * Theme.gameEditor.rowSpacing))
            radius: Theme.controls.inputRadius
            color: Theme.gameEditor.cardBackground
            border.color: Theme.gameEditor.cardBorder
            border.width: Theme.gameEditor.cardBorderWidth
            clip: true

        ListView {
            id: list
            anchors.fill: parent
            spacing: 0
            clip: true
            model: root.backups
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {}

            delegate: Item {
              id: backupRow
              required property var modelData
              required property int index
              width: ListView.view.width
              implicitHeight: rowContent.implicitHeight + 2 * Theme.gameEditor.rowSpacing
              height: implicitHeight

              property bool renaming: false
              property bool confirming: false
              readonly property bool named: modelData.name !== ""

              Rectangle {
                  visible: backupRow.index > 0
                  width: parent.width
                  height: 1
                  color: Theme.settingsWindow.rowDivider
              }
              RowLayout {
                id: rowContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.gameEditor.cardPadding
                anchors.rightMargin: Theme.gameEditor.cardPadding
                spacing: Theme.layout.spacing

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !backupRow.renaming && !backupRow.confirming
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing

                        Label {
                            Layout.maximumWidth: parent.width - 2 * Theme.controls.iconSize
                            text: backupRow.named ? backupRow.modelData.name : qsTr("Unnamed")
                            color: Theme.palette.text
                            elide: Text.ElideRight
                        }
                        Icon {
                            visible: backupRow.named
                            path: Icons.save2Line
                            size: Theme.controls.iconSize
                            color: Theme.gameEditor.hintColor
                            ToolTip.visible: pinHover.hovered
                            ToolTip.delay: Theme.tooltip.delay
                            ToolTip.text: qsTr("A backup with a name is always kept")
                            HoverHandler { id: pinHover }
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 · %2").arg(backupRow.modelData.when).arg(backupRow.modelData.size)
                        color: Theme.gameEditor.hintColor
                        font.pixelSize: Theme.gameEditor.hintFontSize
                        elide: Text.ElideRight
                    }
                }
                SettingsTextField {
                    id: nameField
                    visible: backupRow.renaming
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name")
                    text: backupRow.modelData.name
                    onVisibleChanged: if (visible) forceActiveFocus()
                    onAccepted: backupRow.renaming = false
                    Keys.onEscapePressed: {
                        text = backupRow.modelData.name
                        backupRow.renaming = false
                    }
                    onActiveFocusChanged: if (!activeFocus && backupRow.renaming) backupRow.renaming = false
                    onEditingFinished: {
                        if (text.trim() !== backupRow.modelData.name)
                            root.rename(backupRow.modelData.path, text)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: backupRow.confirming
                    spacing: 0

                    Label {
                        text: qsTr("Replace your current saves?")
                        color: Theme.gameEditor.labelColor
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("They're moved aside next to where they are first.")
                        color: Theme.gameEditor.hintColor
                        font.pixelSize: Theme.gameEditor.hintFontSize
                    }
                }
                SettingsButton {
                    visible: !backupRow.confirming && !backupRow.renaming
                    iconPath: Icons.editLine
                    flat: true
                    tip: qsTr("Rename")
                    onClicked: backupRow.renaming = true
                }
                SettingsButton {
                    visible: !backupRow.confirming && !backupRow.renaming
                    iconPath: Icons.deleteBin6Line
                    flat: true
                    tip: qsTr("Delete")
                    onClicked: {
                        const error = GameSaves.remove(root.game.id, backupRow.modelData.path)
                        if (error !== "")
                            root.message(error)
                    }
                }
                SettingsButton {
                    visible: !backupRow.confirming && !backupRow.renaming
                    text: qsTr("Restore")
                    enabled: !root.restoring
                    onClicked: backupRow.confirming = true
                }
                SettingsButton {
                    visible: backupRow.confirming
                    text: qsTr("Cancel")
                    flat: true
                    onClicked: backupRow.confirming = false
                }
                SettingsButton {
                    visible: backupRow.confirming
                    text: qsTr("Restore")
                    primary: true
                    enabled: !root.restoring
                    onClicked: {
                        backupRow.confirming = false
                        root.restore(backupRow.modelData.path)
                    }
                }
              }
            }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Keeps the last %1 backups in %2. A backup with a name is always kept.")
                          .arg(root.saves.keep ?? 0).arg(root.saves.folder ?? "")
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
            }
            SettingsButton {
                text: qsTr("Change match")
                visible: root.saves.hasManifest ?? false
                tip: qsTr("Choose which game in the Ludusavi list this one's saves are taken from")
                onClicked: root.chooseRequested()
            }
            SettingsButton {
                text: GameSaves.working ? qsTr("Backing up") : qsTr("Back up now")
                primary: true
                enabled: root.paths.length > 0 && !GameSaves.working
                onClicked: GameSaves.backUpNow(root.game.id)
            }
        }
    }
}
