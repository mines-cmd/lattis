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
    readonly property int shownPaths: 3
    readonly property bool restoring: Backups.importing

    signal message(string text)

    function matchText() {
        switch (saves.match) {
        case "steam": return qsTr("Found by its Steam ID")
        case "title": return qsTr("Found by its title")
        case "chosen": return qsTr("The game you chose")
        }
        return ""
    }

    function chooseGame() {
        picker.active = true
        picker.item.gameId = game.id
        picker.item.gameTitle = game.title ?? ""
        picker.item.chosen = saves.match === "chosen" ? (saves.matched ?? "") : ""
        picker.item.open()
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

    Loader {
        id: picker
        active: false
        source: "SavesGamePicker.qml"
        onLoaded: item.message.connect(text => root.message(text))
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Save backups of %1").arg(root.game.title ?? "")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.labelColor
            text: root.fallback ? qsTr("No save folders are known for this game, so its whole Proton prefix is backed up, without Proton's own files.")
                : root.matched ? qsTr("Saves of %1 (%2)").arg(root.saves.matched).arg(root.matchText())
                : root.paths.length > 0 ? qsTr("Your own save folders")
                : qsTr("No saves found for this game. Choose the game it is in the Ludusavi list, or set its save folder in Properties.")
        }
        Repeater {
            model: root.paths.slice(0, root.shownPaths)

            Label {
                required property string modelData
                Layout.fillWidth: true
                text: modelData
                color: Theme.gameEditor.hintColor
                font.family: Theme.font.monoFamily
                font.pixelSize: Theme.gameEditor.hintFontSize
                elide: Text.ElideMiddle
            }
        }
        Label {
            visible: root.paths.length > root.shownPaths
            text: qsTr("and %n more", "", root.paths.length - root.shownPaths)
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
        }
        Label {
            visible: root.matched && !root.fallback && root.paths.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("None of its save files exist on this computer yet, or the list has none for it. Set a save folder in Properties to back up your own.")
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            SettingsButton {
                text: qsTr("Back up now")
                enabled: root.paths.length > 0 && !GameSaves.working
                onClicked: GameSaves.backUpNow(root.game.id)
            }
            SettingsButton {
                text: qsTr("Choose game")
                visible: root.saves.hasManifest ?? false
                onClicked: root.chooseGame()
            }
            Label {
                Layout.fillWidth: true
                text: GameSaves.working ? qsTr("Backing up") : GameSaves.message
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
                elide: Text.ElideRight
            }
        }

        Label {
            visible: root.backups.length === 0
            text: qsTr("No backups yet.")
            color: Theme.gameEditor.labelColor
        }

        ListView {
            id: list
            visible: root.backups.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 6 * Theme.controls.buttonHeight + 5 * spacing)
            spacing: Theme.layout.spacing
            clip: true
            model: root.backups
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar {}

            delegate: RowLayout {
                id: backupRow
                required property var modelData
                width: ListView.view.width
                spacing: Theme.layout.spacing

                ColumnLayout {
                    Layout.preferredWidth: 150
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: backupRow.modelData.when
                        color: Theme.gameEditor.labelColor
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: backupRow.modelData.size
                        color: Theme.gameEditor.hintColor
                        font.pixelSize: Theme.gameEditor.hintFontSize
                    }
                }
                SettingsTextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name")
                    text: backupRow.modelData.name
                    onEditingFinished: {
                        if (text.trim() !== backupRow.modelData.name)
                            root.rename(backupRow.modelData.path, text)
                    }
                }
                SettingsButton {
                    text: qsTr("Restore")
                    enabled: !root.restoring
                    onClicked: root.restore(backupRow.modelData.path)
                }
            }
        }

        Label {
            visible: root.backups.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Restoring moves the saves you have now aside, next to where they are. The last %1 backups are kept in %2; a backup with a name is always kept.")
                      .arg(root.saves.keep ?? 0).arg(root.saves.folder ?? "")
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
