import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    property var game: ({})
    readonly property string platform: game.platform ?? ""
    readonly property bool wanted: (game.id ?? "") !== "" && !(game.rom ?? false)
                                   && (platform === "steam" || platform === "proton" || platform === "linux")
    readonly property var saves: wanted && GameSaves.state.requested === game.id ? GameSaves.state : ({})
    readonly property var backups: saves.backups ?? []
    readonly property var paths: saves.paths ?? []
    readonly property bool matched: (saves.matched ?? "") !== ""
    readonly property bool fallback: saves.fallback ?? false
    readonly property int shownPaths: 3
    readonly property bool restoring: Backups.importing

    signal failed(string text)

    function reload() {
        if (wanted)
            GameSaves.load(game.id)
    }

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
            failed(error)
    }

    visible: (saves.eligible ?? false) && ((saves.hasManifest ?? false) || backups.length > 0 || paths.length > 0)
    spacing: 10

    onGameChanged: reload()
    onWantedChanged: reload()
    Component.onCompleted: reload()

    Connections {
        target: Backups
        function onImportFinished(kind, error) {
            if (kind === "game-saves" && error === "")
                root.reload()
        }
    }

    Loader {
        id: picker
        active: false
        source: "SavesGamePicker.qml"
        onLoaded: item.message.connect(text => root.failed(text))
    }

    Label {
        text: qsTr("Save backups")
        color: Theme.details.headingColor
        font.weight: Theme.font.strongWeight
        font.pixelSize: Theme.details.headingFontSize
    }

    Surface {
        Layout.fillWidth: true
        implicitHeight: column.implicitHeight + 2 * Theme.media.cardPadding
        radius: Theme.media.cardRadius
        fill: Theme.media.cardBackground
        border.color: Theme.media.cardBorder
        border.width: Theme.media.cardBorderWidth

        ColumnLayout {
            id: column
            anchors { fill: parent; margins: Theme.media.cardPadding }
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.details.descriptionColor
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
                    color: Theme.details.countColor
                    font.family: Theme.font.monoFamily
                    font.pixelSize: Theme.details.countFontSize
                    elide: Text.ElideMiddle
                }
            }
            Label {
                visible: root.paths.length > root.shownPaths
                text: qsTr("and %n more", "", root.paths.length - root.shownPaths)
                color: Theme.details.countColor
                font.pixelSize: Theme.details.countFontSize
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.details.descriptionColor
                text: root.backups.length === 0 ? qsTr("No backups yet.")
                    : qsTr("Last backup %1, %2").arg(root.backups[0].when).arg(root.backups[0].size)
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
                    onClicked: {
                        picker.active = true
                        picker.item.gameId = root.game.id
                        picker.item.gameTitle = root.game.title ?? ""
                        picker.item.chosen = root.saves.match === "chosen" ? (root.saves.matched ?? "") : ""
                        picker.item.open()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: GameSaves.working ? qsTr("Backing up") : GameSaves.message
                    color: Theme.details.countColor
                    font.pixelSize: Theme.details.countFontSize
                    elide: Text.ElideRight
                }
            }

            Repeater {
                model: root.backups

                RowLayout {
                    id: backupRow
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    Label {
                        Layout.fillWidth: true
                        text: backupRow.modelData.when
                        color: Theme.details.descriptionColor
                    }
                    Label {
                        text: backupRow.modelData.size
                        color: Theme.details.countColor
                        font.pixelSize: Theme.details.countFontSize
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
                text: qsTr("Restoring moves the saves you have now aside, next to where they are. The last %1 backups are kept in %2.")
                          .arg(root.saves.keep ?? 0).arg(root.saves.folder ?? "")
                color: Theme.details.countColor
                font.pixelSize: Theme.details.countFontSize
            }
        }
    }
}
