import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property var game: ({})
    property bool startedUp: false
    property bool allCollapsed: false
    property real dragReserve: 0
    property real toolbarLeft: -1
    readonly property real noticeX: Theme.details.marginLeft + Theme.details.padding
    readonly property real noticeWidth: {
        const right = Math.min(width - marginRight - Theme.details.padding, width - dragReserve,
                               toolbarLeft < 0 ? Infinity : toolbarLeft - Theme.layout.spacing)
        return Math.max(0, right - noticeX)
    }

    // The hero follows the selection after a short delay, so holding an arrow key in the
    // list doesn't decode every image on the way. Text updates immediately.
    property var heroGame: ({})
    readonly property alias shownHeroGame: hero.shownGame

    readonly property bool hasGame: game.id !== undefined
    readonly property bool downloading: hasGame && Theme.downloadBar.show && Library.download.gameId !== undefined
    readonly property bool collapsed: General.downloadBarCollapsed
    property real slide: collapsed ? 1 : 0
    Behavior on slide { enabled: Motion.on; MotionNumber { duration: Motion.duration(Theme.downloadBar.collapseDuration) } }
    // Too narrow for the stat chips beside Launch and the info panel beside the description
    // (details.compact_width). Measured between the wide margins, as the compact right margin
    // widens the content.
    function compactAt(w: real): bool {
        return Theme.details.compactWidth > 0
            && w - Theme.details.marginLeft - Theme.details.marginRight - 2 * Theme.details.padding < Theme.details.compactWidth
    }
    readonly property bool compact: compactAt(width)
    readonly property int marginRight: compact ? Theme.details.compactMarginRight : Theme.details.marginRight
    property string descriptionExpandedFor: ""
    readonly property string launcher: game.launcher ?? ""

    // An emulated game none of whose emulators is found: Launch installs the first one, if it can be.
    // The Installer is only touched then, so it isn't made for a page that doesn't need it.
    readonly property string missingEmulator: {
        void game.launchTip
        void Library.emulators
        return hasGame && game.installed ? GameRunner.missingEmulator(game.id) : ""
    }
    readonly property var emulatorRow: missingEmulator !== "" ? Installer.emulators[missingEmulator] : undefined
    readonly property bool emulatorInstallable: (emulatorRow?.routes?.length ?? 0) > 0
    readonly property string emulatorName: emulatorInstallable
        ? (Library.emulators.find(e => e.id === missingEmulator)?.name ?? "") : ""
    readonly property string emulatorState: emulatorRow?.state ?? "idle"
    readonly property string emulatorStep: {
        switch (emulatorState) {
        case "checking": return qsTr("Checking")
        case "downloading": return emulatorRow.progress >= 0 ? qsTr("%1 %").arg(Math.round(emulatorRow.progress * 100)) : qsTr("Downloading")
        case "extracting": return qsTr("Unpacking")
        case "installing": return qsTr("Installing")
        default: return ""
        }
    }
    // Tells of a failure that happened here, not one left over from before this game was shown.
    property bool emulatorWasBusy: false
    function openInstallDialog() {
        if (emulatorName === "")
            return
        installDialog.active = true
        installDialog.item.openFor(missingEmulator)
    }

    Loader {
        id: installDialog
        active: false
        source: "EmulatorInstallDialog.qml"
    }
    Connections {
        target: installDialog.item
        function onError(text) { root.message(text) }
    }

    onEmulatorStepChanged: {
        const busy = emulatorStep !== ""
        if (emulatorWasBusy && !busy && emulatorState === "failed" && !(installDialog.item?.dialog.visible ?? false))
            emulatorMessage(emulatorRow.error, game.id)
        emulatorWasBusy = busy
    }

    signal launchRequested()
    signal stopRequested()
    signal message(string text)
    signal emulatorMessage(string text, string gameId)
    signal clicked()
    signal editRequested()
    signal addRequested()
    signal steamUndoRequested()
    signal historyRequested()
    signal menuRequested(Item button)
    signal customizeRequested()
    signal galleryRequested(string id)
    signal modManagerRequested(string id)

    function logoForScreenshot(mode) {
        if (mode === "adjust")
            hero.startAdjust()
        else if (mode.startsWith("scrim"))
            hero.openScrimMenu(mode === "scrim/side")
        else
            hero.openMenu()
    }

    // The description, tags and Media row, in the order of `[game_page] sections`.
    component TextBlock: GridLayout {
        id: block
        property var game: ({})
        property int maxLines: 2147483647
        property bool cut: false
        property bool expanded: false
        readonly property alias descriptionHeading: heading
        readonly property alias description: desc
        readonly property alias moreLink: more
        readonly property alias tagsHeading: tagsTitle
        readonly property alias tagsFlow: flow
        readonly property alias tagRepeater: tags
        // A switch fades the content (the description and Show more) and leaves the Description
        // heading, which is always in the same place, on screen. The tags section goes with the
        // content unless both games have tags: then it stays, and morphs.
        property real contentOpacity: 1
        property real contentShift: 0
        property real descriptionHeadingOpacity: 1
        property real tagsOpacity: contentOpacity
        property real tagsShift: contentShift
        property bool chipsHidden: false
        // The copy that fades out the old game's text loads no clips: it keeps the room its Media row took.
        property bool liveMedia: true
        property real heldMediaHeight: 0
        readonly property alias mediaRow: media
        readonly property bool mediaShown: liveMedia ? media.visible : heldMediaHeight > 0
        property real spacing: 10

        signal moreClicked()
        signal mediaRequested(var item, var list)
        signal galleryRequested(string id)
        signal clipMenuRequested(var clip, Item tile, point position)

        columns: 1
        rowSpacing: spacing
        columnSpacing: 0

        signal failed(string text)

        readonly property var shownSections: General.gamePageSections.filter(
            name => name === "description" ? heading.visible : name === "tags" ? tagsTitle.visible : mediaShown)
        function rowOf(name, offset) {
            return Math.max(0, General.gamePageSections.indexOf(name)) * 3 + offset
        }
        // The description's last line carries the room its line height leaves under the text; a section
        // after anything else gets that room as margin, so every gap between sections looks the same.
        FontMetrics { id: descMetrics; font: desc.font }
        function gapBefore(name) {
            const at = shownSections.indexOf(name)
            if (at <= 0)
                return 0
            const afterText = shownSections[at - 1] === "description" && desc.visible && !more.visible
            return 14 + (afterText ? 0 : descMetrics.height * (desc.lineHeight - 1))
        }

        CollapseHeading {
            id: heading
            Layout.row: rowOf("description", 0)
            Layout.topMargin: gapBefore("description")
            opacity: descriptionHeadingOpacity
            visible: General.showDescription && desc.text.trim() !== ""
            text: qsTr("Description")
            section: "description"
            onFailed: text => block.failed(text)
        }
        Label {
            id: desc
            opacity: contentOpacity
            transform: Translate { y: contentShift }
            visible: heading.visible && !heading.collapsed
            Layout.row: rowOf("description", 1)
            Layout.fillWidth: true
            text: game.description ?? ""
            color: Theme.details.descriptionColor
            wrapMode: Text.WordWrap
            lineHeight: Theme.details.descriptionLineHeight
            maximumLineCount: maxLines
            elide: Text.ElideRight
        }
        Label {
            id: more
            opacity: contentOpacity
            transform: Translate { y: contentShift }
            visible: desc.visible && cut
            Layout.row: rowOf("description", 2)
            text: expanded ? qsTr("Show less") : qsTr("Show more")
            color: Theme.details.linkColor
            font.underline: moreMouse.containsMouse

            MouseArea {
                id: moreMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: moreClicked()
            }
        }

        CollapseHeading {
            id: tagsTitle
            opacity: tagsOpacity
            transform: Translate { y: tagsShift }
            visible: General.showTags && tags.count > 0
            Layout.row: rowOf("tags", 0)
            Layout.topMargin: gapBefore("tags")
            text: qsTr("Tags")
            section: "tags"
            onFailed: text => block.failed(text)
        }
        // Another game's tags replace these at once: the switch has its own motion. A tag added to or
        // removed from this game pops in or out.
        KeyedListModel {
            id: tagModel
            leaveSpeed: "fast"
        }
        function showTags() {
            tagModel.scope = game.id
            tagModel.source = (game.tags ?? []).map(t => ({ key: t.name, name: t.name, color: t.color }))
        }
        onGameChanged: showTags()
        Component.onCompleted: showTags()

        Flow {
            id: flow
            // On the Flow, not the chips: the add transition takes over a chip's opacity.
            opacity: chipsHidden ? 0 : tagsOpacity
            transform: Translate { y: tagsShift }
            visible: tagsTitle.visible && !tagsTitle.collapsed
            Layout.row: rowOf("tags", 1)
            Layout.fillWidth: true
            spacing: Theme.tags.spacing

            add: Transition {
                enabled: Motion.on && !tagModel.rebuilt
                MotionNumber { property: "opacity"; from: 0; to: 1 }
                MotionPop { property: "scale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
            }
            move: Transition {
                enabled: Motion.on && !tagModel.rebuilt
                MotionNumber { properties: "x,y"; speed: "normal" }
            }

            Repeater {
                id: tags
                model: tagModel

                TagChip {
                    id: chip
                    required property var model
                    readonly property bool leaving: model.leaving
                    name: model.name
                    colorIndex: model.color
                    lit: tagMouse.containsMouse
                    raised: tagMouse.containsMouse
                    raiseBy: Theme.details.tagHoverLift
                    pressed: tagMouse.pressed

                    PopLeave { target: chip; leaving: chip.leaving }

                    MouseArea {
                        id: tagMouse
                        readonly property bool filtering: Library.tagFilter.includes(chip.name)
                        anchors.fill: parent
                        // Down by the lift, so the chip rising doesn't take the pointer off it at the bottom edge.
                        anchors.bottomMargin: -Theme.details.tagHoverLift
                        visible: !chip.leaving
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Library.tagFilter = filtering ? Library.tagFilter.filter(t => t !== chip.name)
                                                                 : Library.tagFilter.concat([chip.name])

                        ToolTip.visible: containsMouse
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: filtering ? qsTr("Stop filtering by %1").arg(chip.name)
                                                : qsTr("Show games tagged %1").arg(chip.name)
                    }
                }
            }
        }

        MediaRow {
            id: media
            active: liveMedia
            Layout.row: rowOf("clips", 0)
            Layout.topMargin: gapBefore("clips")
            Layout.fillWidth: true
            opacity: contentOpacity
            game: block.game
            onFailed: text => block.failed(text)
            onMediaRequested: (item, list) => block.mediaRequested(item, list)
            onShowAllRequested: id => block.galleryRequested(id)
            onMenuRequested: (clip, tile, position) => block.clipMenuRequested(clip, tile, position)
        }
        Item {
            visible: !liveMedia && heldMediaHeight > 0
            Layout.row: rowOf("clips", 0)
            Layout.topMargin: gapBefore("clips")
            Layout.preferredHeight: heldMediaHeight
        }
    }

    // The description and tags follow the hero's shown game and switch with the logo. The old content
    // rises away and fades out as soon as another game is picked (`textExit`, over motion.fast), while
    // the hero waits and loads; the new rises into place with the logo, as far and on the same curve.
    // One after the other, so they never overlap; the Description heading stays on screen between
    // them, so it doesn't blink. The exit has its own time: on the hero's curve it was over in a
    // frame or two. When both games have tags, the Tags heading glides to its new place with the logo
    // and the chips morph into the new ones under it.
    property var textGame: ({})
    readonly property bool textPending: textGame.id !== undefined && textGame.id !== game.id
    property bool textExiting: false
    property real textExit: 0
    property real textIn: 1
    property double lastPick: 0
    property string lastPickId: ""
    // Off while a switch is still in flight: picking games quickly only fades, it doesn't rise.
    property bool riseOn: true
    readonly property real riseDistance: riseOn ? Theme.hero.switchRise : 0
    readonly property bool textLeaving: leavingText.game.id !== undefined && textIn < 1
    // The copy's own exit, so a new one starting (textExit back to 0) doesn't bring the old text back.
    property real leavingExit: 1
    readonly property bool tagsMorphing: textLeaving && leavingText.tagsFlow.visible && textColumn.tagsFlow.visible
    readonly property bool tagsStay: textPending && textColumn.tagsHeading.visible && showsTags(game)

    function showsDescription(g): bool {
        return General.showDescription && (g.description ?? "").trim() !== ""
    }
    function showsTags(g): bool {
        return General.showTags && (g.tags ?? []).length > 0
    }

    onGameChanged: {
        if (heroGame.id === undefined || Theme.hero.loadDelay === 0)
            heroGame = game
        else
            heroTimer.restart()

        const now = Date.now()
        const rapid = lastPick > 0 && now - lastPick < Theme.details.rapidSwitch
        if (game.id !== undefined && game.id !== lastPickId) {
            hero.instant = rapid
            lastPick = now
            lastPickId = game.id
        }
        if (rapid && game.id !== undefined && textGame.id !== undefined && textGame.id !== game.id) {
            textExitAnimation.stop()
            textInAnimation.stop()
            leavingExitAnimation.stop()
            textExiting = false
            textExit = 0
            textIn = 1
            leavingText.game = ({})
            textGame = game
            return
        }

        if (game.id !== undefined && textGame.id !== undefined && game.id !== textGame.id)
            riseOn = !(textExiting || textPending || textIn < 1)

        // Not textPending: that binding hasn't seen the new game yet.
        if (textGame.id === undefined || game.id === undefined || textGame.id === game.id
                || Motion.duration(Theme.motion.fast) === 0) {
            textExiting = false
            textExitAnimation.stop()
            textExit = 0
        } else if (!textExiting) {
            textExiting = true
            textExitAnimation.from = 1 - textIn
            textExitAnimation.restart()
        }
    }

    MotionNumber {
        id: textExitAnimation
        target: root
        property: "textExit"
        to: 1
    }
    MotionNumber {
        id: leavingExitAnimation
        target: root
        property: "leavingExit"
        to: 1
    }
    MotionNumber {
        id: textInAnimation
        target: root
        property: "textIn"
        from: 0
        to: 1
        speed: "normal"
    }

    // The new layer's own arrival: the hero's shown layer is already the new one here.
    onShownHeroGameChanged: {
        // A rapid pick already put the text on the selection; the hero catching up with an earlier
        // game must not take it back.
        if (textGame.id !== undefined && textGame.id === game.id && shownHeroGame.id !== game.id)
            return

        const switching = textGame.id !== undefined && shownHeroGame.id !== undefined
                          && textGame.id !== shownHeroGame.id && (hero.shown?.arrival ?? 1) < 1
        if (switching) {
            leavingText.x = textColumn.x
            leavingText.y = textColumn.y
            leavingText.width = textColumn.width
            leavingText.game = textGame
            leavingText.maxLines = textColumn.maxLines
            leavingText.cut = textColumn.cut
            leavingText.expanded = textColumn.expanded
            leavingText.heldMediaHeight = textColumn.mediaShown ? textColumn.mediaRow.height : 0
            leavingExit = textExit
            leavingExitAnimation.duration = Motion.duration(Theme.motion.fast) * (1 - textExit)
            leavingExitAnimation.restart()
        } else {
            leavingText.game = ({})
        }
        textExiting = false
        textInAnimation.stop()
        textIn = switching ? 0 : 1
        textGame = shownHeroGame
        if (switching)
            textInAnimation.start()
    }

    property string openingId: ""

    Timer {
        id: openingTimer
        interval: Theme.iconButton.busyTimeout * 1000
        onTriggered: root.openingId = ""
    }
    Connections {
        target: GameRunner
        function onEmulatorExited(gameId) {
            if (gameId === root.openingId)
                root.openingId = ""
        }
    }
    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window.active)
                root.openingId = ""
        }
    }

    Timer {
        id: heroTimer
        interval: Theme.hero.loadDelay
        onTriggered: root.heroGame = root.game
    }

    // Only for the shown game: it looks at the disk, so it isn't in gameAt() for every row.
    readonly property string saveFolder: General.gamePageButtons.includes("save_folder") && game.id !== undefined
                                         ? GameRunner.saveFolder(game.id) : ""

    readonly property var managers: game.id !== undefined
        ? Library.modManagers.filter(m => m.games.some(g => g.id === game.id)) : []
    readonly property bool managerRunning: managers.some(m => GameRunner.runningManagers.includes(m.id))

    readonly property var actions: ({
        favorite: {
            icon: Icons.star,
            color: (game.favorite ?? false) ? Theme.iconButton.iconActive : Theme.iconButton.iconInactive,
            tip: Actions.tip((game.favorite ?? false) ? qsTr("Remove from favorites") : qsTr("Add to favorites"),
                             Actions.keys.toggle_favorite),
            badge: false
        },
        open_platform: {
            icon: Icons.openExternal,
            color: Theme.iconButton.icon,
            tip: Actions.tip(launcher === "steam" ? qsTr("Open in Steam")
                             : launcher === "heroic" ? qsTr("Open Heroic") : qsTr("Open emulator"),
                             Actions.keys.open_platform),
            badge: false
        },
        folder: {
            icon: Icons.folder,
            color: Theme.iconButton.icon,
            tip: Actions.tip(qsTr("Open game folder"), Actions.keys.open_folder),
            badge: false
        },
        save_folder: {
            icon: Icons.save2Line,
            color: Theme.iconButton.icon,
            tip: Actions.tip(qsTr("Open save folder: %1").arg(GameRunner.shownPath(saveFolder)),
                             Actions.keys.open_save_folder),
            badge: false
        },
        settings: {
            icon: Icons.gear,
            color: Theme.iconButton.icon,
            tip: Actions.tip((game.pendingSteamEdits ?? false) ? qsTr("Properties — changes are queued until Steam closes")
                                                               : qsTr("Properties"),
                             Actions.keys.game_settings),
            badge: game.pendingSteamEdits ?? false
        },
        gallery: {
            icon: Icons.image,
            color: Theme.iconButton.icon,
            tip: Actions.tip(qsTr("Show in Gallery"), Actions.keys.gallery),
            badge: false
        },
        mod_manager: {
            icon: Icons.puzzle,
            color: Theme.iconButton.icon,
            tip: Actions.tip(managers.length === 1 ? (managerRunning ? qsTr("%1 is running") : qsTr("Open %1")).arg(managers[0].name)
                                                   : qsTr("Mod managers"),
                             Actions.keys.open_mod_manager),
            badge: managerRunning
        },
        menu: {
            icon: Icons.more,
            color: Theme.iconButton.icon,
            tip: Actions.tip(qsTr("Game menu"), Actions.keys.game_menu),
            badge: false
        }
    })

    // The right-click menu of one place on the page (chips, buttons or info_rows), made on first use.
    Loader {
        id: pageMenu
        active: false
        source: "GamePageMenu.qml"
    }
    Connections {
        target: pageMenu.item
        function onCustomizeRequested() { root.customizeRequested() }
        function onMessage(text) { root.message(text) }
    }
    Loader {
        id: clipMenu
        active: false
        source: "ClipMenu.qml"
    }
    Connections {
        target: clipMenu.item
        function onMessage(text) { root.message(text) }
    }
    function openClipMenu(clip, item, point) {
        clipMenu.active = true
        clipMenu.item.media = clip
        clipMenu.item.popup(item, point.x, point.y)
    }

    // Qt Multimedia loads with the viewer, when the first clip or screenshot is opened.
    Loader {
        id: mediaViewer
        active: false
        source: "MediaViewer.qml"
    }
    Connections {
        target: mediaViewer.item
        function onMessage(text) { root.message(text) }
        function onClipGone() { textColumn.mediaRow.reload() }
    }
    function showMedia(item, list) {
        mediaViewer.active = true
        mediaViewer.item.show(item, list)
    }

    // "screenshot" opens the first screenshot, "menu" the first tile's menu, anything else plays the first clip.
    function clipForScreenshot(spec) {
        const list = textColumn.mediaRow.shown
        const item = spec === "menu" ? list[0] : list.find(i => spec === "screenshot" ? i.kind === "screenshot" : i.kind !== "screenshot")
        if (item === undefined)
            return
        if (spec === "menu")
            openClipMenu(item, textColumn.mediaRow, Qt.point(100, textColumn.mediaRow.height - 40))
        else
            showMedia(item, list)
    }

    function openPageMenu(place, item, point) {
        pageMenu.active = true
        pageMenu.item.place = place
        pageMenu.item.popup(item, point.x, point.y)
    }

    function pageMenuForScreenshot(place, submenu) {
        const target = place === "chips" ? statsFlow : place === "buttons" ? buttons : place === "friends" ? friends : info
        openPageMenu(place, target, Qt.point(target.width / 2, target.height / 2))
        if (submenu !== undefined) {
            const menu = pageMenu.item
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i)
                if (item.subMenu)
                    item.subMenu.popup(item, item.width, 0)
            }
        }
    }

    function openMenu() {
        for (let i = 0; i < buttonRepeater.count; ++i) {
            const button = buttonRepeater.itemAt(i)
            if (button.modelData === "menu")
                menuRequested(button)
        }
    }

    Loader {
        id: modManagerMenu
        active: false
        source: "ModManagerMenu.qml"
    }
    Connections {
        target: modManagerMenu.item
        function onChosen(id) { root.modManagerRequested(id) }
    }
    function openModManagerMenu(list) {
        modManagerMenu.active = true
        const menu = modManagerMenu.item
        menu.managers = list
        let anchor = buttons
        for (let i = 0; i < buttonRepeater.count; ++i) {
            if (buttonRepeater.itemAt(i).modelData === "mod_manager")
                anchor = buttonRepeater.itemAt(i)
        }
        menu.popup(anchor, anchor.width - menu.width, anchor.height + Theme.menu.buttonGap)
    }

    function activate(button, other) {
        const g = other ?? game
        if (g.id === undefined)
            return
        if (button === "mod_manager") {
            const list = Library.modManagersFor(g.id)
            if (list.length === 1)
                modManagerRequested(list[0].id)
            else if (list.length > 1)
                openModManagerMenu(list)
            return
        }
        if (button === "favorite") {
            const error = Library.setGameValue(g.id, "favorite", !(g.favorite ?? false))
            if (error !== "")
                message(error)
            return
        }
        let text = actions[button].message ?? ""
        if (button === "open_platform") {
            text = GameRunner.openEmulator(g.id)
            if (text !== "") {
                emulatorMessage(text, g.id)
            } else {
                openingId = g.id
                openingTimer.restart()
            }
            return
        }
        if (button === "settings") {
            editRequested()
            return
        }
        if (button === "gallery") {
            galleryRequested(g.id)
            return
        }
        if (button === "folder")
            text = GameRunner.openFolder(g.id)
        if (button === "save_folder")
            text = GameRunner.openSaveFolder(g.id)
        if (text !== "")
            message(text)
    }

    GameFields {
        id: gameFields
        game: root.game
        updateAction: launchButton.updateAction
    }

    readonly property var shownChips: General.gamePageChips.filter(name => gameFields.shown([name]).length > 0)

    // Another game's chips replace these at once: the switch has its own motion. A chip turned on or off,
    // or a Steam chip coming or going on this game, pops in or out.
    KeyedListModel {
        id: chipModel
        leaveSpeed: "fast"
    }
    function showChips() {
        chipModel.scope = game.id
        chipModel.source = shownChips.map(name => ({ key: name }))
    }
    onShownChipsChanged: showChips()
    Component.onCompleted: { showChips(); gameFields.loadLastBackup() }

    SteamPendingMenu {
        id: pendingMenu
        onMessage: text => root.message(text)
        onUndoRequested: root.steamUndoRequested()
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(Theme.details.messageWidth, parent.width - Theme.details.marginLeft - Theme.details.marginRight)
        visible: !root.hasGame && !Library.loading
        // Fades in; gone at once, so it never lingers over a game.
        opacity: visible ? 1 : 0
        spacing: Theme.layout.spacing

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Library.gameCount === 0 ? qsTr("Your library is empty")
                : root.allCollapsed ? qsTr("All groups are collapsed")
                : Library.search !== "" ? qsTr("No games match \"%1\"").arg(Library.search)
                : Library.filtersActive ? qsTr("No games match the filters")
                : Library.notInstalledHidden > 0 ? qsTr("No installed games")
                : qsTr("No games match")
            color: Theme.palette.textMuted
            font.pixelSize: Theme.font.headingSize
        }
        Label {
            visible: Library.gameCount === 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: qsTr("Add a ROM file, or set up your platforms' ROM folders in <b>Settings → Platforms</b> "
                       + "to find all your games.")
            color: Theme.palette.textMuted
        }
        SettingsButton {
            visible: Library.gameCount === 0
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Add a game")
            iconPath: Icons.add
            primary: true
            onClicked: root.addRequested()
        }
        Label {
            visible: Library.gameCount > 0 && Library.notInstalledHidden > 0 && !root.allCollapsed
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("%n game(s) that aren't installed would match.", "", Library.notInstalledHidden)
            color: Theme.palette.textMuted
        }
        RowLayout {
            visible: Library.gameCount > 0 && !root.allCollapsed && (Library.filtersActive || Library.notInstalledHidden > 0)
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.layout.spacing

            SettingsButton {
                visible: Library.filtersActive
                text: qsTr("Clear filters")
                primary: true
                onClicked: Library.clearFilters()
            }
            SettingsButton {
                visible: Library.notInstalledHidden > 0
                text: qsTr("Show not installed")
                onClicked: Library.installedOnly = false
            }
        }
    }

    NoticeBar {
        id: readOnlyNotice
        visible: Library.readOnly
        opacity: visible ? 1 : 0
        z: 1
        x: root.noticeX
        y: Theme.layout.margin
        width: root.noticeWidth
        height: Theme.layout.toolbarHeight
        text: Library.readOnlyNotice

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }
    }

    // After an Import, below that one when both show. Loaded after the first frame: Backups is
    // made on first use.
    Loader {
        id: importNotice
        active: root.startedUp
        z: 1
        x: root.noticeX
        y: Theme.layout.margin + (Library.readOnly ? Theme.layout.toolbarHeight + Theme.layout.spacing : 0)
        width: root.noticeWidth
        height: Theme.layout.toolbarHeight
        source: "ImportNotice.qml"
    }
    Connections {
        target: importNotice.item
        function onMessage(text) { root.message(text) }
    }

    Flickable {
        id: flick

        anchors.fill: parent
        visible: root.hasGame
        clip: true
        contentWidth: width
        contentHeight: content.y + content.implicitHeight + Theme.details.marginBottom
            + (downloading ? (collapsed ? Theme.downloadBar.pillHeight : Theme.downloadBar.height) + Theme.details.spacing : 0)
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        MouseArea {
            width: flick.width
            height: Math.max(flick.contentHeight, flick.height)
            onClicked: root.clicked()
        }

        // While adjusting the logo, a click anywhere off the hero finishes, and does nothing else,
        // like a click off a popup: Launch or a tag under it would be a surprise.
        MouseArea {
            z: 1
            // Hidden, not just disabled: Qt took the pointer's cursor from this MouseArea, over every
            // hand cursor on the page. While it shows, it takes the adjuster's cursor over the logo as well.
            visible: hero.adjusting
            cursorShape: hero.adjusterCursor
            width: flick.width
            height: Math.max(flick.contentHeight, flick.height)
            onPressed: mouse => {
                const p = mapToItem(hero, mouse.x, mouse.y)
                mouse.accepted = !hero.contains(p)
            }
            onClicked: hero.finishAdjust()
        }

        ColumnLayout {
            id: content

            x: Theme.details.marginLeft + Theme.details.padding
            y: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.details.marginTop
            // One expression, not root.marginRight: a binding on it would see compact's old value for a
            // moment, and the hero would load its first game at that width.
            width: root.width - Theme.details.marginLeft - 2 * Theme.details.padding
                   - (root.compactAt(root.width) ? Theme.details.compactMarginRight : Theme.details.marginRight)
            spacing: Theme.details.spacing

            Hero {
                id: hero
                readonly property real artHeight: Math.min(content.width / Theme.hero.aspect, Theme.hero.maxHeight)
                Layout.fillWidth: true
                Layout.preferredHeight: logoOnly ? Theme.hero.logoOnlyHeight : artHeight
                game: root.heroGame
                logoOnly: artHeight < Theme.hero.minHeight
                onMessage: text => root.message(text)
            }

            RowLayout {
                visible: root.game.hidden ?? false
                Layout.fillWidth: true
                spacing: Theme.details.actionSpacing

                Icon {
                    path: Icons.hidden
                    size: Theme.iconButton.iconSize
                    color: Theme.details.descriptionColor
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Hidden. It's listed because hidden games are shown.")
                    color: Theme.details.descriptionColor
                }
                SettingsButton {
                    text: qsTr("Unhide")
                    onClicked: {
                        const error = Library.unhideGame(root.game.id)
                        if (error !== "")
                            root.message(error)
                    }
                }
            }

            // The action row. When it all fits: Launch, the stats, then the buttons at the right
            // end. Otherwise Launch with the buttons beside it (or under it, when even they don't
            // fit), then the stats. Measured, not details.compact_width: the chips' widths vary.
            GridLayout {
                id: actionRow

                readonly property bool oneLine: content.width >= launchButton.implicitWidth + statsFlow.lineWidth
                    + buttons.implicitWidth + 2 * columnSpacing
                readonly property bool buttonsBeside: oneLine
                    || content.width >= Theme.launchButton.height * 2 + columnSpacing + buttons.implicitWidth
                // Off one line, Launch takes the spare width only while it has to shrink; otherwise the
                // buttons' column does, so the chips below, spanning both columns, get the whole row.
                readonly property bool launchShrinks: !buttonsBeside
                    || content.width < launchButton.implicitWidth + columnSpacing + buttons.implicitWidth

                Layout.fillWidth: true
                // The grid's maximum is the sum of its columns' (Launch and the buttons are capped), which
                // would shrink the row, and the chips under them, to that.
                Layout.maximumWidth: Number.POSITIVE_INFINITY
                columnSpacing: Theme.details.actionSpacing
                rowSpacing: Theme.details.actionSpacing

                LaunchButton {
                    id: launchButton
                    Layout.row: 0
                    Layout.column: 0
                    Layout.fillWidth: !actionRow.oneLine && actionRow.launchShrinks
                    Layout.maximumWidth: implicitWidth
                    Layout.minimumWidth: !actionRow.oneLine ? Math.min(implicitWidth, Theme.launchButton.height * 2) : implicitWidth
                    installed: root.game.installed ?? true
                    updateState: root.game.steamUpdate ?? "none"
                    rom: root.game.rom ?? false
                    gameNotFound: root.game.gameNotFound ?? false
                    locatable: root.game.platform === "proton" || root.game.platform === "linux"
                    installable: root.launcher !== "" && !locatable
                    running: GameRunner.running.includes(root.game.id ?? "")
                    emulatorName: root.emulatorName
                    emulatorStep: root.emulatorStep
                    tip: root.emulatorName !== "" ? qsTr("Install %1; Lattis finds it afterwards").arg(root.emulatorName)
                       : (root.game.launchTip ?? "") !== "" ? Actions.tip(root.game.launchTip, Actions.keys.launch) : ""
                    onClicked: {
                        if (root.emulatorName === "")
                            root.launchRequested()
                        else
                            root.openInstallDialog()
                    }
                    onStopRequested: root.stopRequested()
                }

                Flow {
                    id: statsFlow
                    // The chips on one line, for the one-line row (a Flow's own implicit width
                    // follows the width it was last given).
                    readonly property real lineWidth: {
                        let w = 0
                        for (let i = 0; i < chips.count; ++i)
                            w += chips.itemAt(i)?.implicitWidth ?? 0
                        return w + Math.max(0, chips.count - 1) * spacing
                    }

                    visible: chipModel.count > 0
                    Layout.row: !actionRow.oneLine ? (actionRow.buttonsBeside ? 1 : 2) : 0
                    Layout.column: !actionRow.oneLine ? 0 : 1
                    Layout.columnSpan: !actionRow.oneLine && actionRow.buttonsBeside ? 3 : 1
                    Layout.fillWidth: !actionRow.oneLine
                    Layout.preferredWidth: !actionRow.oneLine ? 0 : lineWidth
                    Layout.minimumWidth: !actionRow.oneLine ? 0 : lineWidth
                    spacing: Theme.details.actionSpacing

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: point => root.openPageMenu("chips", statsFlow, point.position)
                    }

                    move: Transition {
                        enabled: Motion.on && !chipModel.rebuilt
                        MotionNumber { properties: "x,y"; speed: "normal" }
                    }

                    Repeater {
                        id: chips
                        model: chipModel

                        Loader {
                            id: chip
                            required property var model
                            readonly property string name: model.key
                            readonly property bool leaving: model.leaving

                            sourceComponent: fieldChip

                            PopLeave { target: chip; leaving: chip.leaving }

                            // Not the Flow's add transition: a chip's width settles a moment after the Flow
                            // first places it, and placing it again cut that transition off, leaving it unseen.
                            Component.onCompleted: {
                                if (Motion.on && !chipModel.rebuilt)
                                    enterAnimation.start()
                            }
                            ParallelAnimation {
                                id: enterAnimation
                                MotionNumber { target: chip; property: "opacity"; from: 0; to: 1 }
                                MotionPop { target: chip; property: "scale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
                            }

                            Component {
                                id: fieldChip
                                StatChip {
                                    id: entry
                                    readonly property var field: gameFields.all[chip.name]
                                    readonly property bool opensHistory: field.history ?? false
                                    readonly property bool opensUrl: (field.action ?? "") === "url"
                                    attention: field.attention ?? false
                                    attentionColor: field.attentionColor
                                    iconTint: field.iconTint
                                    icon: General.chipsWithoutIcon.includes(chip.name) ? "" : field.icon
                                    label: field.label
                                    value: field.value
                                    clickable: !chip.leaving && (opensHistory || opensUrl || (field.filterPlatform ?? "") !== "")
                                    tip: clickable ? field.tip ?? "" : ""
                                    tipStars: field.stars ?? -1
                                    ToolTip.visible: chipHover.hovered && (field.hoverTip ?? "") !== ""
                                    ToolTip.text: field.hoverTip ?? ""
                                    ToolTip.delay: 500
                                    onClicked: {
                                        if (opensHistory)
                                            root.historyRequested()
                                        else if (opensUrl)
                                            Qt.openUrlExternally(field.url)
                                        else
                                            Library.platformFilter = field.filterPlatform
                                    }

                                    HoverHandler { id: chipHover }

                                    SettingsButton {
                                        id: pendingButton
                                        visible: (entry.field.action ?? "") === "pending"
                                        implicitHeight: Theme.statChip.buttonHeight
                                        flat: true
                                        leftPadding: text === "" ? 0 : 14
                                        rightPadding: leftPadding
                                        implicitWidth: text === "" ? implicitHeight : contentItem.implicitWidth + leftPadding + rightPadding
                                        text: GameRunner.closingSteam ? qsTr("Closing Steam") : ""
                                        iconPath: GameRunner.closingSteam ? "" : Icons.chevronRight
                                        iconRotation: pendingMenu.showing ? -90 : 90
                                        enabled: !GameRunner.closingSteam
                                        tip: entry.field.tip ?? ""
                                        onClicked: pendingMenu.popup(pendingButton, 0, pendingButton.height)
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    visible: actionRow.buttonsBeside
                    Layout.row: 0
                    Layout.column: 2
                    Layout.fillWidth: actionRow.oneLine || !actionRow.launchShrinks
                    Layout.fillHeight: true

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: point => root.openPageMenu("chips", parent, point.position)
                    }
                }

                RowLayout {
                    id: buttons
                    Layout.row: actionRow.buttonsBeside ? 0 : 1
                    Layout.column: actionRow.oneLine ? 3 : actionRow.buttonsBeside ? 1 : 0
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    spacing: Theme.details.buttonSpacing

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: point => root.openPageMenu("buttons", buttons, point.position)
                    }

                    Repeater {
                        id: buttonRepeater
                        model: General.gamePageButtons.filter(b => root.actions[b] !== undefined
                                                              && (b !== "open_platform" || root.game.platform !== "linux")
                                                              && (b !== "save_folder" || root.saveFolder !== "")
                                                              && (b !== "mod_manager" || root.managers.length > 0))

                        IconButton {
                            required property string modelData
                            icon: root.actions[modelData].icon
                            iconColor: root.actions[modelData].color
                            tip: root.actions[modelData].tip
                            badge: root.actions[modelData].badge
                            busy: modelData === "open_platform" && root.openingId !== "" && root.openingId === root.game.id
                            onClicked: modelData === "menu" ? root.menuRequested(this) : root.activate(modelData)
                        }
                    }
                }
            }

            GridLayout {
                id: body
                Layout.fillWidth: true
                columns: root.compact ? 1 : 2
                columnSpacing: Theme.details.columnSpacing
                rowSpacing: Theme.details.spacing

                TextBlock {
                    id: textColumn
                    readonly property real exitOpacity: 1 - root.textExit
                    readonly property real flowOpacity: root.textPending ? exitOpacity : root.textIn
                    readonly property real flowShift: root.textPending ? -root.riseDistance * root.textExit
                                                                       : root.riseDistance * (1 - root.textIn)
                    contentOpacity: Theme.details.animateDescription ? flowOpacity : 1
                    contentShift: Theme.details.animateDescription ? flowShift : 0
                    descriptionHeadingOpacity: root.textPending ? (root.showsDescription(root.game) ? 1 : exitOpacity)
                        : root.textLeaving && leavingText.descriptionHeading.visible ? 1 : root.textIn
                    tagsOpacity: !Theme.details.animateTags ? 1 : root.tagsMorphing || root.tagsStay ? 1 : flowOpacity
                    tagsShift: !Theme.details.animateTags ? 0 : root.tagsMorphing ? (leavingText.tagsHeading.y - tagsHeading.y) * (1 - root.textIn)
                             : root.tagsStay ? 0 : flowShift
                    chipsHidden: root.tagsMorphing
                    Layout.row: 0
                    Layout.column: 0
                    Layout.fillWidth: true
                    // A layout's maximum width is its children's: with every section folded, none
                    // fills, and the info panel would slide over to the left.
                    Layout.maximumWidth: Number.POSITIVE_INFINITY
                    Layout.alignment: Qt.AlignTop
                    game: root.textGame
                    maxLines: needsCut && !expanded ? shownLines : 2147483647
                    cut: needsCut
                    expanded: root.descriptionExpandedFor === (root.textGame.id ?? "")
                    onMoreClicked: root.descriptionExpandedFor = expanded ? "" : root.textGame.id
                    onMediaRequested: (item, list) => root.showMedia(item, list)
                    onGalleryRequested: id => root.galleryRequested(id)
                    onClipMenuRequested: (clip, tile, position) => root.openClipMenu(clip, tile, position)
                    onFailed: text => root.message(text)

                    // details.fit_description: how many lines of the description fit the window.
                    // Worked out from the whole text (fullText) and the heights of everything else,
                    // never from the description as shown, which the result changes.
                    readonly property real room: {
                        let others = 0
                        for (const item of [descriptionHeading, tagsHeading, tagsFlow]) {
                            if (item.visible)
                                others += item.implicitHeight + item.Layout.topMargin + spacing
                        }
                        return flick.height - content.y - Theme.details.marginBottom - body.y - others
                            - (root.compact && side.visible ? body.rowSpacing + side.implicitHeight : 0)
                    }
                    readonly property real lineHeightPx: fullText.lineCount > 0 ? fullText.contentHeight / fullText.lineCount : 1
                    readonly property int fittingLines: Math.max(Theme.details.descriptionMinLines,
                        Math.floor((room - moreLink.implicitHeight - spacing) / lineHeightPx))
                    // One fewer when the last line kept is the blank one between two paragraphs,
                    // so "…" doesn't sit on a line of its own.
                    readonly property int shownLines: fittingLines > 1 && fullText.blankLines.includes(fittingLines - 1)
                                                      ? fittingLines - 1 : fittingLines
                    readonly property bool needsCut: Theme.details.fitDescription && fullText.contentHeight > room
                                                     && fullText.lineCount > fittingLines

                    Text {
                        id: fullText
                        visible: false
                        width: textColumn.description.width
                        text: textColumn.description.text
                        font: textColumn.description.font
                        wrapMode: textColumn.description.wrapMode
                        lineHeight: textColumn.description.lineHeight

                        property var blankLines: []
                        onLineLaidOut: line => {
                            const blank = line.number === 0 ? [] : blankLines.slice()
                            if (line.implicitWidth === 0)
                                blank.push(line.number)
                            blankLines = blank
                        }
                    }
                }
                GridLayout {
                    id: side
                    columns: 1
                    rowSpacing: Theme.achievements.gap
                    columnSpacing: 0
                    function rowOf(name) {
                        return Math.max(0, General.gamePagePanels.indexOf(name))
                    }
                    Layout.row: root.compact ? 1 : 0
                    Layout.column: root.compact ? 0 : 1
                    Layout.fillWidth: root.compact
                    Layout.preferredWidth: root.compact ? -1 : Theme.infoPanel.width
                    Layout.alignment: Qt.AlignTop
                    readonly property bool infoShown: info.rows.length > 0 && General.gamePagePanels.includes("info")
                    visible: infoShown || friends.shown || achievements.shown

                    InfoPanel {
                        id: info
                        visible: side.infoShown
                        Layout.row: side.rowOf("info")
                        Layout.fillWidth: true
                        fields: gameFields
                        onHistoryRequested: root.historyRequested()
                        onMessage: text => root.message(text)
                        onUndoRequested: root.steamUndoRequested()

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: point => root.openPageMenu("info_rows", info, point.position)
                        }
                    }
                    FriendsPanel {
                        id: friends
                        Layout.row: side.rowOf("friends")
                        Layout.fillWidth: true
                        game: root.game
                        onMenuRequested: point => root.openPageMenu("friends", friends, point)
                    }
                    AchievementsPanel {
                        id: achievements
                        Layout.row: side.rowOf("achievements")
                        Layout.fillWidth: true
                        game: root.game
                    }
                }
            }

        }

        Item {
            x: content.x + body.x
            y: content.y + body.y
            visible: root.textLeaving

            TextBlock {
                id: leavingText
                liveMedia: false
                readonly property real flowOpacity: 1 - root.leavingExit
                contentOpacity: Theme.details.animateDescription ? flowOpacity : 0
                readonly property real flowShift: -root.riseDistance * root.leavingExit
                contentShift: Theme.details.animateDescription ? flowShift : 0
                descriptionHeadingOpacity: textColumn.descriptionHeading.visible ? 0 : 1 - root.leavingExit
                tagsOpacity: Theme.details.animateTags && !root.tagsMorphing ? flowOpacity : 0
                tagsShift: Theme.details.animateTags ? flowShift : 0
                enabled: false
            }

            // Each old chip turns into the new one in its place: it moves, resizes and recolors, and its
            // name crossfades in place, the two opacities always adding up to 1 so the chip never goes empty
            // (faded one after the other, or with the old gone early, there was a moment with no name, which read
            // as blinking).
            // One without a partner fades out or in, moving with the heading.
            Repeater {
                model: root.tagsMorphing ? Math.max(leavingText.tagRepeater.count, textColumn.tagRepeater.count) : 0

                Rectangle {
                    id: morphChip
                    required property int index
                    readonly property Item from: index < leavingText.tagRepeater.count ? leavingText.tagRepeater.itemAt(index) : null
                    readonly property Item to: index < textColumn.tagRepeater.count ? textColumn.tagRepeater.itemAt(index) : null
                    readonly property real p: root.textIn
                    readonly property real glide: textColumn.y + textColumn.tagsHeading.y
                                                  - leavingText.y - leavingText.tagsHeading.y
                    readonly property real toX: to ? textColumn.x + textColumn.tagsFlow.x + to.x : fromX
                    readonly property real toY: to ? textColumn.y + textColumn.tagsFlow.y + to.y : fromY + glide
                    readonly property real fromX: from ? leavingText.x + leavingText.tagsFlow.x + from.x : toX
                    readonly property real fromY: from ? leavingText.y + leavingText.tagsFlow.y + from.y : toY - glide
                    readonly property color fromColor: from ? from.color : to.color
                    readonly property color toColor: to ? to.color : from.color

                    x: fromX + (toX - fromX) * p
                    y: fromY + (toY - fromY) * p
                    width: from && to ? from.width + (to.width - from.width) * p : (from ?? to).width
                    height: Theme.tags.height
                    z: from && to ? 1 : 0
                    opacity: from && to ? 1 : from ? Math.max(0, 1 - 2 * p) : Math.max(0, 2 * p - 1)
                    radius: Theme.tags.radius
                    color: Qt.rgba(fromColor.r + (toColor.r - fromColor.r) * p, fromColor.g + (toColor.g - fromColor.g) * p,
                                   fromColor.b + (toColor.b - fromColor.b) * p, fromColor.a + (toColor.a - fromColor.a) * p)
                    border.color: Theme.tags.border
                    border.width: Theme.tags.borderWidth
                    clip: true

                    Label {
                        anchors.centerIn: parent
                        visible: morphChip.from !== null
                        opacity: morphChip.to ? 1 - morphChip.p : 1
                        text: morphChip.from?.name ?? ""
                        color: morphChip.from ? morphChip.from.fillText : Theme.tags.text
                        font.pixelSize: Theme.tags.fontSize
                        font.weight: morphChip.from ? morphChip.from.nameWeight : Font.Normal
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: morphChip.to !== null
                        opacity: morphChip.from ? morphChip.p : 1
                        text: morphChip.to?.name ?? ""
                        color: morphChip.to ? morphChip.to.fillText : Theme.tags.text
                        font.pixelSize: Theme.tags.fontSize
                        font.weight: morphChip.to ? morphChip.to.nameWeight : Font.Normal
                    }
                }
            }
        }
    }

    Item {
        id: dock
        anchors.fill: parent
        clip: true
        z: 1

        readonly property real pillWidth: downloadPill.item ? downloadPill.item.implicitWidth : 0
        readonly property real morphWidth: content.width + Theme.downloadBar.overhang * (1 - root.slide) + (pillWidth - content.width) * root.slide
        readonly property real morphHeight: Theme.downloadBar.height + (Theme.downloadBar.pillHeight - Theme.downloadBar.height) * root.slide

        readonly property real rowShift: downloadBar.item
            ? Theme.downloadBar.height - downloadBar.item.rowCenter - Theme.downloadBar.pillHeight / 2 : 0

        // The pill sits opaque under the bar and the bar fades away over it, so the card itself never
        // dips in opacity mid-morph.
        Loader {
            id: downloadPill
            active: root.downloading
            visible: root.slide > 0
            x: content.x + content.width - width
            y: root.height - Theme.details.marginBottom - height + dock.rowShift * (1 - root.slide)
            width: dock.morphWidth
            height: dock.morphHeight
            source: "DownloadPill.qml"
        }
        Loader {
            id: downloadBar
            active: root.downloading
            visible: opacity > 0
            opacity: Math.max(0, 1 - root.slide * 1.6)
            x: content.x + content.width - width
            y: root.height - Theme.details.marginBottom - height + dock.rowShift * (1 - root.slide)
            width: dock.morphWidth
            height: dock.morphHeight
            source: "DownloadBar.qml"
        }
    }
    // Whether the pointer was last seen on the chevron, from whichever card is at rest, so the
    // chevron keeps its hover color while the morph hands it from one card to the other.
    property bool chevronHot: false
    Connections {
        target: downloadBar.item
        function onChevronHoveredChanged() {
            if (root.slide <= 0)
                root.chevronHot = downloadBar.item.chevronHovered
        }
    }
    Connections {
        target: downloadPill.item
        function onHoveredChanged() {
            if (root.slide >= 1)
                root.chevronHot = downloadPill.item.hovered
        }
    }
    Binding {
        target: downloadBar.item
        property: "carriedHover"
        value: root.chevronHot && root.slide > 0
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "carriedHover"
        value: root.chevronHot && root.slide < 1
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "morphing"
        value: root.slide > 0
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "slide"
        value: root.slide
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "fullWidth"
        value: content.width
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "iconDrop"
        value: downloadBar.item ? Theme.downloadBar.height / 2 - downloadBar.item.rowCenter : 0
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "download"
        value: Library.download
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "rowFromBottom"
        value: Theme.downloadBar.pillHeight / 2 + dock.rowShift * (1 - root.slide)
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "rowFromBottom"
        value: Theme.downloadBar.pillHeight / 2 + dock.rowShift * (1 - root.slide)
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadBar.item
        property: "tooltips"
        value: !root.collapsed
        when: downloadBar.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "tooltips"
        value: root.slide >= 1
        when: downloadPill.item !== null
    }
    Binding {
        target: downloadPill.item
        property: "download"
        value: Library.download
        when: downloadPill.item !== null
    }
    Connections {
        target: downloadBar.item
        function onMessage(text) { root.message(text) }
        function onCollapse() { General.setDownloadBarCollapsed(true) }
    }
    Connections {
        target: downloadPill.item
        function onExpand() { General.setDownloadBarCollapsed(false) }
    }

    // Drag-to-move zone: the strip above the hero. Off over a notice bar: taking over from
    // anything, it took the press from the bar's buttons and cancelled their clicks.
    Item {
        z: 2
        y: 0
        width: parent.width - root.dragReserve
        height: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing

        DragHandler {
            target: null
            enabled: !readOnlyNotice.hovered && !(importNotice.item?.hovered ?? false) && !(root.Window.window?.toolbarHovered ?? false)
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
    }
}
