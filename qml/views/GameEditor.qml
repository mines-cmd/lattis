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

    signal saved(string id)
    signal closed()
    signal message(string text)
    signal selectRequested(string id)
    signal shortcutFound(string text)
    signal removed(string next)

    function openFor(id) {
        const g = Library.gameForEditing(id)
        if (g.id === undefined)
            return
        adding = false
        pick = ({})
        show(g)
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
        form.scrollRoom = false
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
        lookupIds = { igdb: candidate.igdb }
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

    function browseArt(kind) {
        if (!Metadata.hasSteamGridDbKey)
            return
        if (steamGridDbId() > 0) {
            artBrowser.active = true
            artBrowser.item.openFor(steamGridDbId(), titleField.text.trim(), kind, {})
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
            artBrowser.item.openWithoutGame(lookupRow.title, browseKind)
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
            titleField.forceActiveFocus()
            return
        }
        const played = lastPlayedEditor.value()
        if (!onSteam && (played === undefined || played > new Date())) {
            error.text = played === undefined ? qsTr("Last played needs a day, a month and a four-digit year, or none of them")
                                              : qsTr("Last played can't be in the future")
            lastPlayedEditor.edit()
            return
        }
        if (adding && platformId === "") {
            error.text = qsTr("Choose the game's platform")
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
        }
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
        height: Math.min(implicitHeight, parent.height * Theme.gameEditor.maxHeight)
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

                GridLayout {
                    id: form

                    readonly property bool twoColumns: width >= Theme.gameEditor.twoColumnsMinWidth

                    // Kept once the scroll bar has shown: content that only overflows at the narrower
                    // width would otherwise hide the bar, widen, fit, and show it again, without end.
                    property bool scrollRoom: false
                    Connections {
                        target: scrollBar
                        function onVisibleChanged() { if (scrollBar.visible) form.scrollRoom = true }
                    }

                    width: scroll.availableWidth - (scrollRoom || scrollBar.visible ? scrollBar.width + Theme.layout.spacing : 0)
                    columns: twoColumns ? 2 : 1
                    columnSpacing: Theme.gameEditor.columnSpacing
                    rowSpacing: Theme.gameEditor.rowSpacing

                    TapHandler { onTapped: content.forceActiveFocus() }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.fillHeight: false // as tall as its cards; layouts fill by default
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.gameEditor.cardSpacing

                        Card {
                            title: qsTr("About")
                            actions: TextAction {
                                text: lookupRow.phase === "running" ? qsTr("Cancel lookup") : qsTr("Look up in IGDB")
                                iconPath: Icons.search
                                clickable: Metadata.hasIgdbKeys && titleField.text.trim() !== ""
                                tip: qsTr("Fill in the title, description and year from IGDB, and add the tags that fit")
                                disabledTip: Metadata.hasIgdbKeys ? "" : qsTr("Add your API keys in Settings → Metadata")
                                onClicked: root.lookUp()
                            }

                            Field {
                                label: qsTr("Title")
                                marked: root.fromLookup("title", titleField.text)

                                SettingsTextField {
                                    id: titleField
                                    Layout.fillWidth: true
                                    onAccepted: root.save()
                                }
                            }

                            Field {
                                label: qsTr("Description")
                                marked: root.fromLookup("description", descriptionArea.text)

                                ScrollView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Theme.gameEditor.descriptionHeight

                                    TextArea {
                                        id: descriptionArea
                                        wrapMode: TextEdit.Wrap
                                        selectByMouse: true
                                        color: Theme.controls.inputText
                                        placeholderText: qsTr("A paragraph or two about the game")
                                        placeholderTextColor: Theme.controls.placeholderColor
                                        // Tab moves on, as in the other fields, instead of typing a tab.
                                        KeyNavigation.tab: yearField
                                        KeyNavigation.backtab: titleField
                                        HoverHandler { id: descriptionPointer }
                                        ContextMenu.menu: TextContextMenu { field: descriptionArea; pointer: descriptionPointer }
                                        background: Rectangle {
                                            radius: Theme.controls.inputRadius
                                            color: Theme.controls.inputBackground
                                            border.color: descriptionArea.activeFocus ? Theme.controls.inputBorderFocus
                                                                                      : Theme.controls.inputBorder
                                            border.width: descriptionArea.activeFocus ? Theme.controls.inputBorderFocusWidth
                                                                                      : Theme.controls.inputBorderWidth
                                        }
                                    }
                                }
                            }
                        }

                        Card {
                            title: qsTr("Details")

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 3
                                columnSpacing: Theme.gameEditor.detailsColumnSpacing
                                rowSpacing: Theme.gameEditor.rowSpacing

                                Field {
                                    Layout.fillWidth: false
                                    label: qsTr("Year")
                                    marked: root.fromLookup("year", yearField.text)

                                    SettingsTextField {
                                        id: yearField
                                        Layout.preferredWidth: Theme.gameEditor.shortFieldWidth
                                        placeholderText: qsTr("Unknown")
                                        validator: IntValidator { bottom: 1; top: 9999 }
                                        onAccepted: root.save()
                                    }
                                }
                                Field {
                                    Layout.fillWidth: false
                                    label: qsTr("Rating")
                                    labelTip: qsTr("Double-click the stars to clear the rating")
                                    marked: root.fromLookup("rating", root.ratingText(rating.value))
                                        || (!root.adding && root.game.ratingSource === "igdb" && rating.value === root.game.rating)

                                    RatingEditor {
                                        id: rating
                                        Layout.preferredHeight: yearField.implicitHeight
                                    }
                                }
                                Field {
                                    label: qsTr("Favorite")

                                    SettingsSwitch {
                                        id: favoriteSwitch
                                        Layout.preferredHeight: yearField.implicitHeight
                                    }
                                }

                                Field {
                                    Layout.fillWidth: false
                                    label: qsTr("Play time")
                                    marked: root.onSteam
                                    markerText: qsTr("from Steam")

                                    RowLayout {
                                        spacing: Theme.layout.spacing
                                        enabled: !root.onSteam
                                        opacity: root.onSteam ? Theme.gameEditor.readOnlyOpacity : 1

                                        StepField {
                                            id: hoursField
                                            minDigits: 4
                                            placeholderText: "0"
                                            // The most that still fits play_minutes with 59 minutes.
                                            validator: IntValidator { bottom: 0; top: 35791393 }
                                            onStepped: direction => root.setPlayMinutes(root.playMinutes() + 60 * direction)
                                            onEditingFinished: root.setPlayMinutes(root.playMinutes())
                                            onAccepted: root.save()
                                        }
                                        Label {
                                            text: qsTr("h")
                                            color: Theme.gameEditor.labelColor
                                        }
                                        StepField {
                                            id: minutesField
                                            Layout.leftMargin: Theme.layout.spacing
                                            placeholderText: "00"
                                            validator: IntValidator { bottom: 0; top: 59 }
                                            onStepped: direction => root.setPlayMinutes(root.playMinutes() + direction)
                                            onEditingFinished: root.setPlayMinutes(root.playMinutes())
                                            onAccepted: root.save()
                                        }
                                        Label {
                                            text: qsTr("m")
                                            color: Theme.gameEditor.labelColor
                                        }
                                    }
                                }
                                Field {
                                    Layout.fillWidth: false
                                    label: qsTr("Last played")
                                    marked: root.onSteam
                                    markerText: qsTr("from Steam")

                                    DateEditor {
                                        id: lastPlayedEditor
                                        enabled: !root.onSteam
                                        opacity: root.onSteam ? Theme.gameEditor.readOnlyOpacity : 1
                                        Layout.preferredHeight: yearField.implicitHeight
                                        onAccepted: root.save()
                                    }
                                }
                                Field {
                                    label: qsTr("Status")

                                    SettingsComboBox {
                                        id: statusBox
                                        Layout.preferredHeight: yearField.implicitHeight
                                        Layout.fillWidth: true
                                        model: root.statuses.map(s => s.name)
                                    }
                                }
                            }
                        }

                        Card {
                            title: qsTr("Tags")
                            actions: TextAction {
                                text: qsTr("New tag")
                                iconPath: Icons.add
                                tip: qsTr("Create a tag and add it to this game")
                                onClicked: tagsEditor.newTag()
                            }

                            TagsEditor {
                                id: tagsEditor
                                Layout.fillWidth: true
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.gameEditor.cardSpacing

                        Card {
                            title: root.romGame ? qsTr("Platform and ROM") : qsTr("Platform and install")

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.gameEditor.columnSpacing

                                Field {
                                    Layout.fillWidth: false
                                    Layout.preferredWidth: platformBox.visible ? Theme.gameEditor.platformFieldWidth : -1
                                    Layout.alignment: Qt.AlignTop
                                    label: qsTr("Platform")

                                    RowLayout {
                                        visible: !platformBox.visible
                                        Layout.fillWidth: true
                                        Layout.minimumHeight: Theme.controls.inputHeight
                                        spacing: Theme.layout.spacing

                                        Icon {
                                            path: Icons.forPlatform(root.adding ? (root.platformChoices[platformBox.currentIndex]?.icon ?? "")
                                                                                : (root.game.platformIcon ?? ""))
                                            size: Theme.controls.iconSize
                                            color: Theme.controls.inputText
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: root.adding ? (root.platformChoices[platformBox.currentIndex]?.name ?? "")
                                                              : (root.game.platformName ?? "")
                                            color: Theme.controls.inputText
                                            font.pixelSize: Theme.gameEditor.platformFontSize
                                            elide: Text.ElideRight
                                        }
                                    }
                                    SettingsComboBox {
                                        id: platformBox
                                        visible: root.adding && root.pick.platforms.length !== 1
                                        Layout.fillWidth: true
                                        model: root.platformChoices.map(c => c.name)
                                        displayText: currentIndex < 0 ? qsTr("Choose a platform") : currentText
                                        onActivated: {
                                            error.text = ""
                                            emulatorBox.currentIndex = 0
                                        }
                                    }
                                }

                                Field {
                                    visible: root.romGame
                                    Layout.alignment: Qt.AlignTop
                                    label: qsTr("ROM")

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumHeight: Theme.controls.inputHeight
                                        spacing: Theme.layout.spacing

                                        PathField {
                                            onCopied: root.message(qsTr("Copied"))
                                            Layout.fillWidth: true
                                            path: root.romShown
                                            missingText: qsTr("ROM not found")
                                            missingColor: Theme.gameEditor.warningColor
                                            fontSize: Theme.gameEditor.pathFontSize
                                        }
                                        SettingsButton {
                                            visible: !root.adding && ((root.game.romKey ?? false) || root.locating)
                                            text: root.locating ? qsTr("Locate") : qsTr("Change")
                                            onClicked: romPicker.active = true
                                        }
                                    }
                                }

                                Field {
                                    visible: !root.romGame
                                    Layout.alignment: Qt.AlignTop
                                    label: qsTr("Install location")
                                    help: root.programGame
                                          ? qsTr("The game's whole folder, with the program somewhere inside. Open folder "
                                                 + "opens it, and the game's size is this folder's.")
                                          : qsTr("Where Steam installed the game.")

                                    PathField {
                                        id: installField
                                        onCopied: root.message(qsTr("Copied"))
                                        Layout.fillWidth: true
                                        Layout.minimumHeight: Theme.controls.inputHeight
                                        path: root.newInstallDir !== "" ? root.newInstallDir : (root.game.installPath ?? "")
                                        missingText: root.programGame ? qsTr("Not set") : qsTr("Not installed")
                                        missingColor: Theme.gameEditor.hintColor
                                        fontSize: Theme.gameEditor.pathFontSize

                                        SettingsButton {
                                            visible: !root.adding && installField.path !== "" && root.newInstallDir === ""
                                            iconPath: Icons.folder
                                            tip: qsTr("Open the install folder")
                                            onClicked: root.openFolder()
                                        }
                                        SettingsButton {
                                            visible: root.programGame
                                            iconPath: Icons.edit
                                            tip: qsTr("Change the install folder")
                                            onClicked: folderPicker.pickFor("install")
                                        }
                                    }
                                }
                            }

                            Field {
                                visible: root.programGame
                                label: qsTr("Program")
                                help: root.linuxGame
                                      ? qsTr("The program or script the launcher runs, with the launch options below. "
                                             + "It counts as running until every process it started has ended.")
                                      : qsTr("The .exe Steam runs, the target of the game's shortcut in Steam. Changing it "
                                             + "here changes the shortcut, and the other way around.")

                                PathField {
                                    onCopied: root.message(qsTr("Copied"))
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: Theme.controls.inputHeight
                                    path: root.exeShown
                                    missingText: qsTr("Not set")
                                    missingColor: Theme.gameEditor.warningColor
                                    fontSize: Theme.gameEditor.pathFontSize

                                    SettingsButton {
                                        visible: !root.adding
                                        readonly property bool locating: root.newExe === ""
                                            && ((root.game.gameNotFound ?? false) || (root.game.uninstalled ?? false))
                                        text: locating ? qsTr("Locate") : ""
                                        iconPath: locating ? "" : Icons.edit
                                        tip: locating ? "" : qsTr("Change the program")
                                        onClicked: exePicker.active = true
                                    }
                                }
                                Hint {
                                    open: root.newExe === "" && (root.game.gameNotFound ?? false)
                                    color: Theme.gameEditor.warningColor
                                    text: root.linuxGame
                                          ? qsTr("Game not found: there's no program there, or it isn't executable. "
                                                 + "Locate it if the game moved; picking it makes it executable.")
                                          : qsTr("Game not found: there's no program there, so Steam can't start it. "
                                                 + "Locate it if the game moved.")
                                }
                                Hint {
                                    open: root.newExe === "" && (root.game.uninstalled ?? false)
                                    text: root.linuxGame
                                          ? qsTr("Uninstalled: its files were deleted. Locating the program brings it "
                                                 + "back, with the same play time.")
                                          : qsTr("Uninstalled: it has no shortcut in Steam. Locating the program adds one "
                                                 + "again, with the same prefix and play time.")
                                }
                                SettingsCheckBox {
                                    id: makeExecutableBox
                                    visible: root.addingLinux && !(root.pick.executable ?? true)
                                    text: qsTr("Make it executable")
                                    ToolTip.visible: hovered
                                    ToolTip.delay: Theme.tooltip.delay
                                    ToolTip.text: qsTr("It isn't marked as a program yet (chmod +x); without that, "
                                                       + "it can't be launched")
                                }
                                TextAction {
                                    visible: root.startDirShown === ""
                                    text: qsTr("Start in another folder")
                                    iconPath: Icons.folder
                                    tip: root.linuxGame ? qsTr("The folder the program starts in: by default, its own")
                                                        : qsTr("Steam's \"Start in\": by default, the program's own folder")
                                    onClicked: folderPicker.pickFor("start")
                                }
                            }
                            Field {
                                visible: root.programGame && root.startDirShown !== ""
                                label: qsTr("Start in")

                                PathField {
                                    onCopied: root.message(qsTr("Copied"))
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: Theme.controls.inputHeight
                                    path: root.startDirShown
                                    fontSize: Theme.gameEditor.pathFontSize

                                    SettingsButton {
                                        iconPath: Icons.edit
                                        tip: qsTr("Change the start folder")
                                        onClicked: folderPicker.pickFor("start")
                                    }
                                    SettingsButton {
                                        iconPath: Icons.close
                                        tip: qsTr("Start in the program's own folder")
                                        onClicked: {
                                            root.newStartDir = ""
                                            root.startDirCleared = true
                                        }
                                    }
                                }
                            }
                            Field {
                                visible: !root.adding
                                label: qsTr("Save folder")
                                help: qsTr("The folder Open save folder opens. When it isn't set, it's looked for: next to "
                                           + "the ROM for an emulator that saves there, the emulator's save folder, or the "
                                           + "game's Proton prefix.")

                                PathField {
                                    onCopied: root.message(qsTr("Copied"))
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: Theme.controls.inputHeight
                                    path: root.saveFolderShown !== "" ? root.saveFolderShown : root.foundSaveFolder
                                    missingText: qsTr("None found")
                                    missingColor: Theme.gameEditor.hintColor
                                    fontSize: Theme.gameEditor.pathFontSize

                                    SettingsButton {
                                        iconPath: Icons.edit
                                        tip: qsTr("Choose the save folder")
                                        onClicked: folderPicker.pickFor("saves")
                                    }
                                    SettingsButton {
                                        visible: root.saveFolderShown !== ""
                                        iconPath: Icons.close
                                        tip: qsTr("Look for the save folder")
                                        onClicked: {
                                            root.newSaveFolder = ""
                                            root.saveFolderCleared = true
                                        }
                                    }
                                }
                                Hint {
                                    open: root.saveFolderShown === "" && root.foundSaveFolder !== ""
                                    text: qsTr("Found for you. Choose another folder to open that one instead.")
                                }
                            }
                            Hint {
                                open: root.adding && root.pick.platforms.length === 0
                                text: qsTr("None of your platforms' emulators run %1 files. They take:")
                                          .arg(root.pick.fileName?.includes(".") ? "." + root.pick.fileName.split(".").pop()
                                                                                   : root.pick.fileName ?? "")
                                      + root.platformChoices.map(c => "\n" + c.name + ": " + c.takes).join("")
                            }
                            Hint {
                                open: root.newRomOwner !== null
                                text: qsTr("This file is also %1 in your library. Saving merges it into this game: its play "
                                           + "time, favorite, tags and missing art come over, and it's removed.")
                                          .arg(root.newRomOwner?.title ?? "")
                            }
                        }

                        Card {
                            title: qsTr("Launch")
                            visible: root.romGame

                            Field {
                                label: qsTr("Emulator")

                                SettingsComboBox {
                                    id: emulatorBox
                                    Layout.fillWidth: true
                                    model: [qsTr("Platform default (%1)").arg(root.emulators.defaultEmulator || qsTr("none"))]
                                           .concat(root.emulators.choices.map(c => c.installed ? c.name
                                                                                               : qsTr("%1 (not installed)").arg(c.name)))
                                }
                                Hint {
                                    readonly property string before: root.emulators.choices.find(c => c.id === root.game.emulator)?.name
                                                                     ?? root.emulators.defaultEmulator
                                    readonly property string after: root.emulator?.name ?? root.emulators.defaultEmulator
                                    open: (root.game.playMinutes ?? 0) > 0 && before !== after
                                    color: Theme.gameEditor.warningColor
                                    text: qsTr("Saves stay in each emulator's own folders; your progress in %1 won't be in %2.")
                                              .arg(before).arg(after)
                                }
                            }

                            Field {
                                label: qsTr("Launch arguments")

                                SettingsTextField {
                                    id: argsField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("None")
                                    onAccepted: root.save()
                                }
                                Hint {
                                    readonly property var shown: root.emulator ?? root.emulators.choices.find(
                                                                     c => c.name === root.emulators.defaultEmulator) ?? null
                                    open: shown !== null
                                    text: shown === null ? ""
                                        : shown.args.length > 0 ? qsTr("Added after %1's own: %2").arg(shown.name).arg(Args.join(shown.args))
                                        : qsTr("%1 has no arguments of its own").arg(shown.name)
                                }
                            }
                        }

                        Card {
                            title: qsTr("Launch")
                            visible: !root.romGame

                            Field {
                                visible: root.onSteam
                                label: qsTr("Proton version")
                                help: root.protonGame
                                      ? qsTr("The compatibility tool Steam runs the program with, as in its Properties. "
                                             + "None for a Linux program.")
                                      : qsTr("The compatibility tool that runs the game, as in Steam's Properties.")

                                SettingsComboBox {
                                    id: protonBox
                                    Layout.fillWidth: true
                                    model: root.protonChoices.map(c => c.displayName)
                                }
                            }

                            Field {
                                visible: (root.game.steamPresets ?? []).length > 0
                                label: qsTr("Presets")

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: Theme.gameEditor.presetSpacing

                                    Repeater {
                                        model: root.game.steamPresets ?? []

                                        SettingsCheckBox {
                                            id: presetBox
                                            required property var modelData
                                            text: modelData.name
                                            checked: Library.steamPresetOn(launchOptionsField.text, modelData.id)
                                            ToolTip.visible: hovered
                                            ToolTip.delay: Theme.tooltip.delay
                                            ToolTip.text: qsTr("Adds %1").arg(modelData.adds)
                                            onToggled: {
                                                launchOptionsField.text = Library.setSteamPreset(launchOptionsField.text,
                                                                                                 modelData.id, checked)
                                                // Clicking broke the binding; the text says what's on.
                                                checked = Qt.binding(() => Library.steamPresetOn(launchOptionsField.text,
                                                                                                 presetBox.modelData.id))
                                            }
                                        }
                                    }
                                }
                            }

                            Field {
                                label: qsTr("Launch options")

                                SettingsTextField {
                                    id: launchOptionsField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("None")
                                    onAccepted: root.save()
                                }
                                Hint {
                                    text: root.linuxGame
                                          ? qsTr("Written like Steam's: variables, then a wrapper before %command%, "
                                                 + "arguments after it, e.g. mangohud %command% --fullscreen")
                                          : qsTr("Same as Steam's launch options, e.g. mangohud %command%")
                                }
                            }

                            Unfold {
                                open: root.onSteam && (root.adding ? root.steamRunning
                                                                   : root.steamPending || (root.steamEdited && root.steamRunning))
                                visible: open || progress > 0
                                Layout.fillWidth: true

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.layout.spacing

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.adding ? qsTr("Steam is running: the game is added to it when it closes")
                                            : root.steamEdited ? qsTr("Steam is running: saving applies this when it closes")
                                            : root.shortcutQueued ? qsTr("Waiting for Steam to close to add this game to it")
                                            : qsTr("Waiting for Steam to close to apply this")
                                        color: Theme.gameEditor.pendingColor
                                        font.pixelSize: Theme.gameEditor.hintFontSize
                                        wrapMode: Text.WordWrap
                                    }
                                    SettingsButton {
                                        text: GameRunner.closingSteam ? qsTr("Closing Steam") : qsTr("Close Steam and apply")
                                        enabled: !GameRunner.closingSteam
                                        tip: qsTr("Saves, closes Steam, writes the changes and starts Steam again")
                                        onClicked: root.closeSteamAndApply()
                                    }
                                }
                            }
                        }

                        Card {
                            title: qsTr("Images")
                            actions: TextAction {
                                text: qsTr("Browse SteamGridDB")
                                iconPath: Icons.image
                                clickable: Metadata.hasSteamGridDbKey
                                tip: qsTr("Pick a hero, cover, logo and icon from SteamGridDB and download them together")
                                disabledTip: qsTr("Add your SteamGridDB key in Settings → Metadata")
                                onClicked: root.chooseArt()
                            }

                            ArtEditor {
                                id: artEditor
                                Layout.fillWidth: true
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !root.adding
                                text: qsTr("Right-click the logo on the game's page to resize or move it.")
                                color: Theme.gameEditor.hintColor
                                wrapMode: Text.WordWrap
                            }
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
                 : qsTr("The folder %1 starts in").arg(root.game.title ?? "")
            onPicked: path => {
                if (folderPicker.target === "install") {
                    root.newInstallDir = path
                } else if (folderPicker.target === "saves") {
                    root.newSaveFolder = path
                    root.saveFolderCleared = false
                } else {
                    root.newStartDir = path
                    root.startDirCleared = false
                }
            }
            onClosed: folderPicker.active = false
        }
    }

    component Field: ColumnLayout {
        id: field
        property alias label: name.text
        property bool marked: false
        property string markerText: qsTr("from IGDB")
        property string help
        property string labelTip

        Layout.fillWidth: true
        spacing: Theme.gameEditor.labelSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                id: name
                Layout.fillWidth: !marker.visible
                color: Theme.gameEditor.labelColor
                elide: Text.ElideRight

                MouseArea {
                    id: nameMouse
                    anchors.fill: parent
                    enabled: field.labelTip !== ""
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
                ToolTip.visible: nameMouse.containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: field.labelTip
            }
            Label {
                id: marker
                visible: field.marked
                Layout.fillWidth: true
                text: field.markerText
                color: Theme.gameEditor.markerColor
                font.pixelSize: Theme.gameEditor.markerFontSize
                elide: Text.ElideRight
            }
            // A control's own hover, like the buttons' tooltips: a HoverHandler's could stay
            // hovered after the tooltip opened.
            AbstractButton {
                id: help
                visible: field.help !== ""
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                background: null
                contentItem: Icon {
                    path: Icons.help
                    size: name.font.pixelSize
                    color: help.hovered ? Theme.gameEditor.helpHoverColor : Theme.gameEditor.helpColor
                }

                ToolTip {
                    visible: help.hovered
                    delay: Theme.tooltip.delay
                    text: field.help
                    width: Math.min(implicitWidth, Theme.gameEditor.helpWidth)
                    // Its right edge at the ?, above it. The default placement centers it by its
                    // unwrapped width, far to the left for a long text.
                    x: parent.width - width
                    y: -height - 4
                }
            }
        }
    }
    component TextAction: Item {
        id: action
        property alias text: actionLabel.text
        property alias iconPath: actionIcon.path
        property string tip
        property string disabledTip
        // Not `enabled`: the item stays enabled, so its tooltip shows while the button is disabled.
        property alias clickable: actionButton.enabled
        signal clicked()

        implicitWidth: actionButton.implicitWidth
        implicitHeight: actionButton.implicitHeight
        HoverHandler { id: actionHover }
        ToolTip.visible: actionHover.hovered && ToolTip.text !== ""
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: actionButton.enabled ? tip : disabledTip

        AbstractButton {
            id: actionButton
            anchors.fill: parent
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            opacity: enabled ? 1 : 0.45
            onClicked: action.clicked()

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            background: null
            contentItem: Row {
                spacing: 4

                Icon {
                    id: actionIcon
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.gameEditor.cardActionFontSize + 3
                    color: actionLabel.color
                }
                Label {
                    id: actionLabel
                    anchors.verticalCenter: parent.verticalCenter
                    color: actionButton.hovered || actionButton.visualFocus
                           ? Theme.gameEditor.cardActionHoverColor : Theme.gameEditor.cardActionColor
                    font.pixelSize: Theme.gameEditor.cardActionFontSize
                }
            }
        }
    }
    component Card: ColumnLayout {
        property alias title: heading.text
        property alias actions: actionsSlot.data
        default property alias fields: cardColumn.data

        Layout.fillWidth: true
        Layout.fillHeight: false
        spacing: Theme.gameEditor.cardTitleSpacing

        RowLayout {
            visible: heading.text !== ""
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                id: heading
                Layout.fillWidth: true
                color: Theme.gameEditor.cardTitleColor
                font.pixelSize: Theme.gameEditor.cardTitleFontSize
                font.weight: Theme.font.strongWeight
            }
            Item {
                id: actionsSlot
                implicitWidth: childrenRect.width
                implicitHeight: childrenRect.height
            }
        }
        Surface {
            Layout.fillWidth: true
            implicitHeight: cardColumn.implicitHeight + 2 * Theme.gameEditor.cardPadding
            radius: Theme.gameEditor.cardRadius
            fill: Theme.gameEditor.cardBackground
            fillEnd: Theme.gameEditor.cardBackgroundEnd
            direction: Theme.gameEditor.gradientDirection
            border.color: Theme.gameEditor.cardBorder
            border.width: Theme.gameEditor.cardBorderWidth

            ColumnLayout {
                id: cardColumn
                anchors.fill: parent
                anchors.margins: Theme.gameEditor.cardPadding
                spacing: Theme.gameEditor.rowSpacing
            }
        }
    }
    // A note under a field; `open` unfolds it, so one that appears while the editor is open
    // doesn't push what's below it down at once.
    component Hint: Unfold {
        property alias text: note.text
        property alias color: note.color

        open: true
        visible: open || progress > 0
        Layout.fillWidth: true

        Label {
            id: note
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
        }
    }
}
