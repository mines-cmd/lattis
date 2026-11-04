import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window
    property bool toolbarHovered: false

    function launch(game) {
        if (!game.installed && (game.platform === "proton" || game.platform === "linux")) {
            locateProgram(game)
            return
        }
        if (!game.installed && game.platform !== "steam")
            return
        // Edits waiting for Steam to close: ask whether to restart it. With Steam closed,
        // they're written first without asking.
        if ((game.platform === "steam" || game.platform === "proton") && game.installed && game.pendingSteamEdits
                && !GameRunner.running.includes(game.id) && GameRunner.steamRunning()) {
            steamPrompt.active = true
            steamPrompt.item.ask(game)
            return
        }
        const error = GameRunner.launch(game.id)
        if (error !== "")
            showEmulatorMessage(error, game.id)
    }

    function openModManager(id) {
        const missing = GameRunner.missingPrefixGame(id)
        if (missing !== "") {
            toast.show(qsTr("Run %1 once first, so Proton creates its prefix").arg(Library.gameTitle(missing)),
                       { text: qsTr("Launch"), run: () => {
                           const row = Library.rowOfGame(missing)
                           if (row >= 0) {
                               launch(Library.gameAt(row))
                               return
                           }
                           const error = GameRunner.launch(missing)
                           if (error !== "")
                               toast.show(error)
                       } })
            return
        }
        const error = GameRunner.launchModManager(id)
        if (error !== "")
            toast.show(error)
    }

    function addModManager(gameId) {
        modManagerDialog.active = true
        modManagerDialog.item.openFor(null, [gameId])
    }

    Loader {
        id: modManagerDialog
        active: false
        source: "settings/ModManagerDialog.qml"
    }
    Connections {
        target: modManagerDialog.item
        function onSaved(name) { toast.show(qsTr("%1 added").arg(name)) }
    }

    function askForceClose(game) {
        forceClosePrompt.active = true
        forceClosePrompt.item.ask(game)
    }

    function askSteamUndo() {
        steamUndo.active = true
        steamUndo.item.open()
    }

    function dragPlatformForScreenshot(spec) {
        const [id, offset] = spec.split(":")
        rail.dragForScreenshot(id, parseInt(offset))
    }

    function dragToolbarForScreenshot(spec) {
        const [name, offset] = spec.split(":")
        toolbar.dragForScreenshot(name, parseInt(offset))
    }

    function dragPlatformInSettings(spec) {
        const [id, offset] = spec.split(":")
        settingsWindow.dragPlatformForScreenshot(id, parseInt(offset))
    }

    function makeUndoableChange(kind) {
        const game = gameList.currentGame
        switch (kind) {
        case "hide": hideGame(game); break
        case "save": Library.saveGame(game.id, { description: "Changed for the screenshot" }); break
        case "delete_game": Library.removeGame(game.id, true); break
        case "delete_tag": Library.removeTag(game.tags[0].name); break
        case "move_platform": Library.movePlatform(Library.platforms[1].id, -1); break
        }
    }

    function openPlayHistory(game) {
        playHistory.active = true
        playHistory.item.openFor(game)
    }

    function playHistoryForScreenshot(id) {
        gameList.selectGame(id)
        openPlayHistory(gameList.currentGame)
    }

    property bool steamSyncAsked: false
    function steamSyncResult() {
        return SteamLibrary.message !== "" ? SteamLibrary.message
            : SteamLibrary.statusLine !== "" ? SteamLibrary.statusLine : qsTr("Steam library synced")
    }
    Connections {
        target: SteamLibrary
        function onChanged() {
            if (window.steamSyncAsked && !SteamLibrary.syncing) {
                window.steamSyncAsked = false
                toast.show(window.steamSyncResult())
            }
        }
    }

    property bool statsOpen: false
    // The views under the page stay drawn while it fades in and out.
    readonly property bool statsCovering: statsOpen && statsPage.opacity === 1

    // Switching between the pages keeps the old one drawn under the new one until the new one
    // covers it, so they crossfade instead of both fading through the game page.
    property string pageBelow: ""
    // Kept after the switch ends, so the old page fades out under the new one rather than over it.
    property string pageOnTop: ""

    function setStatsOpen(open, switching = false) {
        if (open === statsOpen)
            return
        if (open) {
            const below = galleryOpen ? "gallery" : ""
            setGalleryOpen(false, "", true)
            pageBelow = below
            pageOnTop = "stats"
            statsPage.active = true
            statsOpen = true
            statsPage.item.start()
            if (statsPage.opacity === 1)
                pageBelow = ""
        } else {
            statsOpen = false
            pageBelow = ""
            if (!switching)
                statsPage.item.leave()
            gameList.focusList()
        }
    }

    property bool galleryOpen: false

    // Creating the page on the first click froze the window for ~20 ms before it showed.
    Timer {
        running: true
        interval: 2000
        onTriggered: {
            galleryPage.active = true
            galleryPage.item.warmUp()
        }
    }
    readonly property bool galleryCovering: galleryOpen && galleryPage.opacity === 1
    readonly property bool pageCovering: statsCovering || galleryCovering

    function setGalleryOpen(open, gameId = "", switching = false) {
        if (open === galleryOpen)
            return
        if (open) {
            const below = statsOpen ? "stats" : ""
            setStatsOpen(false, true)
            pageBelow = below
            pageOnTop = "gallery"
            galleryPage.active = true
            galleryOpen = true
            galleryPage.item.start(gameId)
            if (galleryPage.opacity === 1)
                pageBelow = ""
        } else {
            galleryOpen = false
            pageBelow = ""
            if (!switching)
                galleryPage.item.leave()
            gameList.focusList()
        }
    }

    // "1" opens the page; "viewer" or "viewer/<n>" also opens the n-th item (the first), "menu" the first tile's menu; ";"-joined "filter:<section>=<keys>" and "panel" set filters and open the narrow window's panel.
    function galleryForScreenshot(spec) {
        setGalleryOpen(true)
        galleryPage.item.autoOpen = spec
    }

    function statsForScreenshot(spec) {
        setStatsOpen(true)
        const [period, suffix] = spec.split("/")
        const parts = period.split("-")
        if (parts.length === 2)
            statsPage.item.pick(parseInt(parts[0]), parseInt(parts[1]))
        else if (/^\d{4}$/.test(period))
            statsPage.item.pickYear(parseInt(period))
        if (suffix === "picker")
            statsPage.item.pickerPending = true
    }

    function selectGame(id) {
        gameList.selectGame(id)
    }

    function scrollList(pixelsPerSecond) {
        gameList.scrollThrough(pixelsPerSecond)
    }

    function selectAndLaunch(id) {
        gameList.selectGame(id)
        launch(Library.gameAt(Library.rowOfGame(id)))
    }

    Loader {
        id: steamPrompt
        active: false
        source: "views/SteamLaunchPrompt.qml"
    }
    Connections {
        target: steamPrompt.item
        function onMessage(text) { toast.show(text) }
        function onClosed() { gameList.focusList() }
    }

    Connections {
        target: Library
        function onUndoOffered(token, text) {
            if (!(window.settingsWindow?.changing ?? false))
                toast.showUndo(text, token)
        }
    }

    Loader {
        id: forceClosePrompt
        active: false
        source: "views/ForceClosePrompt.qml"
    }
    Connections {
        target: forceClosePrompt.item
        function onMessage(text) { toast.show(text) }
        function onClosed() { gameList.focusList() }
    }

    Loader {
        id: steamUndo
        active: false
        source: "views/SteamUndoPrompt.qml"
    }
    Connections {
        target: steamUndo.item
        function onMessage(text) { toast.show(text) }
        function onClosed() { gameList.focusList() }
    }

    Loader {
        id: playHistory
        active: false
        source: "views/PlayHistoryPopup.qml"
    }
    Connections {
        target: playHistory.item
        function onMessage(text) { toast.show(text) }
        function onUndoHandedOver(text, link, duration) { toast.show(text, link, duration) }
        function onClosed() { gameList.focusList() }
    }

    Loader {
        id: locatePicker
        property var game: ({})
        active: false
    }
    Connections {
        target: locatePicker.item
        function onPicked(file) {
            const error = Library.locateProgram(locatePicker.game.id, file)
            if (error !== "")
                toast.show(error)
        }
        function onClosed() { locatePicker.active = false }
    }
    // The dialog opens as it's made, so its title and filters go in with it.
    function locateProgram(game) {
        locatePicker.game = game
        locatePicker.setSource("views/RomFileDialog.qml", {
            title: qsTr("Locate the program of %1").arg(game.title ?? ""),
            nameFilters: game.platform === "linux"
                         ? [qsTr("Linux programs (*.sh *.x86_64 *.x86 *.AppImage *.appimage *.bin)"), qsTr("All files (*)")]
                         : [qsTr("Windows programs (*.exe)"), qsTr("All files (*)")]
        })
        locatePicker.active = true
    }

    function showEmulatorMessage(text, gameId) {
        const emulator = GameRunner.missingEmulator(gameId)
        toast.show(text, emulator === "" ? null
                   : { text: qsTr("Emulators"), run: () => openSettingsAt("emulators", emulator) })
    }

    // [launch] while_running = "minimize": out of the way while a game runs, back when the
    // last one exits.
    property bool minimizedForGame: false

    // Set by main.cpp once the first frame is shown.
    property bool startedUp: false

    onStartedUpChanged: {
        if (!startedUp)
            return
        if (Library.setupPending)
            openSetup()
        else if (AppUpdate.whatsNewVersion !== "")
            showWhatsNew()
    }

    // The marker goes at once so the note shows once; the notes stay in the link.
    function showWhatsNew() {
        const version = AppUpdate.whatsNewVersion
        const notes = AppUpdate.whatsNewNotes
        AppUpdate.dismissWhatsNew()
        updatedVersion = version
        updatedNotes = notes
        toast.show(qsTr("Updated to %1").arg(version), {
            text: qsTr("What's new"),
            run: () => openWhatsNew()
        }, 10000)
    }

    property string updatedVersion: ""
    property string updatedNotes: ""

    function openWhatsNew() {
        whatsNewDialog.active = true
        whatsNewDialog.item.version = updatedVersion
        whatsNewDialog.item.notes = updatedNotes
        whatsNewDialog.item.open()
    }

    function openUpdate() {
        updateDialog.active = true
        updateDialog.item.open()
    }

    function openSetup(step) {
        setupGuide.reopened = !Library.setupPending
        setupGuide.active = true
        setupGuide.item.goTo(step ?? 0)
        setupGuide.item.forceActiveFocus()
        setupFocus.restart()
    }

    // The app menu hands focus back to what it took it from once its exit animation ends, after the guide has asked for it.
    Timer {
        id: setupFocus
        interval: 400
        onTriggered: setupGuide.item?.forceActiveFocus()
    }

    function openSetupAt(n) {
        const step = parseInt(n) - 1
        const pending = Library.setupPending
        openSetup(step)
        if (pending && step === setupGuide.item.doneStep)
            setupGuide.item.finish()
    }

    function openFindRomFoldersForScreenshot(path) {
        setupGuide.item?.page?.openFindDialogForScreenshot?.(path)
    }

    function expandSetupRowForScreenshot(platformId) {
        setupGuide.item?.page?.expandForScreenshot?.(platformId)
    }

    function testMetadataKeyForScreenshot() {
        setupGuide.item?.page?.testForScreenshot?.()
    }

    Connections {
        target: GameRunner
        function onMessage(text) { toast.show(text) }
        function onSessionStarted(id) {
            if (id.startsWith("mod-manager:"))
                return
            if (GameRunner.minimizeWhileRunning &&window.visibility !== Window.Minimized) {
                window.minimizedForGame = true
                window.showMinimized()
            }
        }
        function onSessionEnded() {
            if (window.minimizedForGame && GameRunner.running.length === GameRunner.runningManagers.length) {
                window.minimizedForGame = false
                window.showNormal()
            }
        }
    }

    Connections {
        target: ScanLookup
        function onMessage(text, review) {
            // The setup guide's Done step says it already (on the first start: reopened, it has no summary).
            if (setupGuide.active && !setupGuide.reopened)
                return
            toast.show(text, review ? { text: qsTr("Review"), run: () => openBulkLookup() } : null)
        }
    }

    function editGame(id, tab) {
        editorLoader.active = true
        editorLoader.item.openFor(id, tab)
    }

    // The screenshot hook's "<game id>[/general|images|launch|saves]".
    function selectAndEdit(spec) {
        const [id, tab] = spec.split("/")
        gameList.selectGame(id)
        editGame(id, Math.max(0, ["general", "images", "launch", "saves"].indexOf(tab)))
    }

    function addGame() {
        romPicker.active = true
    }
    function addGameFile(file) {
        editorLoader.active = true
        editorLoader.item.openForFile(file)
    }

    function lookUpInEditor() {
        editorLoader.item?.lookUp()
    }

    function uninstallInEditor() {
        editorLoader.item?.uninstall()
    }

    function mediaFoldersInEditor() {
        editorLoader.item?.openMediaFolders()
    }

    function installDialogForScreenshot() {
        details.openInstallDialog()
    }

    function logoForScreenshot(mode) {
        details.logoForScreenshot(mode)
    }

    function gameMenuForScreenshot(spec) {
        const [id, submenu] = spec.split("/")
        if (id === "page")
            details.openMenu()
        else
            gameList.openMenu(Library.rowOfGame(id))
        if (submenu !== undefined)
            gameList.openMenuSubmenu(submenu)
    }

    function gamePageMenuForScreenshot(spec) {
        const [place, submenu] = spec.split("/")
        details.pageMenuForScreenshot(place, submenu)
    }

    function clipForScreenshot(spec) {
        details.clipForScreenshot(spec)
    }

    function selectGamesForScreenshot(ids) {
        gameList.selectGames(ids.split(","))
    }

    function platformMenuForScreenshot(id) {
        rail.openPlatformMenuForScreenshot(id)
    }

    function railMenuForScreenshot() {
        rail.openRailMenu(rail.height / 2)
    }

    function toolbarMenuForScreenshot() {
        toolbar.openMenu(toolbar.width / 2, toolbar.height)
    }

    function appMenuForScreenshot(mode) {
        rail.openMenu()
        if (mode !== "menu")
            rail.openSubmenuForScreenshot(mode)
    }

    function detailedLookupInEditor(name) {
        editorLoader.item?.openDetailedLookup(name === "1" ? "" : name, name !== "1")
    }

    function useDetailedLookupInEditor() {
        editorLoader.item?.useDetailedLookup()
    }

    function browseArtInEditor(kind) {
        editorLoader.item?.browseArt(kind)
    }

    Loader {
        id: editorLoader
        active: false
        source: "views/GameEditor.qml"
    }
    Connections {
        target: editorLoader.item
        function onClosed() { gameList.focusList() }
        function onSaved(id) { gameList.selectGame(id) }
        function onSelectRequested(id) { gameList.selectGame(id) }
        function onRemoved(next) { gameList.selectGame(next) }
        function onMessage(text) { toast.show(text) }
        function onAddModManagerRequested(gameId) { addModManager(gameId) }
        function onShortcutFound(text) {
            toast.show(text, { text: qsTr("Add existing shortcuts"),
                               run: () => openSettingsAt("steam", "shortcuts") })
        }
    }

    Loader {
        id: romPicker
        active: false
        source: "views/RomFileDialog.qml"
    }
    Connections {
        target: romPicker.item
        function onPicked(file) { window.addGameFile(file) }
        function onClosed() { romPicker.active = false }
    }

    property var settingsWindow: null
    property var bulkLookupWindow: null

    function openBulkLookup(from, forTags) {
        if (bulkLookupWindow === null)
            bulkLookupWindow = createWindow("views/BulkLookup.qml")
        bulkLookupWindow.returnTo = from ?? window
        if (forTags)
            bulkLookupWindow.suggestTagsForEveryGame()
        else
            bulkLookupWindow.show()
    }
    function openBulkLookupForScreenshot(mode, replace, size) {
        openBulkLookup(window, mode.startsWith("tags"))
        if (replace)
            bulkLookupWindow.setReplaceExisting(true)
        const wh = size.split("x")
        if (wh.length === 2) {
            bulkLookupWindow.width = Number(wh[0])
            bulkLookupWindow.height = Number(wh[1])
        }
        if (mode.endsWith("look_up"))
            bulkLookupWindow.lookUp()
    }

    function bulkFilterForScreenshot(state) {
        bulkLookupWindow?.filterResults(state)
    }

    function bulkTagsForScreenshot(search) {
        bulkLookupWindow?.openTags(search)
    }

    // Created on first use, not at startup, to keep the main window's first frame fast.
    function openSettings() {
        if (settingsWindow === null) {
            settingsWindow = createWindow("settings/SettingsWindow.qml")
            settingsWindow.game = Qt.binding(() => gameList.currentGame)
            settingsWindow.lookUpMissing.connect(() => openBulkLookup(settingsWindow))
            settingsWindow.suggestTagsFromIgdb.connect(() => openBulkLookup(settingsWindow, true))
            settingsWindow.openUpdate.connect(() => {
                raise()
                requestActivate()
                openUpdate()
            })
            settingsWindow.runSetupGuide.connect(() => {
                settingsWindow.visible = false
                raise()
                requestActivate()
                openSetup()
            })
        }
        settingsWindow.visible = true
        settingsWindow.raise()
        settingsWindow.requestActivate()
    }

    function openSettingsAt(category, item) {
        openSettings()
        settingsWindow.showCategory(category, item)
    }

    property int windowedVisibility: General.startedFullscreen
        ? (WindowState.maximized("main") ? Window.Maximized : Window.Windowed)
        : Window.Windowed

    function toggleFullscreen() {
        if (visibility === Window.FullScreen) {
            visibility = windowedVisibility
        } else {
            windowedVisibility = visibility
            visibility = Window.FullScreen
        }
    }

    function run(name) {
        if (name.startsWith("show_platform_")) {
            const platform = Library.platforms[parseInt(name.slice("show_platform_".length)) - 1]
            if (platform !== undefined)
                Library.platformFilter = platform.id
            return
        }
        switch (name) {
        case "show_all":
            Library.platformFilter = ""
            break
        case "next_view":
        case "previous_view": {
            const views = [""].concat(Library.platforms.map(p => p.id))
            const step = name === "next_view" ? 1 : -1
            const i = views.indexOf(Library.platformFilter)
            Library.platformFilter = views[(i + step + views.length) % views.length]
            break
        }
        case "toggle_favorite":
        case "hide_game":
            if (gameList.selectionCount > 0) {
                runOnMany(name, gameList.selectedIds())
                break
            }
            runOn(name, gameList.currentGame)
            break
        case "launch":
        case "open_platform":
        case "open_folder":
        case "open_save_folder":
        case "open_mod_manager":
        case "game_settings":
        case "play_history":
        case "uninstall_game":
            runOn(name, gameList.currentGame)
            break
        case "game_menu":
            if (details.hasGame)
                gameList.openMenu(Library.rowOfGame(gameList.currentGame.id))
            break
        case "app_menu": rail.openMenu(); break
        case "undo": toast.undo(); break
        case "play_statistics": setStatsOpen(!statsOpen); break
        case "gallery": setGalleryOpen(!galleryOpen); break
        case "add_game": addGame(); break
        case "toggle_fullscreen": toggleFullscreen(); break
        case "toggle_rail": {
            const error = General.setRailHidden(!General.railHidden)
            if (error.length > 0)
                toast.show(error)
            break
        }
        case "toggle_toolbar": {
            const error = General.setToolbar(!General.toolbar)
            if (error.length > 0)
                toast.show(error)
            break
        }
        case "open_settings": openSettings(); break
        case "setup_guide": openSetup(); break
        case "look_up_missing": openBulkLookup(); break
        case "scan_roms":
            if (!Library.scanning)
                Library.scanNow()
            break
        case "sync_steam":
            if (SteamLibrary.configured && !SteamLibrary.syncing) {
                SteamLibrary.syncNow()
                steamSyncAsked = SteamLibrary.syncing
                toast.show(steamSyncAsked ? qsTr("Syncing Steam library…") : steamSyncResult())
            }
            break
        case "apply_steam_changes":
            if (Library.steamWaiting.length > 0 && !GameRunner.closingSteam) {
                const error = GameRunner.closeSteamAndApply()
                if (error !== "")
                    toast.show(error)
            }
            break
        case "about":
            aboutDialog.active = true
            aboutDialog.item.open()
            break
        case "update_app": openUpdate(); break
        case "whats_new": openWhatsNew(); break
        case "open_data_folder": {
            const error = GameRunner.openDataFolder()
            if (error !== "")
                toast.show(error)
            break
        }
        case "back_up_library": openSettingsAt("library", "backup-library"); break
        case "back_up_saves": openSettingsAt("library", "backup-saves"); break
        case "import_backup": openSettingsAt("library", "import"); break
        case "show_shortcuts": openSettingsAt("keyboard"); break
        case "quit": Qt.quit(); break
        default: gameList.run(name)
        }
    }

    function runOn(name, game) {
        if (game.id === undefined)
            return
        switch (name) {
        case "launch": launch(game); break
        case "toggle_favorite": details.activate("favorite", game); break
        case "open_platform": details.activate("open_platform", game); break
        case "open_folder": details.activate("folder", game); break
        case "open_save_folder": details.activate("save_folder", game); break
        case "open_mod_manager": details.activate("mod_manager", game); break
        case "add_mod_manager": addModManager(game.id); break
        case "game_settings": editGame(game.id); break
        case "hide_game": hideGame(game); break
        case "play_history": openPlayHistory(game); break
        case "uninstall_game": uninstallGame(game); break
        }
    }

    function runOnMany(name, ids) {
        const games = ids.map(id => Library.gameAt(Library.rowOfGame(id)))
        let error = ""
        switch (name) {
        case "toggle_favorite":
            error = Library.setGamesValue(ids, "favorite", !games.every(g => g.favorite))
            break
        case "hide_game": {
            const hide = games.some(g => !g.hidden)
            const next = hide && !Library.showHidden ? gameList.neighborOutside(ids) : ""
            error = Library.setGamesValue(ids, "hidden", hide)
            if (error === "" && next !== "")
                gameList.selectGame(next)
            break
        }
        }
        if (error !== "")
            toast.show(error)
    }

    function hideGame(game) {
        if (game.hidden ?? false) {
            const error = Library.unhideGame(game.id)
            if (error !== "")
                toast.show(error)
            return
        }
        const next = game.id === gameList.selectedId && !Library.showHidden ? gameList.neighborOf(game.id) : ""
        const error = Library.removeGame(game.id, false)
        if (error !== "") {
            toast.show(error)
            return
        }
        if (next !== "")
            gameList.selectGame(next)
    }

    function uninstallGame(game) {
        if (game.platform === "steam" && (game.steamUninstallable ?? false)) {
            steamUninstall.active = true
            steamUninstall.item.ask(game)
        } else if (game.platform === "proton" || game.platform === "linux") {
            protonUninstall.active = true
            protonUninstall.next = game.id === gameList.selectedId ? gameList.neighborOf(game.id) : ""
            protonUninstall.item.ask(game)
        }
    }

    Loader {
        id: steamUninstall
        active: false
        source: "views/SteamUninstallPrompt.qml"
        onLoaded: item.scrim = true
    }
    Connections {
        target: steamUninstall.item
        function onFailed(text) { toast.show(text) }
    }

    Loader {
        id: whatsNewDialog
        active: false
        source: "views/WhatsNewDialog.qml"
    }
    Connections {
        target: whatsNewDialog.item
        function onClosed() { gameList.focusList() }
    }

    Loader {
        id: aboutDialog
        active: false
        source: "views/AboutDialog.qml"
    }
    Connections {
        target: aboutDialog.item
        function onClosed() { gameList.focusList() }
        function onUpdateRequested() { window.openUpdate() }
    }

    Loader {
        id: updateDialog
        active: false
        source: "views/UpdateDialog.qml"
    }
    Connections {
        target: updateDialog.item
        function onClosed() { gameList.focusList() }
        function onMessage(text) { toast.show(text) }
    }

    Loader {
        id: protonUninstall
        property string next: ""
        active: false
        source: "views/ProtonUninstall.qml"
        onLoaded: item.scrim = true
    }
    Connections {
        target: protonUninstall.item
        function onDone(removedGame) {
            if (removedGame && protonUninstall.next !== "")
                gameList.selectGame(protonUninstall.next)
        }
    }

    readonly property var setupActions: setupGuide.active ? ["toggle_fullscreen", "quit"] : null

    ActionShortcuts {
        scope: "main"
        only: window.setupActions
    }

    Connections {
        target: Actions
        function onTriggered(name, target) {
            if (target === window && (window.setupActions === null || window.setupActions.includes(name)))
                window.run(name)
        }
    }

    // The window is made from a Component that nothing keeps, so a later step can let Qt
    // drop the compiled file with it.
    function createWindow(url) {
        const component = Qt.createComponent(url)
        if (component.status !== Component.Ready) {
            console.warn(component.errorString())
            return null
        }
        return component.createObject(window)
    }

    width: WindowState.width("main", 1600)
    height: WindowState.height("main", 950)
    // The details go compact long before this (details.compact_width).
    minimumWidth: Theme.rail.width + Theme.list.width + Theme.details.marginLeft + Theme.details.minWidth
                  + Theme.details.compactMarginRight
    minimumHeight: 600
    visibility: General.startedFullscreen ? Window.FullScreen
              : WindowState.maximized("main") ? Window.Maximized : Window.Windowed
    Component.onCompleted: WindowState.track(this, "main")
    title: qsTr("Lattis")
    color: Theme.palette.background
    flags: Qt.Window | Qt.FramelessWindowHint

    font.pixelSize: Theme.font.size
    font.family: Theme.font.family !== "" ? Theme.font.family : Qt.application.font.family

    palette.toolTipBase: Theme.tooltip.background
    palette.toolTipText: Theme.tooltip.text
    palette.dark: Theme.tooltip.border
    palette.highlight: Theme.palette.accent
    palette.highlightedText: Theme.palette.onAccent

    // Every tooltip is the attached ToolTip's one instance, shared by all windows of the engine, and
    // the style's has no transitions. Made after the first frame, which making it would delay.
    Item {
        id: tooltipHost

        Component {
            id: tooltipBackground
            Rectangle {
                radius: Theme.radius.tiny
                color: Theme.tooltip.background
                border.color: Theme.tooltip.border
                border.width: Theme.tooltip.borderWidth
            }
        }
        Transition {
            id: tooltipEnter
            MotionNumber { property: "opacity"; from: 0; to: 1 }
        }
        Transition {
            id: tooltipExit
            MotionNumber { property: "opacity"; to: 0 }
        }
        Connections {
            id: tooltipSetup
            target: window
            function onFrameSwapped() {
                tooltipSetup.enabled = false
                tooltipHost.ToolTip.toolTip.enter = tooltipEnter
                tooltipHost.ToolTip.toolTip.exit = tooltipExit
                tooltipHost.ToolTip.toolTip.background = tooltipBackground.createObject(tooltipHost)
            }
        }
    }

    // Window background gradient; when backgroundEnd equals background this is flat,
    // and the ApplicationWindow.color above provides the same solid fill at no extra cost.
    Surface {
        anchors.fill: parent
        fill: Theme.palette.background
        fillEnd: Theme.palette.backgroundEnd
        direction: Theme.palette.gradientDirection
    }

    Backdrop {
        visible: Theme.backdrop.enabled && !window.pageCovering
        x: Theme.backdrop.area === "details" ? details.x : 0
        width: parent.width - x
        height: parent.height * Theme.backdrop.height
        game: Theme.backdrop.enabled ? details.shownHeroGame : ({})
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        enabled: !setupGuide.active
        visible: !window.pageCovering

        Rail {
            id: rail
            // Above the list, which reaches into the rail's slot while the rail is hidden, so the logo stays on top.
            z: 1
            startedUp: window.startedUp
            Layout.fillHeight: true
            Layout.preferredWidth: Theme.rail.width
            onAddGame: window.addGame()
            onOpenSettings: window.openSettings()
            onAction: name => window.run(name)
            onEditTheme: window.openSettingsAt("appearance")
            onMessage: text => toast.show(text)
            onUndoable: (text, link) => toast.show(text, link, toast.undoDuration)
            onEditPlatform: id => window.openSettingsAt("platforms", id)
        }

        GameList {
            id: gameList
            reach: General.railHidden ? Theme.rail.width + Theme.list.padding - Theme.list.railHiddenMargin
                                      : Theme.list.padding - Theme.list.railMargin
            extraTop: General.railHidden ? Theme.list.railHiddenTopMargin : Theme.list.railTopMargin
            Layout.fillHeight: true
            Layout.preferredWidth: Theme.list.width
            onLaunchRequested: window.launch(currentGame)
            onMessage: text => toast.show(text)
            onGameAction: (name, game) => window.runOn(name, game)
            onGamesAction: (name, ids) => window.runOnMany(name, ids)
            onModManagerRequested: id => window.openModManager(id)
        }

        GameDetails {
            id: details
            Layout.fillWidth: true
            Layout.fillHeight: true
            game: gameList.currentGame
            allCollapsed: gameList.allCollapsed
            dragReserve: windowControls.width + 2 * Theme.windowControls.margin
            startedUp: window.startedUp
            toolbarLeft: toolbar.visible ? toolbar.x - x : -1
            onLaunchRequested: window.launch(game)
            onStopRequested: window.askForceClose(game)
            onMessage: text => toast.show(text)
            onEmulatorMessage: (text, gameId) => window.showEmulatorMessage(text, gameId)
            onClicked: {
                gameList.clearSelection()
                gameList.focusList()
            }
            onEditRequested: window.editGame(game.id)
            onAddRequested: window.addGame()
            onSteamUndoRequested: window.askSteamUndo()
            onHistoryRequested: window.openPlayHistory(game)
            onMenuRequested: button => gameList.openMenuBelow(Library.rowOfGame(game.id), button)
            onCustomizeRequested: window.openSettingsAt("game_page")
            onGalleryRequested: id => window.setGalleryOpen(true, id)
            onModManagerRequested: id => window.openModManager(id)
        }
    }

    // Its own stacking context, so a page can drop below the other without going under the game page.
    Item {
        anchors.fill: parent

        Loader {
            id: statsPage
            anchors.fill: parent
            active: false
            z: window.pageOnTop === "stats" ? 1 : 0
            visible: opacity > 0
            opacity: window.statsOpen || window.pageBelow === "stats" ? 1 : 0
            onOpacityChanged: if (opacity === 1 && window.statsOpen) window.pageBelow = ""
            // Without a layer each card fades on its own over the fading background, so the cards
            // stack to look more solid and the background seems to go first.
            layer.enabled: opacity > 0 && opacity < 1
            // Hidden pages get no input anyway; staying enabled keeps their buttons from fading up from the
            // disabled look as the page fades in, or dimming on the page left below during a switch.
            enabled: (window.statsOpen || window.pageBelow === "stats" || opacity === 0) && !setupGuide.active
            source: "views/StatsPage.qml"

            Behavior on opacity {
                enabled: Motion.fades
                MotionNumber { fade: true; speed: "normal"; linear: true }
            }
        }
        Binding {
            target: statsPage.item
            property: "dragReserve"
            value: windowControls.width + 2 * Theme.windowControls.margin
        }
        Connections {
            target: statsPage.item
            function onCloseRequested() { window.setStatsOpen(false) }
            function onGameRequested(id) {
                gameList.revealGame(id)
                window.setStatsOpen(false)
            }
        }

        Loader {
            id: galleryPage
            anchors.fill: parent
            active: false
            z: window.pageOnTop === "gallery" ? 1 : 0
            visible: opacity > 0
            opacity: window.galleryOpen || window.pageBelow === "gallery" ? 1 : 0
            onOpacityChanged: if (opacity === 1 && window.galleryOpen) window.pageBelow = ""
            layer.enabled: opacity > 0 && opacity < 1
            enabled: (window.galleryOpen || window.pageBelow === "gallery" || opacity === 0) && !setupGuide.active
            source: "views/GalleryPage.qml"

            Behavior on opacity {
                enabled: Motion.fades
                MotionNumber { fade: true; speed: "normal"; linear: true }
            }
        }
        Binding {
            target: galleryPage.item
            property: "dragReserve"
            value: windowControls.width + 2 * Theme.windowControls.margin
        }
        Connections {
            target: galleryPage.item
            function onCloseRequested() { window.setGalleryOpen(false) }
            function onMessage(text) {
                if (text !== "")
                    toast.show(text)
            }
            function onGameRequested(id) {
                gameList.revealGame(id)
                window.setGalleryOpen(false)
            }
        }
    }

    // A click anywhere outside the list clears a multi-selection. Passive, so the click still
    // reaches what it landed on.
    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        grabPermissions: PointerHandler.ApprovesTakeOverByAnything
        enabled: gameList.selectionCount > 0
        onTapped: eventPoint => {
            if (!gameList.contains(gameList.mapFromItem(null, eventPoint.scenePosition)))
                gameList.clearSelection()
        }
    }

    TopToolbar {
        id: toolbar
        Component.onCompleted: window.toolbarHovered = Qt.binding(() => toolbar.hovered)
        readonly property real low: details.x + Theme.details.marginLeft
        readonly property real high: windowControls.visible ? windowControls.x - Theme.layout.spacing
                                                            : window.width - Theme.layout.margin
        x: high - low < width ? low + (high - low - width) / 2
                              : Math.max(low, Math.min(high - width, (window.width - width) / 2))
        y: Theme.toolbar.marginTop
        checkedButton: window.statsOpen ? "play_statistics" : window.galleryOpen ? "gallery" : ""
        onTriggered: name => window.run(name)
        onMessage: text => toast.show(text)
    }

    Loader {
        id: setupGuide
        property bool reopened: false
        anchors.fill: parent
        active: false
        source: "views/SetupGuide.qml"
    }
    Binding {
        target: setupGuide.item
        property: "reopened"
        value: setupGuide.reopened
    }
    Binding {
        target: setupGuide.item
        property: "dragReserve"
        value: windowControls.width + 2 * Theme.windowControls.margin
    }
    Connections {
        target: setupGuide.item
        function onMessage(text) { toast.show(text) }
        function onClosed() {
            Qt.callLater(() => {
                setupGuide.active = false
                gameList.focusList()
            })
        }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
    }

    WindowControls {
        id: windowControls
        panel: true
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.windowControls.margin
    }

    WindowFrame {}
}
