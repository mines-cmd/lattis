import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    property var game: ({})
    readonly property var saves: GameSaves.state.requested === game.id ? GameSaves.state : ({})
    readonly property var backups: saves.backups ?? []
    readonly property var paths: saves.paths ?? []
    readonly property bool matched: (saves.matched ?? "") !== ""
    readonly property bool fallback: saves.fallback ?? false
    readonly property bool restoring: Backups.importing
    readonly property int keep: saves.keep ?? 0
    readonly property bool empty: !fallback && paths.length === 0 && backups.length === 0

    signal message(string text)
    signal chooseRequested()
    signal chooseFolderRequested()

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

    Layout.fillWidth: true
    spacing: Theme.gameEditor.rowSpacing

    Connections {
        target: Backups
        function onImportFinished(kind, error) {
            if (kind === "game-saves" && error === "")
                GameSaves.load(root.game.id)
        }
    }

    ColumnLayout {
        visible: root.empty
        Layout.fillWidth: true
        Layout.topMargin: Theme.gameEditor.rowSpacing
        Layout.bottomMargin: Theme.gameEditor.rowSpacing
        spacing: Theme.gameEditor.rowSpacing

        Icon {
            Layout.alignment: Qt.AlignHCenter
            path: Icons.save2Line
            size: Theme.controls.iconSize
            color: Theme.gameEditor.hintColor
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No saves found")
            color: Theme.gameEditor.labelColor
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
            text: root.matched ? qsTr("None of its save files exist on this computer yet. Pick another game from the Ludusavi list, or choose its save folder.")
                : qsTr("Pick this game from the Ludusavi list, or choose its save folder.")
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.layout.spacing

            SettingsButton {
                text: qsTr("Change match")
                visible: root.saves.hasManifest ?? false
                tip: qsTr("Choose which game in the Ludusavi list this one's saves are taken from")
                onClicked: root.chooseRequested()
            }
            SettingsButton {
                text: qsTr("Choose folder")
                onClicked: root.chooseFolderRequested()
            }
        }
    }
    EditorField {
        visible: !root.empty
        heading: true
        label: qsTr("Backups")
        help: qsTr("Keeps the last %1 backups in %2. A backup with a name is always kept. Backing up, "
                   + "renaming, deleting and restoring happen at once, and Cancel doesn't undo them. "
                   + "Mod manager links wait for Save.").arg(root.keep).arg(root.saves.folder ?? "")
        actions: Row {
            spacing: Theme.layout.spacing

            SettingsButton {
                iconPath: Icons.folderOpenLine
                flat: true
                visible: (root.saves.folder ?? "") !== ""
                tip: qsTr("Open the backups folder")
                onClicked: Qt.openUrlExternally("file://" + root.saves.folder)
            }
            SettingsButton {
                text: GameSaves.working ? qsTr("Backing up") : qsTr("Back up now")
                enabled: root.paths.length > 0 && !GameSaves.working
                onClicked: GameSaves.backUpNow(root.game.id)
            }
        }
    }
    Label {
        visible: root.fallback
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
        text: qsTr("No save folders are known for this game, so its whole Proton prefix is backed up, without Proton's own files.")
    }

    Label {
        visible: GameSaves.working || GameSaves.message !== ""
        Layout.fillWidth: true
        text: GameSaves.working ? qsTr("Backing up") : GameSaves.message
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
        elide: Text.ElideRight
    }

    Label {
        visible: root.backups.length === 0 && (root.paths.length > 0 || root.fallback)
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("No backups yet. Back up now, or one is made when the game closes.")
        color: Theme.gameEditor.hintColor
    }

    Rectangle {
        visible: root.backups.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(list.contentHeight, 4 * (Theme.controls.buttonHeight + 2 * Theme.gameEditor.rowSpacing))
        radius: Theme.controls.inputRadius
        color: Theme.gameEditor.background
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
                                Layout.maximumWidth: backupRow.width / 2
                                text: backupRow.named ? backupRow.modelData.name : backupRow.modelData.when
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
                            text: backupRow.named ? qsTr("%1 · %2").arg(backupRow.modelData.when).arg(backupRow.modelData.size)
                                : backupRow.modelData.size
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
                        Keys.onEscapePressed: event => {
                            text = backupRow.modelData.name
                            backupRow.renaming = false
                            event.accepted = true
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
}
