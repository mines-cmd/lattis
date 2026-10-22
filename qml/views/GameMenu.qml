import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
SettingsMenu {
    id: root

    pops: true

    property var game: ({})
    property var games: []

    readonly property bool multi: games.length > 1
    readonly property bool onSteam: game.platform === "steam" || game.platform === "proton"
    readonly property bool programGame: game.platform === "proton" || game.platform === "linux"
    readonly property bool running: GameRunner.running.includes(game.id ?? "")
    readonly property bool installed: game.installed ?? true

    readonly property bool allFavorite: games.every(g => g.favorite)
    readonly property bool anyVisible: games.some(g => !g.hidden)
    readonly property bool emulatorChoosable: multi && !["steam", "proton", "linux"].includes(games[0].platform)
                                              && games.every(g => g.platform === games[0].platform)
    readonly property var emulatorChoices: emulatorChoosable ? Library.emulatorChoices(games[0].platform).choices : []
    property var tagStates: ({})

    signal chosen(string name)
    signal failed(string text)
    signal newTagRequested(var ids)

    onGamesChanged: {
        const states = {}
        for (const g of games) {
            for (const tag of g.tags)
                states[tag.name] = (states[tag.name] ?? 0) + 1
        }
        for (const name of Object.keys(states))
            states[name] = states[name] === games.length ? 1 : 2
        tagStates = states
    }

    function ids() { return games.map(g => g.id) }

    function syncSubmenus() {
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item?.subMenu === tagsMenu || item?.subMenu === emulatorMenu)
                item.visible = multi
            if (item?.subMenu === emulatorMenu)
                item.enabled = emulatorChoosable
        }
    }
    function openSubmenu(which) {
        const submenu = which === "emulator" ? emulatorMenu : tagsMenu
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item?.subMenu === submenu) {
                currentIndex = i
                submenu.popup(item, item.width, 0)
            }
        }
    }
    onMultiChanged: syncSubmenus()
    onEmulatorChoosableChanged: syncSubmenus()
    Component.onCompleted: syncSubmenus()

    function toggleTag(name) {
        const on = (tagStates[name] ?? 0) === 1
        const error = Library.retagGames(ids(), on ? [] : [name], on ? [name] : [])
        if (error !== "")
            failed(error)
        const states = Object.assign({}, tagStates)
        states[name] = on ? 0 : 1
        tagStates = states
    }

    function pinEmulator(id) {
        const error = Library.setGamesValue(ids(), "emulator", id)
        if (error !== "")
            failed(error)
    }

    component Entry: SettingsMenuItem {
        property string actionName
        keys: Actions.label(Actions.keys[actionName] ?? [])
        onTriggered: Qt.callLater(() => root.chosen(actionName))
    }
    component TagRow: Item {
        id: tagRow

        required property var modelData
        readonly property var tag: modelData
        readonly property int state: root.tagStates[tag.name] ?? 0

        implicitHeight: Theme.menu.itemHeight
        implicitWidth: tagContent.implicitWidth + 20

        Rectangle {
            anchors.fill: parent
            radius: Theme.menu.itemRadius
            color: tagHover.hovered ? Theme.controls.hover : "transparent"
        }
        RowLayout {
            id: tagContent
            anchors.fill: parent
            anchors.leftMargin: Theme.menu.itemPadding
            anchors.rightMargin: Theme.menu.itemPadding
            spacing: Theme.menu.itemSpacing

            Item {
                implicitWidth: 16
                Layout.fillHeight: true
                Icon {
                    anchors.centerIn: parent
                    visible: tagRow.state === 1
                    path: Icons.check
                    size: 16
                    color: Theme.controls.inputText
                }
                Rectangle {
                    anchors.centerIn: parent
                    visible: tagRow.state === 2
                    width: 8
                    height: 2
                    color: Theme.controls.inputText
                }
            }
            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: height / 2
                color: Theme.tags.palette.length > 0 ? Theme.tags.palette[tagRow.tag.color % Theme.tags.palette.length]
                                                     : Theme.filters.chipBackground
            }
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: tagRow.tag.name
                color: Theme.controls.inputText
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
        HoverHandler {
            id: tagHover
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            // After the tap: the change reloads the tags, which rebuilds these rows.
            onTapped: Qt.callLater(root.toggleTag, tagRow.tag.name)
        }
    }

    Entry {
        actionName: "launch"
        iconPath: !root.installed && root.game.platform === "steam" ? Icons.download2Line
                : !root.installed && root.programGame ? Icons.fileSearchLine
                : Icons.playLargeLine
        visible: !root.multi
        text: root.running ? Theme.launchButton.runningLabel
            : root.installed ? Theme.launchButton.label
            : root.game.platform === "steam" ? Theme.launchButton.installLabel
            : root.programGame ? qsTr("Locate")
            : Theme.launchButton.missingRomLabel
        enabled: !root.running && (root.installed || root.onSteam || root.programGame)
    }
    SettingsMenuSeparator { visible: !root.multi }
    Entry {
        actionName: "toggle_favorite"
        iconPath: (root.multi ? root.allFavorite : (root.game.favorite ?? false)) ? Icons.starOffLine : Icons.starLine
        text: (root.multi ? root.allFavorite : (root.game.favorite ?? false)) ? qsTr("Remove from favorites")
                                                                             : qsTr("Add to favorites")
    }
    SettingsMenu {
        id: tagsMenu
        pops: true
        title: qsTr("Tags")
        iconPath: Icons.tagLine
        checkColumn: true

        Instantiator {
            model: root.multi ? Library.tags : []
            delegate: TagRow {}
            onObjectAdded: (index, object) => tagsMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => tagsMenu.removeItem(object)
        }
        SettingsMenuSeparator {}
        SettingsMenuItem {
            text: qsTr("New tag")
            iconPath: Icons.add
            onTriggered: Qt.callLater(() => root.newTagRequested(root.ids()))
        }
    }
    SettingsMenu {
        id: emulatorMenu
        pops: true
        title: root.emulatorChoosable ? qsTr("Emulator") : qsTr("Emulator (ROM games of one platform)")
        iconPath: Icons.gamepadLine
        checkColumn: true

        SettingsMenuItem {
            text: qsTr("Platform default")
            checked: root.multi && root.games.every(g => g.emulator === "")
            onTriggered: root.pinEmulator("")
        }
        Instantiator {
            model: root.emulatorChoices
            delegate: SettingsMenuItem {
                required property var modelData
                text: modelData.installed ? modelData.name : qsTr("%1 (not installed)").arg(modelData.name)
                checked: root.games.every(g => g.emulator === modelData.id)
                onTriggered: root.pinEmulator(modelData.id)
            }
            onObjectAdded: (index, object) => emulatorMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => emulatorMenu.removeItem(object)
        }
    }
    Entry {
        actionName: "open_platform"
        iconPath: root.onSteam ? Icons.steamLine : Icons.gamepadLine
        visible: !root.multi && root.game.platform !== "linux"
        text: root.onSteam ? qsTr("Open in Steam") : qsTr("Open emulator")
    }
    Entry {
        actionName: "open_folder"
        iconPath: Icons.folderOpenLine
        visible: !root.multi
        text: qsTr("Open game folder")
    }
    Entry {
        actionName: "open_save_folder"
        iconPath: Icons.save2Line
        visible: !root.multi && GameRunner.saveFolder(root.game.id ?? "") !== ""
        text: qsTr("Open save folder")
    }
    Entry {
        actionName: "play_history"
        iconPath: Icons.history
        visible: !root.multi
        text: qsTr("Play history")
    }
    SettingsMenuSeparator {}
    Entry {
        actionName: "game_settings"
        iconPath: Icons.settings4Line
        visible: !root.multi
        text: qsTr("Properties")
    }
    Entry {
        actionName: "hide_game"
        iconPath: (root.multi ? !root.anyVisible : (root.game.hidden ?? false)) ? Icons.eyeLine : Icons.eyeCloseLine
        text: (root.multi ? !root.anyVisible : (root.game.hidden ?? false)) ? qsTr("Unhide") : qsTr("Hide")
    }
    Entry {
        actionName: "uninstall_game"
        iconPath: Icons.deleteBin6Line
        visible: !root.multi && (root.game.platform === "steam" ? (root.game.steamUninstallable ?? false) : root.programGame)
        text: qsTr("Uninstall")
        danger: true
    }
}
