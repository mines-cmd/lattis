import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

ApplicationWindow {
    id: window
    property bool toolbarHovered: false
    property bool noticesHovered: false

    function launch(game) {
        if (!game.installed && (game.platform === "proton" || game.platform === "linux")) {
            locateProgram(game)
            return
        }
        if (!game.installed && (game.launcher ?? "") === "")
            return
        // Without Legendary (a Flatpak Heroic) it stays what it was: open Heroic and copy the title.
        if (!game.installed && game.platform === "epic" && GameRunner.legendaryFound()) {
            if ((game.steamUpdate ?? "none") === "none")
                askEpicInstall(game)
            return
        }
        // Edits waiting for Steam to close: ask whether to restart it. With Steam closed,
        // they're written first without asking.
        if ((game.platform === "steam" || game.platform === "proton") && game.installed && game.pendingEdits === "steam"
                && !GameRunner.running.includes(game.id) && GameRunner.steamRunning()) {
            steamPrompt.active = true
            steamPrompt.item.ask(game)
            return
        }
        const error = GameRunner.launch(game.id)
        if (error !== "")
            showEmulatorMessage(error, game.id)
    }

    function runningGameTitles() {
        return GameRunner.running.filter(key => !key.startsWith("mod-manager:"))
                                 .map(id => GameRunner.runningTitle(id))
    }

    property bool quitConfirmed: false

    function requestQuit() {
        const titles = runningGameTitles()
        const jobs = GameRunner.epicJobTitles()
        if (titles.length === 0 && jobs.length === 0) {
            Qt.quit()
            return
        }
        quitPrompt.active = true
        quitPrompt.item.ask(titles, jobs)
    }

    onClosing: close => {
        if (quitConfirmed || (runningGameTitles().length === 0 && GameRunner.epicJobTitles().length === 0))
            return
        close.accepted = false
        requestQuit()
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

    function askEpicInstall(game) {
        epicInstall.active = true
        epicInstall.item.ask(game)
    }

    Loader {
        id: epicInstall
        active: false
        source: "views/EpicInstallDialog.qml"
    }
    Connections {
        target: epicInstall.item
        function onClosed() { browser.focusList() }
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

    function themeDialogInSettings(kind) {
        settingsWindow.themeDialogForScreenshot(kind)
    }

    function makeUndoableChange(kind) {
        const game = browser.currentGame
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
        browser.selectGame(id)
        openPlayHistory(browser.currentGame)
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
    property bool overviewOpen: false
    // The views under the page stay drawn while it fades in and out.
    readonly property bool statsCovering: statsOpen && statsPage.opacity === 1
    readonly property bool overviewCovering: overviewOpen && overviewPage.opacity === 1
    readonly property string openPage: statsOpen ? "stats" : galleryOpen ? "gallery" : overviewOpen ? "overview" : ""

    // Switching between the pages keeps the old one drawn under the new one until the new one
    // covers it, so they crossfade instead of both fading through the game page.
    property string pageBelow: ""
    // Kept after the switch ends, so the old page fades out under the new one rather than over it.
    property string pageOnTop: ""

    function setStatsOpen(open, switching = false) {
        if (open === statsOpen)
            return
        if (open) {
            const below = openPage
            gamePageOpen = false
            setGalleryOpen(false, "", true)
            setOverviewOpen(false, true)
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
            browser.focusList()
        }
    }

    function setOverviewOpen(open, switching = false) {
        if (open === overviewOpen)
            return
        if (open) {
            const below = openPage
            gamePageOpen = false
            setStatsOpen(false, true)
            setGalleryOpen(false, "", true)
            pageBelow = below
            pageOnTop = "overview"
            overviewPage.asynchronous = false
            overviewPage.active = true
            overviewOpen = true
            overviewPage.item.start()
            if (overviewPage.opacity === 1)
                pageBelow = ""
        } else {
            overviewOpen = false
            pageBelow = ""
            if (!switching)
                overviewPage.item.leave()
            browser.focusList()
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
            // Made over several frames, as it takes ~45 ms; a click before that finishes it at once.
            if (!overviewPage.active) {
                overviewPage.asynchronous = true
                overviewPage.active = true
            }
        }
    }
    readonly property bool galleryCovering: galleryOpen && galleryPage.opacity === 1
    readonly property bool pageCovering: statsCovering || galleryCovering || overviewCovering
    readonly property bool gridView: General.view === "grid"
    // Here rather than in GameGrid: a Binding destroyed with its Loader item doesn't restore the value.
    Binding { target: Library; property: "flat"; value: window.gridView }
    readonly property Item browser: gridLoader.item ?? gameList
    readonly property Item details: gridView ? pageDetails.item : listDetails.item
    property bool gamePageOpen: false
    // 0 shows the grid, 1 the game page. They take turns over the shared backdrop rather than
    // crossfading, which shows both layouts at once: two titles, two Launch buttons.
    property real gamePageShown: gamePageOpen ? 1 : 0
    Behavior on gamePageShown {
        enabled: Motion.fades
        MotionNumber { fade: true; speed: "normal"; linear: true }
    }
    readonly property bool gamePageCovering: gamePageOpen && gamePageShown === 1

    // From the switch until the snapshot starts to fade: the new view's art loads without fading in,
    // to show whole when the snapshot goes.
    property bool viewCovered: false
    // The new view rises into place from this far down as the snapshot fades.
    property real viewRise: 0

    // Kept as the grid changes it: by the time gridView turns false the grid may already be gone.
    property var gridSelection: ({ id: "", ids: [] })

    onGridViewChanged: {
        if (!gridView) {
            gamePageOpen = false
            pageDetails.active = false
            if (gridSelection.id !== "") {
                gameList.selectGame(gridSelection.id)
                gameList.setSelection(gridSelection.ids)
            }
        }
        Qt.callLater(() => browser.focusList())
    }

    function setGamePageOpen(open) {
        if (open === gamePageOpen)
            return
        if (open) {
            if (!gridView || browser.currentGame.id === undefined)
                return
            setStatsOpen(false, true)
            setGalleryOpen(false, "", true)
            setOverviewOpen(false, true)
            pageDetails.active = true
            gamePageOpen = true
            gamePage.forceActiveFocus()
        } else {
            gamePageOpen = false
            browser.focusList()
        }
    }

    function switchView() {
        const next = gridView ? "list" : "grid"
        const apply = () => {
            const error = General.setView(next)
            if (error.length > 0)
                toast.show(error)
        }
        // The pages sit over the views: switching under one would show nothing, so it closes to reveal the new view.
        if (openPage !== "") {
            apply()
            setStatsOpen(false)
            setOverviewOpen(false)
            setGalleryOpen(false)
            return
        }
        if (!Motion.fades || setupGuide.active || gamePageCovering) {
            apply()
            return
        }
        if (viewWait.running)
            revealView()
        viewReveal.complete()
        viewCover.pending = apply
        // Not just the library: the backdrop changes with the view too.
        viewArea.grabToImage(result => {
            // The url is only good while the result lives.
            viewCover.grab = result
            viewSnapshot.source = result.url
            // Setting the source can load the image and run onStatusChanged's coverView() already.
            if (viewCover.pending !== null && viewSnapshot.status === Image.Ready)
                coverView()
        })
    }

    // Only once the snapshot can be drawn: switching before it is would show the new view half-made.
    function coverView() {
        const apply = viewCover.pending
        viewCover.pending = null
        viewCover.opacity = 1
        viewCovered = true
        apply()
        viewRise = Motion.on ? Theme.motion.viewSwitchRise : 0
        viewWait.since = Date.now()
        viewWait.start()
    }

    function newViewReady(): bool {
        return gridView ? (gridLoader.item?.artReady() ?? true) : !(listDetails.item?.heroLoading ?? false)
    }

    function revealView() {
        viewWait.stop()
        viewCovered = false
        viewReveal.restart()
    }

    // The page stays until the game's art is in: fading off at once showed the art pop in and the
    // hero's own switch play through the page.
    function closePageOnGame(id, close) {
        browser.revealGame(id)
        if (!Motion.fades) {
            close()
            return
        }
        pageWait.close = close
        pageWait.gameId = id
        pageWait.since = Date.now()
        pageWait.start()
    }

    function gameShown(id): bool {
        if (gridView)
            return gridLoader.item?.artReady() ?? true
        return !listDetails.item || (listDetails.item.shownHeroGame.id === id && !listDetails.item.heroLoading)
    }

    function setGalleryOpen(open, gameId = "", switching = false) {
        if (open === galleryOpen)
            return
        if (open) {
            const below = openPage
            gamePageOpen = false
            setStatsOpen(false, true)
            setOverviewOpen(false, true)
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
            browser.focusList()
        }
    }

    // "1" opens the page; "viewer" or "viewer/<n>" also opens the n-th item (the first), "menu" the first tile's menu; ";"-joined "filter:<section>=<keys>" and "panel" set filters and open the narrow window's panel.
    function galleryForScreenshot(spec) {
        setGalleryOpen(true)
        galleryPage.item.autoOpen = spec
    }

    // "edit" also turns on edit mode; "edit/<card>:<dx>:<dy>" shows that card held and dragged that far.
    function overviewForScreenshot(mode) {
        setOverviewOpen(true)
        if (mode.startsWith("edit"))
            overviewPage.item.editForScreenshot(mode.split("/")[1] ?? "")
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
        browser.selectGame(id)
    }

    function scrollList(pixelsPerSecond) {
        browser.scrollThrough(pixelsPerSecond)
    }

    function selectAndLaunch(id) {
        browser.selectGame(id)
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
        function onClosed() { browser.focusList() }
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
        function onClosed() { browser.focusList() }
    }

    Loader {
        id: steamUndo
        active: false
        source: "views/WaitingUndoPrompt.qml"
    }
    Connections {
        target: steamUndo.item
        function onMessage(text) { toast.show(text) }
        function onClosed() { browser.focusList() }
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
        function onClosed() { browser.focusList() }
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
        whatsNewDialog.item.heading = ""
        whatsNewDialog.item.sections = []
        whatsNewDialog.item.version = updatedVersion
        whatsNewDialog.item.notes = updatedNotes
        whatsNewDialog.item.open()
    }

    // Emulators and games updated since the toast last went away; one update at a time or "Update all".
    property var emulatorReleases: []

    function emulatorUpdated(name, version, notes) {
        // The open updates dialog shows the notes and the result itself.
        if (updatesDialog.item?.opened)
            return
        emulatorReleases = emulatorReleases.concat([{ name, version, notes }])
        const count = emulatorReleases.length
        toast.show(count === 1 ? qsTr("Updated %1 to %2").arg(name).arg(version)
                               : qsTr("Updated %n item(s)", "", count), {
            text: qsTr("What's new"),
            run: () => openEmulatorNotes()
        }, 10000)
    }

    function openEmulatorNotes() {
        const releases = emulatorReleases
        emulatorReleases = []
        if (releases.length === 0)
            return
        whatsNewDialog.active = true
        whatsNewDialog.item.heading = releases.length === 1
            ? qsTr("What's new in %1 %2").arg(releases[0].name).arg(releases[0].version)
            : qsTr("What's new in your updates")
        whatsNewDialog.item.notes = releases.length === 1 ? releases[0].notes : ""
        whatsNewDialog.item.sections = releases.length === 1 ? []
            : releases.map(r => ({ title: r.name + " " + r.version, notes: r.notes }))
        whatsNewDialog.item.open()
    }

    property bool emulatorCheckAsked: false
    Connections {
        target: Installer
        function onEmulatorUpdated(name, version, notes) { window.emulatorUpdated(name, version, notes) }
        function onReviewRequested(gameId) { window.openUpdates(gameId) }
        function onUpdatesChecked(checked, failed) {
            if (!window.emulatorCheckAsked)
                return
            window.emulatorCheckAsked = false
            const updates = Installer.updates
            const failure = failed === 0 ? ""
                : failed === checked ? qsTr("Couldn't reach the releases")
                : qsTr("couldn't check %n item(s)", "", failed)
            toast.show(updates.length === 0
                           ? (failed === checked ? failure : failed > 0 ? qsTr("No updates found; %1").arg(failure) : qsTr("Everything is up to date"))
                       : updates.length === 1 ? qsTr("Update available for %1: %2").arg(updates[0].name).arg(updates[0].version)
                       : qsTr("%n update(s) available", "", updates.length),
                       updates.length > 0 ? { text: qsTr("Review"), run: () => window.run("update_emulators") } : null)
        }
    }

    function openUpdates(gameId) {
        updatesDialog.active = true
        updatesDialog.item.show(gameId)
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

    function emulatorNotesForScreenshot(mode) {
        const notes = qsTr("## Highlights\n\n- Faster shader compilation\n- Fixed crash on startup with some controllers\n- New **Vulkan** renderer option\n\nSee the [full changelog](https://example.com).")
        emulatorUpdated("Eden", "v0.2.1", notes)
        if (mode === "many")
            emulatorUpdated("DuckStation", "0.1-9000", "")
        if (mode !== "toast")
            openEmulatorNotes()
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
        browser.selectGame(id)
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

    function tabInEditor(tab) {
        if (editorLoader.item)
            editorLoader.item.tab = tab
    }

    function copyChangesInEditor(spec) {
        const editor = editorLoader.item
        if (!editor)
            return
        for (const part of spec.split(",")) {
            const [op, id] = part.split(":")
            if (op === "link")
                editor.addCopy(id)
            else if (op === "unlink")
                editor.toggleCopyUnlink(id)
            else if (op === "main")
                editor.setCopyMain(id)
        }
    }

    function mediaFoldersInEditor() {
        editorLoader.item?.openMediaFolders()
    }

    function installDialogForScreenshot() {
        if (gridView)
            browser.openInstallDialog()
        else
            details?.openInstallDialog()
    }

    function logoForScreenshot(mode) {
        details?.logoForScreenshot(mode)
    }

    function gameMenuForScreenshot(spec) {
        const [id, submenu] = spec.split("/")
        if (id === "page" && gridView)
            browser.openPanelMenu()
        else if (id === "page")
            details?.openMenu()
        else
            browser.openMenu(Library.rowOfGame(id))
        if (submenu !== undefined)
            browser.openMenuSubmenu(submenu)
    }

    function gamePageMenuForScreenshot(spec) {
        const [place, submenu] = spec.split("/")
        if (gridView)
            browser.pageMenuForScreenshot(place === "chips" ? "grid_chips" : "grid_panels")
        else
            details?.pageMenuForScreenshot(place, submenu)
    }

    function clipForScreenshot(spec) {
        details?.clipForScreenshot(spec)
    }

    function selectGamesForScreenshot(ids) {
        browser.selectGames(ids.split(","))
    }

    function platformMenuForScreenshot(id) {
        rail.openPlatformMenuForScreenshot(id)
    }

    function railMenuForScreenshot(which) {
        if (which === "friends")
            rail.openStatusMenuForScreenshot()
        else
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
        function onClosed() { browser.focusList() }
        function onSaved(id) { browser.selectGame(id) }
        function onSelectRequested(id) { browser.selectGame(id) }
        function onRemoved(next) { browser.selectGame(next) }
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
            settingsWindow.game = Qt.binding(() => browser.currentGame)
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
        // The viewer's own handler toggles the open item's favorite instead.
        if (name === "toggle_favorite" && ((details?.viewerOpen ?? false) || (galleryPage.item?.viewerOpen ?? false)))
            return
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
            if (browser.selectionCount > 0) {
                runOnMany(name, browser.selectedIds())
                break
            }
            runOn(name, browser.currentGame)
            break
        case "launch":
        case "open_platform":
        case "open_folder":
        case "open_save_folder":
        case "open_mod_manager":
        case "game_settings":
        case "play_history":
        case "steam_properties":
        case "uninstall_game":
            runOn(name, browser.currentGame)
            break
        case "game_menu":
            if (browser.currentGame.id !== undefined)
                browser.openMenu(Library.rowOfGame(browser.currentGame.id))
            break
        case "app_menu": rail.openMenu(); break
        case "undo": toast.undo(); break
        case "overview": setOverviewOpen(!overviewOpen); break
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
        case "toggle_view": switchView(); break
        case "open_game_page": setGamePageOpen(true); break
        case "toggle_grid_panel": {
            if (!gridView)
                break
            const error = General.setGridPanel(!General.gridPanel)
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
        case "apply_heroic_changes":
            if (Library.heroicWaiting.length > 0 && !GameRunner.closingHeroic) {
                const error = GameRunner.closeHeroicAndApply()
                if (error !== "")
                    toast.show(error)
            }
            break
        case "about":
            aboutDialog.active = true
            aboutDialog.item.open()
            break
        case "update_app": openUpdate(); break
        case "check_emulator_updates":
            if (!emulatorCheckAsked) {
                const looking = Installer.checkUpdatesAsked()
                emulatorCheckAsked = looking > 0
                toast.show(looking === 0 ? qsTr("Nothing installed by Lattis to check")
                                         : qsTr("Checking for updates…"))
            }
            break
        case "update_emulators":
            openUpdates()
            break
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
        case "quit": requestQuit(); break
        default:
            // The list is under a covering page: its keys would move the selection and take focus from the page.
            if (!(pageCovering && browser.coveredActions.includes(name)))
                browser.run(name)
        }
    }

    // The button logic lives in GameDetails, which grid view doesn't create until it needs it.
    function activateOn(button, game) {
        if (details === null)
            pageDetails.active = true
        details.activate(button, game)
    }

    function runOn(name, game) {
        if (game.id === undefined)
            return
        switch (name) {
        case "launch": launch(game); break
        case "update_game":
            if (game.launcher === "heroic")
                GameRunner.update(game.id)
            else
                Installer.review(game.id)
            break
        case "toggle_favorite": activateOn("favorite", game); break
        case "open_platform": activateOn("open_platform", game); break
        case "open_folder": activateOn("folder", game); break
        case "open_save_folder": activateOn("save_folder", game); break
        case "open_mod_manager": activateOn("mod_manager", game); break
        case "open_game_page": setGamePageOpen(true); break
        case "add_mod_manager": addModManager(game.id); break
        case "game_settings": editGame(game.id); break
        case "hide_game": hideGame(game); break
        case "play_history": openPlayHistory(game); break
        case "steam_properties": showError(GameRunner.openSteamProperties(game.id)); break
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
            const next = hide && !Library.showHidden ? browser.neighborOutside(ids) : ""
            error = Library.setGamesValue(ids, "hidden", hide)
            if (error === "" && next !== "")
                browser.selectGame(next)
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
        const next = game.id === browser.selectedId && !Library.showHidden ? browser.neighborOf(game.id) : ""
        const error = Library.removeGame(game.id, false)
        if (error !== "") {
            toast.show(error)
            return
        }
        if (next !== "")
            browser.selectGame(next)
    }

    function showError(error) {
        if (error.length > 0)
            toast.show(error)
    }

    function uninstallGame(game) {
        if (game.platform === "steam" && (game.steamUninstallable ?? false)) {
            steamUninstall.active = true
            steamUninstall.item.ask(game)
        } else if (game.platform === "epic" && (game.installed ?? false)) {
            epicUninstall.active = true
            epicUninstall.item.ask(game)
        } else if (game.platform === "proton" || game.platform === "linux") {
            protonUninstall.active = true
            protonUninstall.next = game.id === browser.selectedId ? browser.neighborOf(game.id) : ""
            protonUninstall.item.ask(game)
        }
    }

    Loader {
        id: epicUninstall
        active: false
        source: "views/EpicUninstall.qml"
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
        function onClosed() { browser.focusList() }
    }

    Loader {
        id: updatesDialog
        active: false
        source: "views/UpdatesDialog.qml"
    }
    Connections {
        target: updatesDialog.item
        function onClosed() { browser.focusList() }
    }

    Loader {
        id: quitPrompt
        active: false
        source: "views/QuitPrompt.qml"
    }
    Connections {
        target: quitPrompt.item
        function onClosed() { browser.focusList() }
        function onQuitConfirmed() {
            window.quitConfirmed = true
            Qt.quit()
        }
    }

    Loader {
        id: aboutDialog
        active: false
        source: "views/AboutDialog.qml"
    }
    Connections {
        target: aboutDialog.item
        function onClosed() { browser.focusList() }
        function onUpdateRequested() { window.openUpdate() }
    }

    Loader {
        id: updateDialog
        active: false
        source: "views/UpdateDialog.qml"
    }
    Connections {
        target: updateDialog.item
        function onClosed() { browser.focusList() }
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
                browser.selectGame(protonUninstall.next)
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

    // The grid's backdrop follows the selection after the hero's delay, so holding an arrow key doesn't decode every image on the way.
    readonly property var gridCurrent: gridLoader.item?.currentGame ?? ({})
    property var gridHeroGame: ({})
    onGridCurrentChanged: {
        if (gridHeroGame.id === undefined || Theme.hero.loadDelay === 0)
            gridHeroGame = gridCurrent
        else
            gridHeroTimer.restart()
    }
    Timer {
        id: gridHeroTimer
        interval: Theme.hero.loadDelay
        onTriggered: window.gridHeroGame = window.gridCurrent
    }

    // What the view switch grabs: everything the switch changes, under the toolbar and window controls.
    Item {
        id: viewArea
        anchors.fill: parent

        // Window background gradient; when backgroundEnd equals background this is flat,
        // and the ApplicationWindow.color above provides the same solid fill at no extra cost.
        Surface {
            anchors.fill: parent
            fill: Theme.palette.background
            fillEnd: Theme.palette.backgroundEnd
            direction: Theme.palette.gradientDirection
        }

        Backdrop {
            readonly property bool on: window.gridView ? Theme.grid.backdropEnabled : Theme.backdrop.enabled
            visible: on && !window.pageCovering
            instant: window.pageCovering
            x: Theme.backdrop.area === "details" && !window.gridView ? listDetails.x : 0
            width: parent.width - x
            height: parent.height * heightFraction
            heightFraction: window.gridView ? Theme.grid.backdropHeight : Theme.backdrop.height
            strength: window.gridView ? Theme.grid.backdropOpacity : Theme.backdrop.opacity
            saturation: window.gridView ? Theme.grid.backdropSaturation : Theme.backdrop.saturation
            blurAmount: window.gridView ? Theme.grid.backdropBlur : Theme.backdrop.blur
            fadeStart: window.gridView ? Theme.grid.backdropFadeStart : Theme.backdrop.fadeStart
            game: !on ? ({})
                : window.gridView && !window.gamePageOpen ? window.gridHeroGame
                : (details?.shownHeroGame ?? ({}))
        }

        RowLayout {
            id: libraryLayout
            anchors.fill: parent
            spacing: 0
            enabled: !setupGuide.active
            visible: !window.pageCovering && !window.gamePageCovering
            // Gone before the game page is halfway in.
            opacity: 1 - Math.min(1, window.gamePageShown / 0.6)
            // Without a layer the panel's cards fade over the covers fading behind them and stack.
            layer.enabled: opacity > 0 && opacity < 1

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

            Loader {
                id: gridLoader
                active: window.gridView
                visible: active
                Layout.fillWidth: true
                Layout.fillHeight: true
                transform: Translate { y: window.viewRise }
                sourceComponent: GameGrid {
                    coverFades: !window.viewCovered
                    // Start from the list's selection; the grid's first own pick replaces these bindings.
                    selectedId: gameList.selectedId
                    selection: gameList.selection
                    onSelectedIdChanged: window.gridSelection = { id: selectedId, ids: Object.keys(selection) }
                    onSelectionChanged: window.gridSelection = { id: selectedId, ids: Object.keys(selection) }
                    toolbarLeft: toolbar.visible ? toolbar.x - parent.x : -1
                    onLaunchRequested: window.launch(currentGame)
                    onStopRequested: window.askForceClose(currentGame)
                    onMessage: text => toast.show(text)
                    onEmulatorMessage: (text, gameId) => window.showEmulatorMessage(text, gameId)
                    onGameAction: (name, game) => window.runOn(name, game)
                    onGamesAction: (name, ids) => window.runOnMany(name, ids)
                    onModManagerRequested: id => window.openModManager(id)
                    onEditRequested: window.editGame(currentGame.id)
                    onHistoryRequested: window.openPlayHistory(currentGame)
                    onCustomizeRequested: window.openSettingsAt("game_page")
                    onPageRequested: window.setGamePageOpen(true)
                }
            }

            GameList {
                id: gameList
                visible: !window.gridView
                reach: General.railHidden ? Theme.rail.width + Theme.list.padding - Theme.rail.containerMargin - Theme.list.railHiddenMargin
                                          : Theme.list.padding - Theme.list.railMargin
                extraTop: General.railHidden ? Theme.list.railHiddenTopMargin : Theme.list.railTopMargin
                transform: Translate { y: window.viewRise }
                Layout.fillHeight: true
                Layout.preferredWidth: Theme.list.width
                onLaunchRequested: window.launch(currentGame)
                onMessage: text => toast.show(text)
                onGameAction: (name, game) => window.runOn(name, game)
                onGamesAction: (name, ids) => window.runOnMany(name, ids)
                onModManagerRequested: id => window.openModManager(id)
            }

            Loader {
                id: listDetails
                active: !window.gridView
                visible: active
                Layout.fillWidth: true
                Layout.fillHeight: true
                transform: Translate { y: window.viewRise }
                sourceComponent: detailsComponent
            }
        }
    }

    Component {
        id: detailsComponent

        GameDetails {
            game: browser.currentGame
            allCollapsed: browser.allCollapsed
            dragReserve: windowControls.width + 2 * Theme.windowControls.margin
            startedUp: window.startedUp
            heroEntering: window.viewCovered
            covered: window.pageCovering
            onLaunchRequested: window.launch(game)
            onStopRequested: window.askForceClose(game)
            onMessage: text => toast.show(text)
            onGameRequested: gameId => browser.selectGame(gameId)
            onEmulatorMessage: (text, gameId) => window.showEmulatorMessage(text, gameId)
            onClicked: {
                browser.clearSelection()
                browser.focusList()
            }
            onEditRequested: window.editGame(game.id)
            onAddRequested: window.addGame()
            onHistoryRequested: window.openPlayHistory(game)
            onMenuRequested: button => browser.openMenuBelow(Library.rowOfGame(game.id), button)
            onCustomizeRequested: window.openSettingsAt("game_page")
            onGalleryRequested: id => window.setGalleryOpen(true, id)
            onModManagerRequested: id => window.openModManager(id)
        }
    }

    Item {
        id: viewCover
        property var pending: null
        property var grab: null
        anchors.fill: parent
        visible: opacity > 0
        opacity: 0
        // Blurring as it fades, the old view melts into the new one instead of the two showing
        // sharp over each other.
        layer.enabled: visible && Theme.motion.viewSwitchBlur > 0
        layer.effect: MultiEffect {
            blurEnabled: true
            blurMax: Theme.motion.viewSwitchBlur
            blur: 1 - viewCover.opacity
        }

        // The grab is see-through wherever nothing was drawn.
        Rectangle {
            anchors.fill: parent
            color: window.color
        }

        Image {
            id: viewSnapshot
            anchors.fill: parent
            cache: false
            fillMode: Image.Stretch
            onStatusChanged: {
                if (viewCover.pending === null)
                    return
                if (status === Image.Ready) {
                    window.coverView()
                } else if (status === Image.Error) {
                    const apply = viewCover.pending
                    viewCover.pending = null
                    viewCover.grab = null
                    apply()
                }
            }
        }
    }

    FrameAnimation {
        id: viewWait
        property real since: 0
        onTriggered: {
            if (window.newViewReady() || Date.now() - since >= Theme.motion.viewSwitchWait)
                window.revealView()
        }
    }

    FrameAnimation {
        id: pageWait
        property var close: null
        property var gameId
        property real since: 0
        onTriggered: {
            if (window.gameShown(gameId) || Date.now() - since >= Theme.motion.viewSwitchWait) {
                stop()
                close()
            }
        }
    }

    ParallelAnimation {
        id: viewReveal
        onFinished: {
            viewSnapshot.source = ""
            viewCover.grab = null
        }
        // Linear, as the theme's curves drop a third of a large fade in its first frame.
        MotionNumber { target: viewCover; property: "opacity"; from: 1; to: 0; linear: true }
        MotionNumber { target: window; property: "viewRise"; to: 0; speed: "normal" }
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
            function onGameRequested(id) { window.closePageOnGame(id, () => window.setStatsOpen(false)) }
        }

        Loader {
            id: overviewPage
            anchors.fill: parent
            active: false
            z: window.pageOnTop === "overview" ? 1 : 0
            visible: opacity > 0
            opacity: window.overviewOpen || window.pageBelow === "overview" ? 1 : 0
            onOpacityChanged: if (opacity === 1 && window.overviewOpen) window.pageBelow = ""
            layer.enabled: opacity > 0 && opacity < 1
            enabled: (window.overviewOpen || window.pageBelow === "overview" || opacity === 0) && !setupGuide.active
            source: "views/OverviewPage.qml"
            onLoaded: if (!window.overviewOpen) item.warmUp()

            Behavior on opacity {
                enabled: Motion.fades
                MotionNumber { fade: true; speed: "normal"; linear: true }
            }
        }
        Binding {
            target: overviewPage.item
            property: "dragReserve"
            value: windowControls.width + 2 * Theme.windowControls.margin
        }
        Connections {
            target: overviewPage.item
            function onCloseRequested() { window.setOverviewOpen(false) }
            function onStatsRequested() { window.setStatsOpen(true) }
            function onMessage(text) { toast.show(text) }
            function onFilterRequested(platform, statuses, tags) {
                Library.showFiltered(platform, statuses, tags)
                window.setOverviewOpen(false)
            }
            function onYearRequested(year) {
                Library.showFiltered("", [], [])
                Library.yearFrom = year
                Library.yearTo = year
                window.setOverviewOpen(false)
            }
            function onAllBySizeRequested() {
                Library.showFiltered("", [], [])
                Library.installedOnly = true
                Library.sort = "size"
                Library.sortDescending = true
                window.setOverviewOpen(false)
            }
            function onPlayRequested(id) { window.launch(Library.gameWithId(id)) }
            function onGameRequested(id) { window.closePageOnGame(id, () => window.setOverviewOpen(false)) }
            function onAttentionRequested(kind) {
                switch (kind) {
                case "neverBackedUp": GameSaves.backUpAll(); break
                case "missingArt":
                case "missingInfo": window.openBulkLookup(window); break
                case "updates":
                    Library.showFiltered("steam", [], [])
                    window.setOverviewOpen(false)
                    break
                case "romsNotFound": window.openSettingsAt("platforms"); break
                case "emulatorsMissing": window.openSettingsAt("emulators"); break
                }
            }
        }

        Connections {
            target: GameSaves
            function onBackedUpAll() {
                if (window.overviewOpen && GameSaves.message !== "")
                    toast.show(GameSaves.message)
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
            function onGameRequested(id) { window.closePageOnGame(id, () => window.setGalleryOpen(false)) }
        }

        FocusScope {
            id: gamePage
            anchors.fill: parent
            visible: opacity > 0
            // Starts once the grid is mostly gone.
            opacity: Math.max(0, (window.gamePageShown - 0.4) / 0.6)
            layer.enabled: opacity > 0 && opacity < 1
            enabled: !setupGuide.active
            Keys.onEscapePressed: window.setGamePageOpen(false)
            // Squared, the rise settles as the fade ends, and the sink on closing comes from the same curve,
            // so turning back halfway doesn't jump.
            transform: Translate {
                y: Motion.on ? Theme.details.pageOffset * Math.pow(1 - gamePage.opacity, 2) : 0
            }

            Loader {
                id: pageDetails
                anchors.fill: parent
                active: false
                sourceComponent: detailsComponent
            }

            ChromeButton {
                x: Theme.layout.margin
                y: Theme.layout.margin
                icon: Icons.arrowUp
                iconRotation: -90
                label: qsTr("Back to games")
                tip: qsTr("Back to games (Esc)")
                onClicked: window.setGamePageOpen(false)
            }
        }
    }

    // A click anywhere outside the list clears a multi-selection. Passive, so the click still
    // reaches what it landed on.
    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        grabPermissions: PointerHandler.ApprovesTakeOverByAnything
        enabled: browser.selectionCount > 0
        onTapped: eventPoint => {
            if (!browser.contains(browser.mapFromItem(null, eventPoint.scenePosition)))
                browser.clearSelection()
        }
    }

    TopToolbar {
        id: toolbar
        Component.onCompleted: window.toolbarHovered = Qt.binding(() => toolbar.hovered)
        // In the grid it glides between the grid's edge and the game page's as the page comes and goes.
        readonly property real pageTurn: window.gamePageShown * window.gamePageShown * (3 - 2 * window.gamePageShown)
        readonly property real low: window.gridView
                                    ? (1 - pageTurn) * ((General.railHidden ? 0 : Theme.rail.width) + Theme.grid.margin)
                                      + pageTurn * Theme.details.marginLeft
                                    : listDetails.x + Theme.details.marginLeft
        readonly property real high: panelButton.visible ? panelButton.x - Theme.layout.spacing
                                   : windowControls.visible ? windowControls.x - Theme.layout.spacing
                                                            : window.width - Theme.layout.margin
        x: high - low < width ? low + (high - low - width) / 2
                              : Math.max(low, Math.min(high - width, (window.width - width) / 2))
        y: Theme.toolbar.marginTop
        started: window.startedUp
        checkedButton: window.statsOpen ? "play_statistics" : window.galleryOpen ? "gallery" : window.overviewOpen ? "overview" : ""
        onTriggered: name => window.run(name)
        onMessage: text => toast.show(text)
    }

    // Right of the toolbar, the same spot in the list and the grid.
    TopNotices {
        id: notices
        Component.onCompleted: window.noticesHovered = Qt.binding(() => notices.hovered)
        visible: !window.pageCovering && !setupGuide.active
        x: toolbar.x + toolbar.width + Theme.layout.spacing
        y: Theme.layout.margin
        width: Math.max(0, toolbar.high - x)
        height: Theme.layout.toolbarHeight
        startedUp: window.startedUp
        onMessage: text => toast.show(text)
        onUndoRequested: window.askSteamUndo()
        onGameRequested: id => browser.revealGame(id)
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
                browser.focusList()
            })
        }
    }

    Loader {
        id: dateFieldHost
        active: false
        anchors.horizontalCenter: parent.horizontalCenter
        sourceComponent: Rectangle {
            id: hostCard

            property alias field: hostField

            implicitWidth: hostColumn.implicitWidth + 2 * Theme.menu.dialogPadding
            implicitHeight: hostColumn.implicitHeight + 2 * Theme.menu.dialogPadding
            color: Theme.menu.popupBackground
            radius: Theme.controls.inputRadius

            ColumnLayout {
                id: hostColumn
                anchors.centerIn: parent
                spacing: Theme.controls.controlSpacing

                DateTimeField {
                    id: hostField
                    maximumDate: new Date()
                }
                Label {
                    text: hostField.valid ? "picked " + Qt.formatDateTime(hostField.value, "yyyy-MM-dd HH:mm") : "nothing picked"
                    color: Theme.controls.labelColor
                }
            }
        }
    }

    // Stands in for the Add session dialog until it exists: a date field to try the calendar on.
    function dateFieldForScreenshot(spec) {
        const parts = spec.split("/")
        dateFieldHost.active = true
        dateFieldHost.y = parts.includes("bottom") ? height - dateFieldHost.height - 40 : 80
        const when = parts[0].match(/^(\d+)-(\d+)-(\d+) (\d+):(\d+)$/)
        if (when)
            dateFieldHost.item.field.value = new Date(+when[1], +when[2] - 1, +when[3], +when[4], +when[5])
        const limit = parts.find(part => part.startsWith("max="))?.slice(4).split("-")
        if (limit)
            dateFieldHost.item.field.maximumDate = new Date(+limit[0], +limit[1] - 1, +limit[2])
        if (parts.includes("open"))
            dateFieldHost.item.field.openCalendar()
    }

    Toast {
        id: toast
        // The setup guide's Done step shows this itself.
        status: ScanLookup.running && !(setupGuide.active && !setupGuide.reopened)
                ? qsTr("Looking up new games: %1 of %2").arg(ScanLookup.progressDone).arg(ScanLookup.progressTotal)
                : window.emulatorCheckAsked ? qsTr("Checking for updates…")
                : ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3 * Theme.layout.spacing
    }

    WindowControls {
        id: windowControls
        panel: true
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.windowControls.margin
    }

    Rectangle {
        id: panelButton
        visible: window.gridView && libraryLayout.opacity > 0 && !setupGuide.active
        opacity: libraryLayout.opacity
        // Next to the first button, a spacing away like the buttons are from each other, unless the
        // controls have a panel of their own, which it then stays outside of.
        x: (windowControls.visible ? windowControls.x + (Theme.windowControls.background.a === 0 ? windowControls.padding : 0)
                                   : window.width - Theme.windowControls.margin) - width - Theme.windowControls.spacing
        y: windowControls.y + (windowControls.height - height) / 2
        width: Theme.windowControls.size
        height: Theme.windowControls.size
        radius: Theme.windowControls.radius
        color: panelMouse.containsMouse ? Theme.windowControls.hoverBackground : Qt.alpha(Theme.windowControls.hoverBackground, 0)
        Behavior on color { enabled: Motion.fades; MotionColor {} }

        Icon {
            anchors.centerIn: parent
            path: Icons.layoutSidebarRight
            size: Theme.windowControls.iconSize
            strokeWidth: Theme.windowControls.iconStroke
            color: Theme.windowControls.icon
        }
        MouseArea {
            id: panelMouse
            anchors.fill: parent
            enabled: !window.gamePageOpen
            hoverEnabled: true
            onClicked: window.run("toggle_grid_panel")
        }
        ToolTip.visible: panelMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: Actions.tip(General.gridPanel ? qsTr("Hide panel") : qsTr("Show panel"), Actions.keys.toggle_grid_panel)
    }

    WindowFrame {}
}
