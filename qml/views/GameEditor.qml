import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components/Args.js" as Args

// See docs/code/qml.md.
Item {
    id: root

    readonly property bool isOpen: dialog.visible
    property var game: ({})
    property bool adding: false
    property var pick: ({})
    readonly property bool addingProgram: adding && (pick.proton ?? false)
    readonly property bool steamGame: !adding && game.platform === "steam"
    readonly property bool protonGame: adding ? addingProgram : game.platform === "proton"
    readonly property bool onSteam: steamGame || protonGame
    readonly property bool addingLinux: adding && (pick.linux ?? false)
    readonly property bool linuxGame: adding ? addingLinux : game.platform === "linux"
    readonly property bool programGame: protonGame || linuxGame
    readonly property var platformChoices: !adding ? [] : pick.platforms.length > 0 ? pick.choices.filter(c => c.runs)
                                                                                    : pick.choices
    readonly property string platformId: adding ? (platformBox.currentIndex >= 0
                                                   ? platformChoices[platformBox.currentIndex].id : "")
                                                : (game.platform ?? "")
    readonly property var statuses: [
        { id: "", name: qsTr("No status") },
        { id: "not_started", name: qsTr("Backlog") },
        { id: "playing", name: qsTr("Playing") },
        { id: "finished", name: qsTr("Finished") },
        { id: "completed", name: qsTr("Completed") },
        { id: "dropped", name: qsTr("Dropped") }
    ]
    property string newRom: ""
    readonly property var newRomOwner: {
        if (newRom === "")
            return null
        const pick = Library.inspectRom(newRom)
        if (pick.existing === "" || pick.existing === game.id)
            return null
        return { id: pick.existing, title: Library.gameForEditing(pick.existing).title ?? pick.fileName }
    }
    readonly property string romShown: adding ? (pick.path ?? "")
        : newRom !== "" ? decodeURIComponent(newRom.replace(/^file:\/\//, ""))
        : (game.romPath || (game.romKey ? (game.romWritten ?? "") : ""))
    readonly property bool locating: !adding && !(game.romKey ?? false) && (game.romPath ?? "") === ""

    readonly property bool romGame: (adding && !addingProgram && !addingLinux) || (game.rom ?? false)
    readonly property var emulators: adding ? Library.emulatorChoices(platformId)
                                            : { choices: game.emulators ?? [], defaultEmulator: game.defaultEmulator ?? "" }
    readonly property string emulatorId: emulatorBox.currentIndex > 0
                                         ? emulators.choices[emulatorBox.currentIndex - 1].id : ""
    readonly property var emulator: emulators.choices.find(c => c.id === emulatorId) ?? null

    property string newExe: ""
    property string newStartDir: ""
    property bool startDirCleared: false
    property string newInstallDir: ""
    property string newSaveFolder: ""
    property bool saveFolderCleared: false
    readonly property string saveFolderShown: saveFolderCleared ? "" : newSaveFolder !== "" ? newSaveFolder
                                                                    : (game.saveFolder ?? "")
    property string foundSaveFolder: ""
    property var newMediaFolders: null
    readonly property var mediaFoldersShown: newMediaFolders ?? game.mediaFolders ?? []
    readonly property bool mediaGame: !adding && (onSteam || linuxGame)
    readonly property bool backupGame: !adding && (onSteam || linuxGame)
    readonly property string exeShown: newExe !== "" ? newExe : (game.exePath || game.exe || "")
    readonly property string startDirShown: startDirCleared ? "" : newStartDir !== "" ? newStartDir : (game.startDir ?? "")
    property bool shortcutQueued: false

    readonly property var protonChoices: {
        const tools = game.protonTools ?? []
        const none = protonGame ? qsTr("None (runs natively)")
                   : game.protonDefault ? qsTr("Steam's default (%1)").arg(game.protonDefault)
                   : qsTr("Steam's default")
        const out = [{ name: "", displayName: none }].concat(tools)
        const current = game.proton ?? ""
        if (current !== "" && !tools.some(t => t.name === current))
            out.push({ name: current, displayName: qsTr("%1 (not installed)").arg(current) })
        return out
    }
    property bool steamPending: false
    readonly property bool steamEdited: onSteam
        && (launchOptionsField.text.trim() !== (game.launchOptions ?? "").trim()
            || (protonChoices[protonBox.currentIndex]?.name ?? "") !== (game.proton ?? "")
            || (protonGame && (titleField.text.trim() !== game.title || newExe !== ""
                               || startDirShown !== (game.startDir ?? ""))))
    property bool steamRunning: false
    onSteamEditedChanged: if (steamEdited) steamRunning = GameRunner.steamRunning()

    property int lookupRequest: 0
    property int artRequest: 0
    property int browseRequest: 0
    property string browseKind
    property bool lookupPicks: false
    property var lookupCandidates: []
    property var lookupMatch: null
    property var lookupValues: ({})
    property var lookupBefore: ({})
    property var lookupFilled: ({})
    property var lookupDiffers: []
    property var lookupOpened: ({})
    property string detailedSearched
    property var lookupIds: ({})
    readonly property var fieldNames: ({ title: qsTr("Title"), description: qsTr("Description"), year: qsTr("Year"),
                                          rating: qsTr("Rating") })

    // The tabs' controls, read by show(), changes() and save().
    readonly property alias titleField: generalPage.titleField
    readonly property alias descriptionArea: generalPage.descriptionArea
    readonly property alias yearField: generalPage.yearField
    readonly property alias rating: generalPage.rating
    readonly property alias favoriteSwitch: generalPage.favoriteSwitch
    readonly property alias hoursField: generalPage.hoursField
    readonly property alias minutesField: generalPage.minutesField
    readonly property alias lastPlayedEditor: generalPage.lastPlayedEditor
    readonly property alias statusBox: generalPage.statusBox
    readonly property alias tagsEditor: generalPage.tagsEditor
    readonly property alias platformBox: launchPage.platformBox
    readonly property alias emulatorBox: launchPage.emulatorBox
    readonly property alias argsField: launchPage.argsField
    readonly property alias protonBox: launchPage.protonBox
    readonly property alias launchOptionsField: launchPage.launchOptionsField
    readonly property alias makeExecutableBox: launchPage.makeExecutableBox
    readonly property alias autoBackupBox: savesPage.autoBackupBox
    readonly property alias artEditor: imagesPage.artEditor
    readonly property alias mediaMenu: imagesPage.mediaMenu

    // What the tabs reach in this file.
    readonly property alias lookupStatus: lookupRow
    readonly property alias errorLabel: error
    readonly property alias folderPickerLoader: folderPicker
    readonly property alias romPickerLoader: romPicker
    readonly property alias exePickerLoader: exePicker
    readonly property alias managerMenuItem: managerMenu

    signal saved(string id)
    signal closed()
    signal message(string text)
    signal selectRequested(string id)
    signal shortcutFound(string text)
    signal removed(string next)
    signal addModManagerRequested(string gameId)

    readonly property var saves: GameSaves.state.requested === game.id ? GameSaves.state : ({})
    readonly property var backups: saves.backups ?? []
    readonly property string homePath: StandardPaths.writableLocation(StandardPaths.HomeLocation).toString().replace(/^file:\/\//, "")

    function openSaves() {
        savesDialog.active = true
        savesDialog.item.game = game
        savesDialog.item.open()
    }

    function chooseSavesGame() {
        savesPicker.active = true
        const picker = savesPicker.item
        picker.gameId = game.id
        picker.gameTitle = game.title ?? ""
        picker.chosen = saves.match === "chosen" ? (saves.matched ?? "") : ""
        picker.open()
    }

    Loader {
        id: savesDialog
        active: false
        source: "SavesDialog.qml"
        onLoaded: {
            item.message.connect(text => root.message(text))
            item.chooseRequested.connect(() => root.chooseSavesGame())
        }
    }

    Loader {
        id: savesPicker
        active: false
        source: "SavesGamePicker.qml"
        onLoaded: item.message.connect(text => root.message(text))
    }

    // Links ticked or unticked here (manager id → linked), written with Save.
    property var managerLinks: ({})
    readonly property bool managerGame: !adding && ["steam", "proton", "linux"].includes(game.platform ?? "")
    readonly property var linkedManagers: managerGame ? Library.modManagers.filter(m => managerLinked(m)) : []
    readonly property var unlinkedManagers: managerGame ? Library.modManagers.filter(m => !managerLinked(m)) : []
    function managerLinked(m) {
        return managerLinks[m.id] ?? m.games.some(g => g.id === game.id)
    }
    function setManagerLinked(id, linked) {
        const next = Object.assign({}, managerLinks)
        next[id] = linked
        managerLinks = next
    }
    function saveManagerLinks() {
        const errors = []
        for (const m of Library.modManagers) {
            if (managerLinks[m.id] === undefined || managerLinks[m.id] === m.games.some(g => g.id === game.id))
                continue
            const ids = m.games.map(g => g.id).filter(id => id !== game.id)
            if (managerLinks[m.id])
                ids.push(game.id)
            const e = Library.setModManagerValue(m.id, "games", ids)
            if (e !== "")
                errors.push(e)
        }
        managerLinks = ({})
        return errors
    }

    // 0 General, 1 Images, 2 Launch, 3 Saves & mods. Adding a game has no tabs: General and Launch show together.
    property int tab: 0
    readonly property bool savesTabUsed: !adding
    readonly property var tabs: [
        { name: qsTr("General"), shown: true },
        { name: qsTr("Images"), shown: true },
        { name: qsTr("Launch"), shown: true },
        { name: managerGame ? qsTr("Saves & mods") : qsTr("Saves"), shown: savesTabUsed }
    ]

    function pageShown(n) {
        return adding ? (n === 0 || n === 2) : tab === n
    }

    function openFor(id, tabIndex) {
        const g = Library.gameForEditing(id)
        if (g.id === undefined)
            return
        adding = false
        pick = ({})
        tab = tabIndex ?? 0
        show(g)
    }

    function openMediaFolders() {
        tab = 1
        mediaMenu.open()
    }

    function openForFile(file) {
        const p = Library.inspectRom(file)
        if (p.existing !== "") {
            selectRequested(p.existing)
            message(qsTr("%1 is already in your library").arg(p.fileName))
            return
        }
        if (p.problem !== "") {
            message(p.problem)
            return
        }
        if ((p.shortcut ?? 0) > 0) {
            shortcutFound(qsTr("%1 is already a shortcut in Steam; bring it in with Add existing shortcuts").arg(p.fileName))
            return
        }
        adding = true
        tab = 0
        pick = p
        platformBox.currentIndex = p.platforms.length > 0 ? 0 : -1
        const blank = { title: p.title, description: "", year: 0, rating: -1, favorite: false, status: "", tags: [],
                        emulator: "", args: [], art: {}, playMinutes: 0 }
        if (addingProgram)
            Object.assign(blank, {
                platform: "proton", exe: p.path, installPath: p.installDir, proton: p.protonVersion,
                launchOptions: p.launchOptions, protonTools: p.protonTools, protonDefault: p.protonDefault,
                steamPresets: p.steamPresets })
        else if (addingLinux)
            Object.assign(blank, {
                platform: "linux", exe: p.path, installPath: p.installDir, launchOptions: p.launchOptions,
                steamPresets: p.steamPresets })
        makeExecutableBox.checked = true
        show(blank)
    }

    function show(g) {
        game = g
        newRom = ""
        newExe = ""
        newStartDir = ""
        startDirCleared = false
        newInstallDir = ""
        newSaveFolder = ""
        saveFolderCleared = false
        newMediaFolders = null
        managerLinks = ({})
        mediaMenu.close()
        form.scrollRoom = false
        if (!tabs[tab]?.shown)
            tab = 0
        foundSaveFolder = g.id !== undefined && !adding ? GameRunner.foundSaveFolder(g.id) : ""
        titleField.text = g.title
        descriptionArea.text = g.description
        yearField.text = g.year > 0 ? String(g.year) : ""
        rating.value = g.rating
        favoriteSwitch.checked = g.favorite
        setPlayMinutes(g.playMinutes)
        lastPlayedEditor.set(g.lastPlayed)
        statusBox.currentIndex = Math.max(0, statuses.findIndex(s => s.id === g.status))
        tagsEditor.reset(g.tags)
        emulatorBox.currentIndex = Math.max(0, emulators.choices.findIndex(c => c.id === g.emulator) + 1)
        autoBackupBox.currentIndex = g.autoBackup === 1 ? 1 : g.autoBackup === 0 ? 2 : 0
        if (backupGame)
            GameSaves.load(g.id)
        argsField.text = Args.join(g.args)
        protonBox.currentIndex = Math.max(0, protonChoices.findIndex(c => c.name === (g.proton ?? "")))
        launchOptionsField.text = g.launchOptions ?? ""
        steamPending = g.pendingSteamEdits ?? false
        shortcutQueued = g.shortcutQueued ?? false
        steamRunning = onSteam && GameRunner.steamRunning()
        artEditor.reset(g.art)
        error.text = ""
        resetLookup()
        lookupOpened = { title: titleField.text, description: descriptionArea.text, year: yearField.text,
                         rating: fieldText("rating") }
        Metadata.reloadKeys()
        dialog.open()
        if (adding) {
            titleField.forceActiveFocus()
            titleField.selectAll()
        } else {
            content.forceActiveFocus()
        }
    }

    function playMinutes() {
        return (parseInt(hoursField.text) || 0) * 60 + (parseInt(minutesField.text) || 0)
    }

    function setPlayMinutes(minutes) {
        const total = Math.max(0, Math.min(35791393 * 60 + 59, minutes))
        hoursField.text = total > 0 ? String(Math.floor(total / 60)) : ""
        minutesField.text = total > 0 ? String(total % 60).padStart(2, "0") : ""
    }

    function ratingText(value) {
        return value >= 0 ? String(value) : ""
    }
    function fieldText(key) {
        if (key === "rating")
            return ratingText(rating.value)
        return { title: titleField, description: descriptionArea, year: yearField }[key].text
    }
    function setFieldText(key, text) {
        if (key === "rating") {
            rating.value = text === "" ? -1 : Number(text)
            return
        }
        ({ title: titleField, description: descriptionArea, year: yearField })[key].text = text
    }
    function fromLookup(key, text) {
        return lookupFilled[key] !== undefined && text === lookupFilled[key]
    }

    function resetLookup() {
        Metadata.cancel(lookupRequest)
        Metadata.cancel(artRequest)
        Metadata.cancel(browseRequest)
        lookupRequest = 0
        artRequest = 0
        browseRequest = 0
        lookupPicks = false
        lookupCandidates = []
        lookupMatch = null
        lookupValues = {}
        lookupBefore = {}
        lookupFilled = {}
        lookupDiffers = []
        lookupIds = {}
        tagsEditor.marked = []
        detailedSearched = ""
        lookupRow.phase = ""
    }

    function lookUp(search) {
        if (lookupRow.phase === "running") {
            Metadata.cancel(lookupRequest)
            Metadata.cancel(browseRequest)
            lookupRequest = 0
            browseRequest = 0
            lookupRow.phase = lookupMatch !== null ? "matched" : ""
            return
        }
        const title = (lookupBefore.title ?? titleField.text).trim()
        if (title === "" || !Metadata.hasIgdbKeys)
            return
        lookupPicks = search === true
        const remembered = lookupIds.igdb ?? game.igdb ?? -1
        lookupRow.title = title
        lookupRow.phase = "running"
        lookupRequest = Metadata.lookup(title, Library.igdbPlatformIds(platformId), search === true ? 0 : Math.max(0, remembered),
                                        game.steamAppId ?? 0)
    }

    function openDetailedLookup(name, everyPlatform) {
        const platforms = Library.igdbPlatformIds(platformId)
        const searched = name || detailedSearched || lookupRow.title || (lookupBefore.title ?? titleField.text).trim()
        const foundNothing = lookupRow.phase === "hint" && lookupCandidates.length === 0
        detailedLookup.active = true
        detailedLookup.item.openFor(titleField.text.trim(), platforms, searched, foundNothing || everyPlatform === true)
    }

    function useDetailedLookup() {
        if (detailedLookup.item)
            detailedLookup.item.use(detailedLookup.item.selected)
    }

    function detailedChosen(candidate, searched) {
        const title = (lookupBefore.title ?? titleField.text).trim()
        lookupPicks = false
        lookupCandidates = []
        detailedSearched = searched
        applyMatch(candidate, title !== "" && searched.toLowerCase() !== title.toLowerCase())
    }

    function lookupFinished(result) {
        lookupRequest = 0
        if (result.error !== "") {
            lookupRow.hint = result.error
            lookupRow.phase = lookupMatch !== null ? "matched" : "hint"
            if (lookupMatch !== null)
                message(result.error)
            return
        }
        lookupCandidates = result.candidates
        if (result.candidates.length === 0) {
            lookupRow.hint = (Library.igdbPlatformIds(platformId).length > 0
                              ? qsTr("IGDB has nothing called %1 on this platform. Check the title, or search another name or platform with Detailed lookup.")
                              : qsTr("IGDB has nothing called %1. Check the title, or search another name with Detailed lookup.")).arg(lookupRow.title)
            lookupRow.phase = "hint"
        } else if (result.match >= 0 && !lookupPicks) {
            applyMatch(result.candidates[result.match])
        } else {
            lookupRow.phase = "pick"
        }
    }

    function applyMatch(candidate, keepTitle) {
        undoFill()
        const values = { title: candidate.name, description: candidate.summary, year: candidate.year > 0 ? String(candidate.year) : "",
                         rating: ratingText(candidate.rating ?? -1) }
        const before = {}
        const filled = {}
        const differs = []
        for (const key of ["title", "description", "year", "rating"]) {
            const text = fieldText(key)
            if (values[key] === "" || text.trim() === values[key])
                continue
            if (text === lookupOpened[key] && (adding || text.trim() === "") && !(key === "title" && keepTitle === true)) {
                before[key] = text
                setFieldText(key, values[key])
                filled[key] = values[key]
            } else {
                differs.push(key)
            }
        }
        lookupValues = values
        lookupBefore = before
        lookupFilled = filled
        lookupDiffers = differs
        lookupMatch = candidate
        lookupIds = steamGame ? { igdb: candidate.igdb } : { igdb: candidate.igdb, steam: candidate.steamAppId || 0 }
        tagsEditor.addSuggested(Library.igdbTags(candidate.facets ?? [], tagsEditor.names()))
        lookupRow.phase = "matched"
        if (Metadata.hasSteamGridDbKey)
            artRequest = Metadata.findArt(candidate.name, candidate.year, candidate.steamAppId || (game.steamAppId ?? 0))
    }

    function replaceToo() {
        const before = Object.assign({}, lookupBefore)
        const filled = Object.assign({}, lookupFilled)
        for (const key of lookupDiffers) {
            before[key] = fieldText(key)
            setFieldText(key, lookupValues[key])
            filled[key] = lookupValues[key]
        }
        lookupBefore = before
        lookupFilled = filled
        lookupDiffers = []
    }

    function undoFill() {
        tagsEditor.removeSuggested()
        for (const key of Object.keys(lookupBefore)) {
            if (fromLookup(key, fieldText(key)))
                setFieldText(key, lookupBefore[key])
        }
        Metadata.cancel(artRequest)
        artRequest = 0
        lookupMatch = null
        lookupBefore = {}
        lookupFilled = {}
        lookupDiffers = []
        lookupIds = {}
    }

    function steamGridDbId() {
        return lookupIds.steamgriddb ?? Math.max(0, game.steamgriddb ?? 0)
    }

    function steamAppForArt() {
        if (steamGame)
            return Library.steamAppFor(game.id)
        return (lookupIds.steam ?? 0) > 0 ? lookupIds.steam : Math.max(0, game.steamApp ?? 0)
    }

    function browseArt(kind) {
        if (!Metadata.hasSteamGridDbKey)
            return
        if (!adding)
            tab = 1
        if (steamGridDbId() > 0) {
            artBrowser.active = true
            artBrowser.item.openFor(steamGridDbId(), titleField.text.trim(), kind, {}, steamAppForArt())
            return
        }
        const title = titleField.text.trim()
        if (title === "")
            return
        browseKind = kind
        lookupRow.title = title
        lookupRow.phase = "running"
        browseRequest = Metadata.findArt(title, parseInt(yearField.text) || 0, game.steamAppId ?? 0)
    }

    function chooseArt() {
        browseArt(["cover", "hero", "logo", "icon"].find(kind => artEditor.shown(kind) === "") ?? "cover")
    }

    function browseFound(result) {
        browseRequest = 0
        lookupRow.phase = lookupMatch !== null ? "matched" : ""
        if (result.error !== "") {
            lookupRow.hint = result.error
            lookupRow.phase = "hint"
            return
        }
        if (result.steamgriddb <= 0) {
            artBrowser.active = true
            artBrowser.item.openWithoutGame(lookupRow.title, browseKind, steamAppForArt())
            return
        }
        lookupIds = Object.assign({}, lookupIds, { steamgriddb: result.steamgriddb })
        browseArt(browseKind)
    }

    Connections {
        target: Metadata
        function onFinished(request, result) {
            if (request === root.lookupRequest && request !== 0) {
                root.lookupFinished(result)
            } else if (request === root.browseRequest && request !== 0) {
                root.browseFound(result)
            } else if (request === root.artRequest && request !== 0) {
                root.artRequest = 0
                if (result.error === "" && result.steamgriddb > 0 && root.lookupMatch !== null)
                    root.lookupIds = Object.assign({}, root.lookupIds, { steamgriddb: result.steamgriddb })
            }
        }
    }

    Connections {
        target: Library
        enabled: dialog.visible && root.onSteam
        function onGamesChanged() {
            const g = Library.gameForEditing(root.game.id)
            root.steamPending = g.pendingSteamEdits ?? false
            root.shortcutQueued = g.shortcutQueued ?? false
            root.steamRunning = GameRunner.steamRunning()
        }
    }

    function changes() {
        const out = {}
        if (titleField.text.trim() !== game.title)
            out.title = titleField.text.trim()
        if (descriptionArea.text.trim() !== game.description)
            out.description = descriptionArea.text.trim()
        const year = yearField.text === "" ? 0 : parseInt(yearField.text)
        if (year !== game.year)
            out.year = year > 0 ? year : null
        // Writing a rating clears where it came from, so a lookup's is kept by saying so again.
        const ratingFromIgdb = fromLookup("rating", ratingText(rating.value))
        if (rating.value !== game.rating) {
            out.rating = rating.value >= 0 ? rating.value : null
            if (ratingFromIgdb)
                out.rating_source = "igdb"
        } else if (ratingFromIgdb && game.ratingSource !== "igdb") {
            out.rating_source = "igdb"
        }
        if (favoriteSwitch.checked !== game.favorite)
            out.favorite = favoriteSwitch.checked
        if (!onSteam) {
            const minutes = playMinutes()
            if (minutes !== game.playMinutes)
                out.play_minutes = minutes > 0 ? minutes : null
            // A new day keeps the time of day it had (noon if it had none); the same day changes nothing.
            const played = lastPlayedEditor.value()
            const before = isNaN(game.lastPlayed) ? null : game.lastPlayed
            if (played === null && before !== null)
                out.last_played = null
            else if (played !== null && (before === null || played.toDateString() !== before.toDateString()))
                out.last_played = new Date(played.getFullYear(), played.getMonth(), played.getDate(),
                                           before?.getHours() ?? 12, before?.getMinutes() ?? 0)
        }
        const status = statuses[statusBox.currentIndex].id
        if (status !== game.status)
            out.status = status
        const tags = tagsEditor.names()
        if (tags.join("\n") !== game.tags.map(t => t.name).join("\n"))
            out.tags = tags
        for (const key of ["igdb", "steamgriddb"]) {
            if (lookupIds[key] !== undefined && lookupIds[key] !== (game[key] ?? -1))
                out[key] = lookupIds[key]
        }
        if (!steamGame && (lookupIds.steam ?? 0) > 0 && lookupIds.steam !== (game.steamApp ?? 0))
            out.steam_app = lookupIds.steam
        if (romGame) {
            if (emulatorId !== game.emulator)
                out.emulator = emulatorId
            const args = Args.split(argsField.text)
            if (Args.join(args) !== Args.join(game.args))
                out.args = args
            if (newRom !== "")
                out.rom = newRom
        }
        if (onSteam || linuxGame) {
            // Compared trimmed: Steam's own can start with a space (" mangohud %command%"), which
            // isn't a change to write back.
            const options = launchOptionsField.text.trim()
            if (options !== (game.launchOptions ?? "").trim())
                out.launch_options = options
        }
        if (onSteam) {
            const proton = protonChoices[protonBox.currentIndex]?.name ?? ""
            if (proton !== (game.proton ?? ""))
                out.proton = proton
        }
        if (programGame) {
            if (newExe !== "") {
                out.exe = newExe
                if (game.uninstalled)
                    out.installed = null
            }
            if (startDirShown !== (game.startDir ?? ""))
                out.start_dir = startDirShown !== "" ? startDirShown : null
            if (newInstallDir !== "")
                out.install_dir = newInstallDir
        }
        if (!adding && saveFolderShown !== (game.saveFolder ?? ""))
            out.save_folder = saveFolderShown !== "" ? saveFolderShown : null
        if (backupGame) {
            const choice = [-1, 1, 0][autoBackupBox.currentIndex]
            if (choice !== (game.autoBackup ?? -1))
                out.auto_backup = choice < 0 ? null : choice === 1
        }
        if (mediaGame && newMediaFolders !== null)
            out.media_folders = newMediaFolders
        return out
    }

    function closeSteamAndApply() {
        save()
        if (dialog.opened) // it said why it didn't save
            return
        const e = GameRunner.closeSteamAndApply()
        if (e !== "")
            message(e)
    }

    function save() {
        if (titleField.text.trim() === "") {
            error.text = qsTr("A game needs a title")
            tab = 0
            titleField.forceActiveFocus()
            return
        }
        const played = lastPlayedEditor.value()
        if (!onSteam && (played === undefined || played > new Date())) {
            error.text = played === undefined ? qsTr("Last played needs a day, a month and a four-digit year, or none of them")
                                              : qsTr("Last played can't be in the future")
            tab = 0
            lastPlayedEditor.edit()
            return
        }
        if (adding && platformId === "") {
            error.text = qsTr("Choose the game's platform")
            tab = 2
            platformBox.forceActiveFocus()
            return
        }
        // New tags and colors first, so the game's tags have their colors when it's saved.
        const errors = tagsEditor.apply()
        const values = changes()
        let id = game.id
        if (adding) {
            delete values.title // given on its own; it makes the id
            if (addingProgram) {
                values.launch_options = launchOptionsField.text.trim()
                values.proton = protonChoices[protonBox.currentIndex]?.name ?? ""
            } else if (addingLinux) {
                values.launch_options = launchOptionsField.text.trim()
                if (!pick.executable)
                    values.makeExecutable = makeExecutableBox.checked
            }
            const added = Library.addGame(pick.path, platformId, titleField.text.trim(), values, artEditor.picked())
            id = added.id
            if (added.error !== "")
                errors.push(added.error)
            if (id === "") {
                error.text = errors.join("\n")
                return
            }
            // Added. Anything else that went wrong (art) keeps the dialog open to say so,
            // now editing the new game.
            adding = false
            game = Library.gameForEditing(id)
            artEditor.reset(game.art)
            tagsEditor.reset(game.tags)
        } else {
            // Art first: the Save's Undo is taken when saveGame() finishes, and covers the art.
            Library.beginEditUndo(id)
            errors.push(...artEditor.apply(id))
            const e = Library.saveGame(id, values)
            if (e !== "") {
                error.text = errors.concat([e]).join("\n")
                return
            }
            errors.push(...saveManagerLinks())
        }
        if (values.auto_backup === true)
            SavesSettings.ensureManifest()
        saved(id)
        error.text = errors.join("\n")
        if (errors.length === 0)
            dialog.close()
    }

    function openFolder() {
        const e = GameRunner.openFolder(game.id)
        if (e !== "")
            message(e)
    }

    function unhide() {
        const e = Library.unhideGame(game.id)
        if (e !== "") {
            error.text = e
            return
        }
        dialog.close()
    }

    function nextGame() {
        const id = game.id
        const row = Library.rowOfGame(id)
        let next = Library.gameIdAt(Library.nextGameRow(row, 1))
        if (next === "" || next === id)
            next = Library.gameIdAt(Library.nextGameRow(row, -1))
        return next === id ? "" : next
    }

    function uninstall() {
        if (game.platform === "steam") {
            confirmUninstall.ask(game)
            return
        }
        protonUninstall.active = true
        protonUninstall.next = nextGame()
        protonUninstall.item.ask(game)
    }

    function remove(wipe) {
        const next = nextGame()
        const e = Library.removeGame(game.id, wipe)
        if (e !== "") {
            error.text = e
            return
        }
        dialog.close()
        removed(next)
    }

    MotionPopup {
        id: dialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Theme.gameEditor.width, parent.width - 2 * Theme.layout.margin)
        height: Math.min(root.adding ? implicitHeight : Theme.gameEditor.height, parent.height * Theme.gameEditor.maxHeight)
        modal: true
        focus: true
        padding: Theme.gameEditor.padding
        closePolicy: Popup.CloseOnEscape
        onClosed: {
            confirmRemove.close()
            confirmUninstall.close()
            if (protonUninstall.item)
                protonUninstall.item.close()
            root.resetLookup()
            Metadata.discardDownloads()
            root.closed()
        }

        Overlay.modal: Scrim {}

        background: Surface {
            radius: Theme.gameEditor.radius
            fill: Theme.gameEditor.background
            fillEnd: Theme.gameEditor.backgroundEnd
            direction: Theme.gameEditor.gradientDirection
            border.color: Theme.gameEditor.border
            border.width: Theme.gameEditor.borderWidth
        }

        contentItem: ColumnLayout {
            id: content
            spacing: Theme.gameEditor.rowSpacing

            // A click on no field takes the focus from the one that has it. The form needs its
            // own handler: the ScrollView takes the clicks inside it.
            TapHandler { onTapped: content.forceActiveFocus() }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    text: root.adding ? qsTr("Add a game") : qsTr("Properties")
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                    font.pixelSize: Theme.gameEditor.headingFontSize
                    elide: Text.ElideRight
                }
                WindowControls {
                    Layout.alignment: Qt.AlignTop
                    dialog: dialog
                }
            }

            LookupRow {
                id: lookupRow
                Layout.fillWidth: true
                match: root.lookupMatch
                candidates: root.lookupCandidates
                differs: root.lookupDiffers.map(key => root.fieldNames[key])
                filledAny: Object.keys(root.lookupFilled).length > 0 || tagsEditor.marked.length > 0
                canChooseArt: Metadata.hasSteamGridDbKey
                onChooseArt: root.chooseArt()
                onCancel: root.lookUp()
                onUndo: {
                    root.undoFill()
                    phase = ""
                }
                onNotThisGame: {
                    if (root.lookupCandidates.length > 1) {
                        phase = "pick"
                    } else {
                        root.undoFill()
                        root.lookUp(true)
                    }
                }
                onReplaceToo: root.replaceToo()
                onPicked: candidate => root.applyMatch(candidate)
                onNoneOfThese: {
                    root.undoFill()
                    phase = ""
                }
                onNotOnIgdb: {
                    root.undoFill()
                    root.lookupIds = { igdb: 0 }
                    hint = qsTr("Marked as not on IGDB; saving remembers it, so looking up missing info skips this game.")
                    phase = "hint"
                }
                onDetailedLookup: root.openDetailedLookup()
                onDismissed: phase = ""
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.gameEditor.railGap

                ColumnLayout {
                    id: rail
                    visible: !root.adding
                    Layout.preferredWidth: Theme.gameEditor.railWidth
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.gameEditor.railSpacing

                    Repeater {
                        model: root.tabs

                        Surface {
                            id: tabItem
                            required property var modelData
                            required property int index
                            readonly property bool selected: root.tab === index

                            visible: modelData.shown
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.gameEditor.railItemHeight
                            radius: Theme.gameEditor.railItemRadius
                            fill: selected ? Theme.gameEditor.railSelectedBackground
                                : tabMouse.containsMouse ? Theme.controls.hover
                                : Qt.alpha(Theme.controls.hover, 0)
                            fillEnd: selected ? Theme.gameEditor.railSelectedBackgroundEnd
                                   : tabMouse.containsMouse ? Theme.controls.hover
                                   : Qt.alpha(Theme.controls.hover, 0)
                            direction: Theme.controls.gradientDirection

                            Behavior on fill { enabled: Motion.fades; MotionColor {} }
                            Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                text: tabItem.modelData.name
                                color: tabItem.selected ? Theme.gameEditor.railSelectedText : Theme.gameEditor.railText
                                elide: Text.ElideRight
                                font.weight: tabItem.selected ? Theme.font.onAccentWeight : Font.Normal

                                Behavior on color { enabled: Motion.fades; MotionColor {} }
                            }

                            MouseArea {
                                id: tabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.tab = tabItem.index
                                    content.forceActiveFocus()
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    id: scroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitHeight: form.implicitHeight
                    contentWidth: availableWidth
                    clip: true

                    // Beside the cards, not over them: the form makes room while it shows, so the
                    // cards line up with the buttons below when nothing scrolls.
                    ScrollBar.vertical: SettingsScrollBar {
                        id: scrollBar
                        parent: scroll
                        x: scroll.width - width
                        y: scroll.topPadding
                        height: scroll.availableHeight
                    }

                    ColumnLayout {
                        id: form

                        // Kept once the scroll bar has shown: content that only overflows at the narrower
                        // width would otherwise hide the bar, widen, fit, and show it again, without end.
                        property bool scrollRoom: false
                        Connections {
                            target: scrollBar
                            function onVisibleChanged() { if (scrollBar.visible) form.scrollRoom = true }
                        }

                        width: scroll.availableWidth - (scrollRoom || scrollBar.visible ? scrollBar.width + Theme.layout.spacing : 0)
                        spacing: Theme.gameEditor.cardSpacing

                        TapHandler { onTapped: content.forceActiveFocus() }

                        EditorGeneralPage {
                            id: generalPage
                            editor: root
                            visible: root.pageShown(0)
                        }

                        EditorLaunchPage {
                            id: launchPage
                            editor: root
                            visible: root.pageShown(2)
                        }

                        EditorSavesPage {
                            id: savesPage
                            editor: root
                            visible: root.pageShown(3)
                        }

                        EditorImagesPage {
                            id: imagesPage
                            editor: root
                            visible: root.pageShown(1)
                        }
                    }
                }
            }

            Label {
                id: error
                visible: text !== ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.errorColor
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                SettingsButton {
                    id: removeButton
                    visible: !root.adding
                    text: (root.game.hidden ?? false) ? qsTr("Unhide") : qsTr("Hide game")
                    onClicked: {
                        if (root.game.hidden ?? false) {
                            root.unhide()
                            return
                        }
                        wipe.checked = false
                        confirmRemove.open()
                    }
                }
                SettingsButton {
                    visible: !root.adding && (root.game.platform === "steam" ? (root.game.steamUninstallable ?? false)
                                              : root.programGame)
                    text: qsTr("Uninstall")
                    danger: true
                    onClicked: root.uninstall()
                }
                Item { Layout.fillWidth: true }
                SettingsButton {
                    text: qsTr("Cancel")
                    onClicked: dialog.close()
                }
                SettingsButton {
                    text: root.adding ? qsTr("Add") : qsTr("Save")
                    primary: true
                    enabled: !root.adding || root.platformId !== ""
                    onClicked: root.save()
                }
            }
        }

        MotionPopup {
            id: confirmRemove

            readonly property bool wipeDeletes: root.game.removeDeletes ?? false
            readonly property bool proton: root.game.platform === "proton"

            anchors.centerIn: parent
            width: Math.min(Theme.gameEditor.confirmWidth, dialog.width)
            modal: true
            padding: Theme.gameEditor.padding
            Overlay.modal: Rectangle { color: "transparent" }
            background: Surface {
                radius: Theme.gameEditor.radius
                fill: Theme.gameEditor.background
                fillEnd: Theme.gameEditor.backgroundEnd
                direction: Theme.gameEditor.gradientDirection
                border.color: Theme.gameEditor.border
                border.width: Theme.gameEditor.borderWidth
            }

            contentItem: ColumnLayout {
                spacing: Theme.gameEditor.rowSpacing

                Label {
                    text: qsTr("Hide %1?").arg(root.game.title ?? "")
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.gameEditor.labelColor
                    text: confirmRemove.proton
                          ? qsTr("It leaves the game list until you unhide it in Settings → Library. It stays in "
                                 + "Steam. To remove it or its files, use Uninstall.")
                          : qsTr("It leaves the game list until you unhide it in Settings → Library.")
                }
                AbstractButton {
                    id: wipe
                    visible: !confirmRemove.proton
                    Layout.fillWidth: true
                    checkable: true
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    contentItem: RowLayout {
                        spacing: Theme.layout.spacing

                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            implicitWidth: Theme.gameEditor.checkSize
                            implicitHeight: Theme.gameEditor.checkSize
                            radius: Theme.controls.inputRadius / 2
                            color: wipe.checked ? Theme.gameEditor.checkOn : Theme.controls.inputBackground
                            border.color: wipe.checked ? Theme.gameEditor.checkOn : Theme.controls.inputBorder
                            border.width: Theme.controls.inputBorderWidth
                            Icon {
                                anchors.centerIn: parent
                                visible: wipe.checked
                                path: Icons.check
                                size: parent.width - 4
                                color: Theme.gameEditor.checkMark
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Also delete its info and art")
                                color: Theme.gameEditor.headingColor
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: confirmRemove.wipeDeletes
                                      ? qsTr("Its game file and images are deleted for good: it can't be unhidden. The game's own files stay.")
                                      : qsTr("Play time, description, tags, rating and images go; unhiding brings it back fresh. The game's own files stay.")
                                color: Theme.gameEditor.hintColor
                                font.pixelSize: Theme.gameEditor.hintFontSize
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    Item { Layout.fillWidth: true }
                    SettingsButton {
                        text: qsTr("Cancel")
                        onClicked: confirmRemove.close()
                    }
                    SettingsButton {
                        text: !wipe.checked ? qsTr("Hide") : confirmRemove.wipeDeletes ? qsTr("Delete") : qsTr("Hide and delete")
                        primary: !wipe.checked
                        danger: wipe.checked
                        onClicked: {
                            confirmRemove.close()
                            root.remove(wipe.checked)
                        }
                    }
                }
            }
        }
    }

    ModManagerMenu {
        id: managerMenu
        managers: root.unlinkedManagers
        popDirection: "right"
        onChosen: id => root.setManagerLinked(id, true)
    }

    SteamUninstallPrompt {
        id: confirmUninstall
        onFailed: text => error.text = text
        onDone: dialog.close()
    }

    Loader {
        id: protonUninstall
        property string next: ""
        active: false
        sourceComponent: ProtonUninstall {
            onDone: removedGame => {
                dialog.close()
                if (removedGame)
                    root.removed(protonUninstall.next)
            }
        }
    }

    Loader {
        id: artBrowser
        active: false
        sourceComponent: ArtBrowser {
            onGamePicked: id => root.lookupIds = Object.assign({}, root.lookupIds, { steamgriddb: id })
            onSteamAppFound: id => root.lookupIds = Object.assign({}, root.lookupIds, { steam: id })
            onChosen: files => {
                for (const kind of Object.keys(files))
                    artEditor.set(kind, files[kind])
            }
        }
    }

    Loader {
        id: detailedLookup
        active: false
        sourceComponent: DetailedLookup {
            onChosen: (candidate, searched) => root.detailedChosen(candidate, searched)
        }
    }

    Loader {
        id: romPicker
        active: false
        sourceComponent: RomFileDialog {
            title: (root.locating ? qsTr("Locate the ROM of %1") : qsTr("Change the ROM of %1"))
                       .arg(root.game.title ?? "")
            onPicked: file => root.newRom = file
            onClosed: romPicker.active = false
        }
    }

    Loader {
        id: exePicker
        active: false
        sourceComponent: ProgramDialog {
            title: qsTr("The program of %1").arg(root.game.title ?? "")
            nameFilters: root.linuxGame
                         ? [qsTr("Linux programs (*.sh *.x86_64 *.x86 *.AppImage *.appimage *.bin)"), qsTr("All files (*)")]
                         : [qsTr("Windows programs (*.exe)"), qsTr("All files (*)")]
            onPicked: path => root.newExe = path
            onClosed: exePicker.active = false
        }
    }

    Loader {
        id: folderPicker
        property string target
        function pickFor(what) {
            target = what
            active = true
        }
        active: false
        sourceComponent: RomFolderDialog {
            title: folderPicker.target === "install" ? qsTr("The install folder of %1").arg(root.game.title ?? "")
                 : folderPicker.target === "saves" ? qsTr("The save folder of %1").arg(root.game.title ?? "")
                 : folderPicker.target === "media" ? qsTr("A media folder of %1").arg(root.game.title ?? "")
                 : qsTr("The folder %1 starts in").arg(root.game.title ?? "")
            onPicked: path => {
                if (folderPicker.target === "install") {
                    root.newInstallDir = path
                } else if (folderPicker.target === "saves") {
                    root.newSaveFolder = path
                    root.saveFolderCleared = false
                } else if (folderPicker.target === "media") {
                    if (!root.mediaFoldersShown.includes(path))
                        root.newMediaFolders = root.mediaFoldersShown.concat([path])
                } else {
                    root.newStartDir = path
                    root.startDirCleared = false
                }
            }
            onClosed: folderPicker.active = false
        }
    }
}
