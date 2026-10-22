# The library (`src/library/`)

The library code turns the files in the data dir into the game list the user sees, and turns the user's edits back into those files. It owns everything about games themselves: where they come from, how they're merged, which are installed, how the list is sorted and filtered, and how a game is added, edited, hidden or uninstalled. Launching lives in `src/launch/`, the Steam file formats in `src/steam/` and metadata lookups in `src/metadata/`; they all work through the library.

What the files hold is described for users in the guide's [data folder](../guide/data-folder.md) page, and every key in the `reference.toml` files the app writes. This page is about the code.

## Contents

- [Where games come from](#where-games-come-from)
- [From files to the list](#from-files-to-the-list)
- [Writing changes](#writing-changes)
- [Undo](#undo)
- [The game list](#the-game-list)
- [ROM folders](#rom-folders)
- [Emulators](#emulators)
- [Steam, Proton and Linux games](#steam-proton-and-linux-games)
- [Play history](#play-history)
- [Tags and art](#tags-and-art)
- [Uninstalling](#uninstalling)
- [Format versions](#format-versions)
- [Files](#files)
- [QML reference](#qml-reference)

## Where games come from

A game is identified by an **id**, a lowercase ASCII slug like `switch-metroid-dread` or `steam-1145360`. The id names its game file (`library/games/<id>.toml`) and its art folder (`art/<id>/`), so it's restricted to `a-z`, `0-9` and `-`. That way it matches on case-insensitive FAT/exFAT drives and looks the same to every program. An id never changes once it's given, even when the game is renamed (`gameid.*`).

A game can be known from two kinds of file, and is usually known from both:

- **Source indexes** are written by the app as it discovers games. `library/roms/<platform>.toml` lists the ROMs a folder scan found, keyed by path. `library/steam.toml` lists the Steam games the import found, keyed by app ID. An index entry holds only what discovery knows: an id and a title (and for ROMs a Switch title ID and a size).
- **Game files** (`library/games/<id>.toml`) hold everything the user or a lookup decided: description, tags, favorite, play time, launch options, art choices. For an indexed game the file holds only these changes, and deleting it simply forgets them. A game added by hand with "+" (a ROM outside the ROM folders, a Windows program, a Linux program) has no index entry, so its file also says what the game is: its `rom` or `exe`, title and platform.

`buildLibrary()` merges the two by id, the file's values winning over the entry's. Either one alone is a game. The result is a list of `Game`s: what the UI shows, with platform names, tag colors and art URLs resolved, and with what no file says filled in from the system (whether it's installed, its size, Steam's play time).

**Platforms** come from `library/platforms.toml`, in the order the rail shows them. A platform names its emulators in order of preference and its ROM folders. Its `id` is permanent once games use it; its `name` is free to change. Games on a platform that isn't defined there still show, under their platform id. The built-in platforms `steam`, `proton` and `linux` are added to the file when they're first needed (`LibraryStore::addBuiltInPlatforms()`), ahead of the emulators' platforms and in that order: each goes right after the last built-in that comes before it, or first when there is none, so a built-in the user moved keeps the later ones next to it.

## From files to the list

```
 library/*.toml ─┐
 art/<id>/       ├─▶ scanLibrary() ──▶ LibrarySnapshot ──▶ buildLibrary() ──▶ Library (list model) ──▶ QML
 ROM folders     │   (worker thread)    │      ▲
 emulators       │                      ▼      │
 Steam           ┘                    LibraryCache (~/.cache)
```

Three classes carry the library at run time:

- **`LibrarySnapshot`** (`librarycache.h`) is everything parsed, at one point in time: the last good value of every library file with its modification time and size, the art found, the last ROM scan, the emulator installs, and what the Steam import read.
- **`LibraryStore`** (`librarystore.*`) owns the snapshot. It scans, writes, watches the files and keeps the cache.
- **`Library`** (`library.*`) is the QML singleton. It asks the store for the snapshot, builds the games from it, and presents them as a list model with grouping, sorting and filters, plus everything the settings pages show about the library.

### Startup

Startup must reach the first frame in about 110–200 ms, so nothing is parsed before it. `LibraryStore`'s constructor loads the binary snapshot from the cache on the main thread, and `Library` builds its rows from it right away. Only after the first frame does `Library::startRefresh()` start the first scan.

The cache (`librarycache.*`) is disposable. It's keyed to the data dir, and anything unexpected (a missing or unreadable file, another format version, another Qt version, another data dir) means no cache, and the library is parsed from scratch. On a first start without a cache, `Library.loading` is true and the list stays empty until the first scan ends. Bump `LibraryCache::kFormatVersion` whenever what's stored, or how, changes.

Before anything reads the library, `LibraryVersion::prepare()` checks its format version (see [Format versions](#format-versions)).

### Scanning

`scanLibrary()` brings a snapshot up to date. It runs on a worker thread and touches no model, only files:

1. It lists every library file with its stamp and compares them with the snapshot's (`LibraryCache::diff()`). Only the new and changed files are parsed. A warm start with nothing changed parses nothing; `filesParsed()` lets the tests check that.
2. It lists the art folders, one directory listing per game rather than a stat per possible file name.
3. On a snapshot's first scan, when `platforms.toml` or `emulators.toml` changed, or when the user asks ("Scan now"), it looks for emulator installs and scans the ROM folders.
4. It runs the Steam import, which re-reads only the Steam files that changed, and the Steam sync.

A file that no longer parses (the user is halfway through editing it) keeps its last good value, with the error among its warnings, and is marked `ok = false`. The app never writes to a file in that state.

The scan doesn't write library files itself. What needs writing (new ROM index entries, new Steam games, values the sync took from Steam) comes back in the `ScanResult`, and the store writes it on the main thread when the scan finishes. The one exception is Steam's art, which the scan copies into `art/<id>/` under a hidden name and then renames, so another scan never sees half a file.

### Watching

After the first scan the store watches the library and art folders and rescans when they change. Editors save either by writing in place or by replacing the file, and one save can arrive as several events, so the store waits for them to settle. Steam's files and `settings.toml` have their own watcher with a longer wait, because Steam writes several files at once.

### Outdated scans

The user can change something while a scan is running. Every change made on the main thread bumps the store's generation counter. When a scan finishes with an older generation, its result is out of date and is thrown away, and another scan runs. The one thing kept from an outdated result is the list of grid files it wrote into Steam, because the sync record has to know about those.

## Writing changes

Every write goes through `LibraryStore`, and every method that writes returns an error message to show, or an empty string when it worked. Writes follow the same pattern:

1. **Patch, don't rewrite.** Files are edited in place with `TomlPatch` (`src/core/tomlpatch.*`), which changes only the keys involved and keeps the user's comments and formatting byte for byte.
2. **Read it straight back.** The written file goes into the snapshot at once with its new stamp, so the UI shows the change immediately and the watcher's next scan sees the file as already up to date.
3. **Announce it.** `changedHere()` bumps the generation, emits `changed()` and schedules a cache save. `Library` rebuilds its rows on `changed()`.

Writing many games at once (a bulk metadata lookup, a tag rename) is wrapped in `beginBatch()` and `endBatch()`, so the list rebuilds once at the end instead of once per file.

Game file values are checked against `GameFields`, the table in `gamefile.cpp` that lists every valid key with its type, range and description. The validation, the writer and the user docs all follow that table. Clearing a value (an empty string, an empty list, `false`, or JS `null` from QML) removes its line rather than writing an empty value. A game file left empty for a game an index still has is deleted, since it only held changes and they're all undone. The logo's keys are `logo_scale`, `logo_position`, `logo_offset_x` and `logo_offset_y` (its size and place) and three for what darkens the art behind it: `logo_scrim` (`gradient`, `shadow` or `none`; unset follows the theme), `logo_scrim_side` (`auto`, `top`, `bottom`, `left`, `right`, `full`; unset follows the theme) and `logo_scrim_strength` (0 to 2, 1 unset). `logo_scrim` was a Bool before: a leftover `false` reads as `none` and `true` as unset. They go through `GameFile`, `Game`, the cache and `Library::gameAt()` (`logoScrim`, `logoScrimSide`, `logoScrimStrength`); the three are saved together in one `saveGame()` by Reset to theme.

Nothing is written when:

- the library is **read-only** (saved by a newer version of the app, or an upgrade failed), or
- the file has a syntax error, since the user is editing it, or
- a library Import is replacing the data dir. `hold()` stops scans and writes until the restore is done.

## Undo

`LibraryUndo` (`libraryundo.*`, owned by `Library`) keeps the last undoable change: one entry, no stack and no Redo. A change records the files it is about to touch (`start()`, `keepCopy()`, `keepMove()`), then `commit()` makes the record current, replacing the older one. A record of files, not of operations, so each change type needs no code of its own to be undone: `restore()` puts the files back byte for byte and `LibraryStore::reloadFiles()` reads them again.

- **What each change keeps:**
  - an editor Save: the game file, and `art/<id>/` when the editor called `beginEditUndo(id)` first (its Save does, then applies the art before `saveGame()`, which takes the entry; `discardEditUndo()` drops a started one);
  - deleting a game: the game file, `art/<id>/`, and for a game only its file makes also its history file, the last two moved rather than deleted;
  - deleting a tag: `tags.toml` and the file of every game that had it, plus the tag filter (`m_afterUndo`);
  - moving a platform: `platforms.toml`.
- **No Undo** when nothing the entry kept differs from what's there after the change, or when a Save changes a ROM (it may merge another game into this one) or a Linux game's program (it is made executable). `undoOffered(0, text)` says so.
- **Where:** `<state dir>/undo/<n>/data/<path in the data dir>`, set with `Library::setUndoDir()`; an empty folder turns Undo off. A folder goes when its entry is replaced, restored or expired (60 s, which outlasts the toast's 15 s so a hovered toast still works), at quit and, after the first frame, at startup (`startRefresh()`). A deleted game's art and history are really gone only then.
- **Signals:** `undoOffered(token, text)` after a change, `undoExpired(token)`. `Library.undo(token)` returns an error message (`Nothing to undo` for a replaced or expired token) or `""`.
- **Hide** keeps the game file too ("<title> is hidden"). **Not covered yet:** bulk changes (they will keep each game's file).

## The game list

`Library` is a `QAbstractListModel` whose rows are games and group headers. How the rows are built:

1. `m_games` holds every game, hidden ones too, in title order: case ignored, numbers by value ("Game 2" before "Game 10"), then by id (`titleBefore()`).
2. The filters pick the games to list. They combine, so a game is listed only when it passes every one:
   - the platform selected in the rail,
   - the search: every word must appear in the title, in any order, ignoring case, accents and punctuation (`searchtext.*`),
   - installed only (on by default),
   - favorites only,
   - tags (any, or all with "match all"),
   - statuses,
   - whether hidden games show.
3. The sort orders them: by title, last played or play time. A stable sort over the title-ordered list keeps ties, and games without the value, in title order. Games without a value always go last.
4. Grouping puts a header row above each group: one per platform (only in "All games"; with a platform selected the list is flat), one per status, or none.

A group can be collapsed. Collapsed groups are remembered per grouping until the app quits. While searching or filtering, every group shows expanded, so a match is never hidden.

The sort, the filters, the grouping and `show_sort_value` are saved in `[list]` of `settings.toml` (`listsettings.*`), and only the keys that changed are written. The file is watched, so a hand edit applies at once. A tag filter naming a tag that doesn't exist (any more) is ignored, and so is an unknown status.

When the games are reloaded from disk but the rows stay the same (a description was edited), the rows update in place so nothing moves. When a few games came or went and nothing else moved (at most six, the rest of the rows in the same order), the rows are inserted or removed, so the list can animate them. When exactly one game moved (a rename, a new platform, a sort that now puts it elsewhere) and nothing else changed, it is one `rowsMoved`. Collapsing or expanding a group (`toggleGroup`, `collapseGroupAt`, `expandGroupNear`) is one `rowsRemoved` or `rowsInserted` for the group's games, however many there are, and a `dataChanged` for its header's `collapsed`. `groupChange` (`{start, rows}`: the first changed row, and the count, negative when removed) tells the view which rows a collapse or expand just removed or inserted so it can animate them; it is empty a moment later. Any other change resets the model and emits `reloaded()`, so the view keeps its scroll position rather than jumping to the top as it does for a new search.

## ROM folders

The ROM scan (`romscan.*`) walks each platform's ROM folders, recursively. It skips hidden files and doesn't enter symlinked folders. It picks up files with one of the platform's emulators' `extensions`, or named one of their `files` (like `eboot.bin` for PS4). Only names and sizes are read, never ROM contents: one stat per file.

**Titles** come from the file name (`RomName`). The extension, bracketed groups (`[0100152000022000]`, `(USA)`, `{…}`) and underscores go, so `Mario Kart 8 Deluxe [0100152000022000][v0].nsp` becomes `Mario Kart 8 Deluxe`. Scene-style names with dots for spaces are handled too. A title in all capitals or all lowercase gets each word capitalized, keeping Roman numerals up to XXXIX in capitals; a mixed-case title is left as it is.

**Switch title IDs** tell games from updates and DLC: one ending in `000` is a base game, `800` its update, anything else DLC. Updates and DLC are counted and skipped, not added as games.

**Folder games** (PS3, PS4) are a whole folder rather than a file. They're titled after their folder, and their size is the folder's, added up once because it can be thousands of files. shadPS4 keeps a game's update in a `<game>-UPDATE` or `<game>-patch` folder next to it; those are skipped, and entries older scans made for them are dropped.

**Renames** are recognized, so a renamed ROM keeps its id, play time and edits. When an index entry's file is gone and a new file appears, the new file takes over the entry when they have the same title ID. Without a title ID, they must have the same cleaned title (only tags changed, as in `Zelda (USA).z64` → `Zelda (USA) [!].z64`) or the same size and extension, and no other new file or gone entry may match as well. The scan never guesses. That's why an entry keeps its size while its file is gone.

New files get an id from their title (`GameId::forTitle()`: `<platform>-<slug>`, with `-2`, `-3`… when taken). Ids are handed out in path order, so they're stable.

A ROM picked with "+" goes through the same name handling (`inspectRomPick()`). If it's inside one of its platform's ROM folders it gets an index entry, as the scan would give it; anywhere else it gets a game file with a `rom` key, written with `~` inside the home folder.

The setup guide's "Find from one folder" matches the subfolders of one folder to platforms by name. It uses a built-in table of the names people give ROM folders (`switch`, `3ds`, `ps2`…; `romfoldermatch.*`) and the platform's own id and name, ignoring case and punctuation.

## Emulators

An emulator is defined by a preset built into the app (`resources/emulators/<id>.toml`), by the user's `library/emulators.toml`, or both (`emulators.*`). The merge lists the presets first, then the user's own. A user block with a preset's id changes only the keys it sets. Any other user block needs a `name`, a `command`, and `extensions` or `files` (an empty `extensions = []` counts: the app writes one for a blank emulator it has just added), or it's skipped with a warning; a block that's only a mistyped preset id is caught this way. It needs no install key: until it has one it's just not found. Valid keys are listed in `EmulatorFields`, which also generates `library/emulators.reference.toml` for users.

Editing an emulator from Settings writes only what differs from the preset: setting a key back to the preset's value removes it, and a block left with just its id goes away. A preset's `path` and `args` can be edited there; a user emulator's `name`, `platforms`, `flatpak`, `path`, `extensions`, `command` and `args`. The other keys stay file-only. `extensions` is normalized on the way in (split on spaces and commas, leading dots dropped, lowercased, no duplicates), and a `command` must hold `{exe}`.

**Adding and removing** (`LibraryStore::addEmulator`, `removeEmulator`). A new emulator's id is `GameId::slug(name)`, checked against the presets and against `emulators.toml` itself rather than the snapshot. A blank one gets `command = ["{exe}", "{args}", "{rom}"]` and `extensions = []`. One started from a preset copies the preset's keys except `name`, the install keys (`flatpak`, `executables`, `appimage`, `path`: the copy would find the original's install) and `saves` and `keys` (the original's folders, which a fork renames). The new id is then appended last to the `emulators` list of each platform it runs that's written in `platforms.toml`, so it's used only where nothing earlier is installed. Removing works on user emulators only: the id leaves every platform's list first, so no platform names a missing emulator, then its block goes. Games pinned to it keep their `emulator` value and fall back to their platform's list, so adding it back restores the pin.

**Detection** (`Emulators::detect()`) looks, in order, for:

1. the user's `path`,
2. the Flatpak,
3. an executable on `PATH`,
4. an AppImage in `~/Applications` (AppImageLauncher) or Gear Lever's folder, matching the name case-insensitively and taking the newest version.

A program with the emulator's `portable` file or folder next to it keeps its data there, which changes where its saves and keys are (`installPaths()`, used by backups). Detection only looks at the file system, and installs aren't cached: they change outside the app and are cheap to find.

A **Flatpak** emulator can only read the folders its permissions allow. `flatpakFilesystems()` reads the app's permissions and applies the system and user overrides. `flatpakCanRead()` then checks each ROM folder against them, so the Platforms page can warn about a folder the emulator won't see and show the command that grants it.

Launching picks the game's pinned emulator if it's installed, else the first installed one in the platform's list (`Emulators::choose()`).

## Steam, Proton and Linux games

These three kinds of game share a lot of the library's code. Their file formats and the sync are covered in [steam.md](steam.md); here is how the library uses them.

- **Steam games** come from the Steam import during each scan. New ones are added to `steam.toml` (with ids like `steam-<appid>`), and Steam's art is copied into `art/<id>/` unless the game file's `skip_steam_art` lists that kind. A Steam game is installed only when Steam says so. Its play time and last played are Steam's and read-only. A Steam game with an update has `steamUpdate` (`SteamUpdate`), `updateBytes` and `updatePercent` (`-1` unknown), set from its manifest; for a game whose update has started, `Library` overlays what `SteamDownloads` finds (running or paused, the percentage), see [steam.md](steam.md#updates). The selected game's map has `steamUpdate` (`"none"`, `"waiting"`, `"downloading"`, `"paused"`), `steamUpdateText` (the chip's value: "Waiting · 1.2 GB", "Downloading · 45%", "Paused · 45%") and `steamUpdatePercent`; `launchTip` says what the Launch button does in each state. The list model has an `updating` role.
- **Proton games** are Windows programs run through a non-Steam shortcut in Steam. Each has a game file with its `exe`, `install_dir` and `shortcut` app ID; `protonPrefix(shortcut)` is its prefix in the first Steam library's `compatdata`, or empty. "+" with an `.exe` creates one and queues its shortcut to be added to Steam. "Add existing shortcuts" brings in Steam's own shortcuts as Proton games. Play time comes from Steam's shortcut.
- **Linux games** are native programs (an ELF, an AppImage or a script) the launcher runs directly. Each has a game file with its `exe`, and its play time is recorded like a ROM game's. Whether a file is a Linux program is decided by its content, not its name (`linuxProgramKind()`): an ELF for this computer's processor, an AppImage (marked in its ELF header), or a script starting with `#!`. A PS3 ELF, say, is not one. A program picked that isn't executable yet is made executable.

For Proton and Linux games, the **install folder** is the program's folder, climbing out of generic ones like `bin`, `Binaries/Win64` or `x86_64` (never up to the home folder or `/`): `Game/Binaries/Win64/Game.exe` → `Game`. An AppImage is the whole game and often sits in a folder of other AppImages, so its files are the AppImage alone. Each scan stats the programs, and adds up an install folder's size once, while the folder stays the same.

**Edits for Steam** (launch options, Proton versions, a Proton game's shortcut) can only be written while Steam is closed, since Steam overwrites its files on exit. The store queues them. `applySteamQueue()` writes them when Steam isn't running, and otherwise a `SteamExitWatch` writes them when Steam exits. The queue shows in the UI as `Library.steamWaiting`, and `undoSteamQueue()` drops an entry, leaving Steam as it is.

## Play history

Besides the total in the game file, the library keeps every play session, so the game's page can list them and the statistics page can add them up (`playhistory.*`). The history is separate from the game file: one file per game, `library/history/<id>.toml`, created with its first session. A game file stays small, and a hand edit of it never collides with a session being written.

```toml
[[session]]
end = 2026-10-10T22:41:00+02:00
minutes = 95

[[session]]
end = 2026-10-12T01:10:00+02:00
minutes = 240
merged = true
```

A session stores its end, with the UTC offset it was written with, and its length; the start is the end minus the length. The end is the one time every source knows, since Steam's `LastPlayed` is an end time too. `merged = true` marks an entry that may hold several sessions (see below). A history file can also carry a top-level `deleted_minutes` (below). Writes replace the whole file atomically, which at a few hundred sessions is a few KB. Nothing reads a history file at startup or during a scan: a game's file is read when its popup opens, and the statistics page reads them all on a worker thread. A file that doesn't parse is never written to, as with game files; an unreadable entry is skipped and reported but stays in the file.

**Where sessions come from** depends on who knows the game ran:

- **ROM and Linux games** are run by the launcher. `Library::recordPlaySession()` adds the minutes to `play_minutes` and sets `last_played`, as it always did, and then appends the session to the history. The sessions `GameRunner` drops (too short, quick crash) never get here.
- **Steam and Proton games** are launched through Steam, and their play time is Steam's, so the history comes from Steam's totals instead of the launcher's timer. That one rule covers a game started from Steam as well as from Lattis, and the minutes match Steam's own number. After each scan the store compares every game's `Playtime` with the last total it saw (`PlayHistory::diffSteam()`). A total that went up becomes a session ending at the game's `LastPlayed`. The last totals live in `library/history/steam-seen.toml`, by Steam user and then by game id, rewritten in one write after a scan that changed any. The file is in the data dir rather than the state dir so that a restored backup doesn't treat every game's whole play time as new.
- **A game seen for the first time** (no entry yet, which is every game on the first run) only records its total. Its earlier play time shows as "Before history". A total that goes *down* (another account's data, a reset) only updates the seen total, and so does a changed `[steam] user`, which has a section of its own.
- **Live or merged.** Steam writes `localconfig.vdf` when a game closes, so a change seen while Lattis runs is one session. The first scan after startup may find the play of several sessions made while Lattis was closed; that entry is `merged`, since its length and time mean little, and it counts on the day it ended.

**Before history** is a game's total minus its sessions, shown when positive. It covers play from before the feature, hand edits to `play_minutes` and play time Steam restored from its servers. Recording a session changes the total only as it did before, and a hand edit never touches the history.

**Deleting a session** (`Library::deleteSession()`, and `restoreSession()` for the Undo that follows) takes the first entry that ends at the given time out of the file.

- For a ROM or Linux game, the minutes also come off `play_minutes` (never below 0), so a game left running overnight can be fixed, and `last_played` moves to the newest session left. When none is left it stays as it was, since it may predate the history.
- For a Steam or Proton game the total is Steam's and read-only, so it stays. The session only leaves the history and the statistics, and its minutes are added to the file's `deleted_minutes`, so "Before history" doesn't grow to absorb them.

Removing a game removes its history file with its game file; hiding keeps it. The backup takes `library/history/` with the rest of `library/`.

**Counting days.** A session counts toward the day it started (end minus length, in local time), and a merged entry toward the day it ended. A session isn't split across midnight or weeks. Weeks start on `QLocale().firstDayOfWeek()`.

**The statistics** are computed by `PlayHistory::monthStats()`, a pure function over the games' sessions, so it runs on any thread and is tested directly. `Library::monthStats(year, month)` collects the games (hidden ones only when "Hidden games" is on in the filters), reads their histories and runs it on a worker thread, and `monthStatsReady` delivers the map. The page ignores a result for a month it has since left. Everything is for the one month and from history only, so "Before history" time is in none of it. The previous month's figures are for the whole month, or for the same days when the month is the current one, since a part month always looks like a drop against a whole one.

`PlayHistory::yearStats()` does the same for a calendar year, with `Library::yearStats(year)` and `yearStatsReady(year, stats)`. Both share `periodStats()`, which adds up a date range (platforms, the top five games, the tiles' counts, the longest session, the streak and the average session); the streak walks days across month boundaries but counts only days inside the range, so a year's streak never runs in from the year before. The year's comparison is against the previous year, clipped to the same span (1 January to today's date) for the current year. Both maps carry `historyMonths`, every month with any history, which the title picker uses to dim empty months without asking again. The month strip is the picked month's calendar year, January to December, so a clicked bar never moves.

## Tags and art

**Tags** are `[[tag]]` blocks in `library/tags.toml`, each with a name, a color (an index into the theme's tag palette) and optionally what it stands for on IGDB (`genre:<id>`, `theme:<id>`, `mode:<id>`, `keyword:<slug>`). A game's `tags` name them. Names are case-sensitive. A tag only games use, with no block, still shows, in the palette's first color. Renaming or removing a tag rewrites the `tags` key of every game that has it, and nothing else in those files. A new library starts with the default tags from `resources/tags.toml`. A default tag without its own `igdb` list uses the default's by name, which is how lookups suggest tags.

**Art** lives in `art/<id>/<kind>.<extension>`: a hero, a logo, an icon and a cover, in PNG, JPEG or WebP. Setting art copies the image after checking that it loads. An image larger than the app ever shows that kind is scaled down (heroes to 3840 px, logos to 1280 px, covers to 1800 px, icons to 512 px) on the way (`ArtImage::copy()`): SteamGridDB and Steam art can be enormous (a 13471×6421 logo), which costs memory and time at every decode. A logo also loses its transparent margins (pixels under alpha 4 count as empty, for lossy WebP's specks), trimmed before any scaling, so its size and position on the game's page follow what's drawn rather than the padding SteamGridDB and Steam logos often carry; a logo without margins, or without an alpha channel, is copied untouched. The copy goes in under a hidden name first, so a failed copy leaves the old art in place. Art URLs carry the newest file's modification time, so an image replaced under the same name loads again.

**Art that arrives another way** (a file copied in by hand, a restored backup, a folder synced from another machine, a library from before the limit) is shrunk in place all the same, so every file in `art/` ends up within the limits. The original is discarded, as it is for art set through the app. Two steps keep this off the startup path. The scan finds the files: it reads the header of each art file (`QImageReader::size()`, no decoding) and keeps, per game, an `ArtCheck` in the snapshot: the `ArtSet` it checked and the kinds over `ArtImage::maxSide()`, tall images included. Reading every header of a 1000-game library takes about 80 ms, so only folders whose `ArtSet` differs from the stored check are read again; an unchanged library costs nothing. A check counts only while its `ArtSet` equals the game's current one, which is why the places that change art by hand needn't update it. A file swapped in with an older time than the folder's newest one goes unnoticed until the folder changes some other way. The checks are cached, and the cache format changed with them, so the first start after the change checks every folder once.

Logos get trimmed the same way, but finding margins means decoding the image, which the scan can't afford. So a check only marks a folder's logo as unchecked (`ArtCheck::trimLogo`), and the queue below decodes it: `ArtImage::copy()` with its `changed` argument writes nothing for a logo with no margin to trim, and the check just settles. Each logo is decoded once per check. That covers a library's existing logos (the cache format changed with `trimLogo`, so the first start after it checks every logo once), hand-copied ones, and, harmlessly, logos the app has just set: any change to a folder's art makes a new check.

The shrinking happens in the hero copy queue, after the first frame, one file at a time on a worker thread (`LibraryStore::shrinkArt()`). For each game, nearest the selected row first, its oversized files and unchecked logo (`LibraryStore::artToShrink()`) are shrunk before its hero copy is made, so the copy is made from the shrunk file and not twice. `ArtImage::copy()` writes the result under a hidden `.<kind>.shrink` name, and `rename(2)` puts it over the original in one step, so a failure leaves the original. Several things hold a shrink back:

- **The game on screen.** A shrink changes the file's time and so the art's URL, which the hero takes for new art and answers by replaying its whole switch, with the picture unchanged. The queue skips the selected game until the selection moves.
- **A read-only library**, as for copying Steam art.
- **A scan or the first load in progress**; the queue tries again when it ends.
- **A file replaced during the shrink.** The shrunk copy is dropped and the replacement stays; the next scan checks it.
- **A file that can't be shrunk** (unreadable, disk full, a read-only folder) stays as it is and isn't tried again until its folder changes. One warning under `launcher.library` covers all of them.

Each shrink is logged at info under `launcher.library` (`art/<id>/<file>: 3479×2250 → 1280×827`).

**Display copies of heroes.** Decoding a hero takes as long as its format makes it, not its width: a 3840 px hero takes up to 140 ms (a progressive JPEG is the worst case), and even a 1920 px PNG takes about 100 ms, which made switching to such a game late and stalled the window. So the list and the hero decode a copy instead, in `~/.cache/lattis/heroes/<id>-<modified>.jpg`, where `<modified>` is the art's `ArtSet::modified`, so replaced art gets a new name. It is always a baseline JPEG at quality 90 (decoding in a few ms; PNG, WebP and progressive JPEG were all slower), because heroes never use transparency: a source with an alpha channel is painted onto black first. Art wider than 2560 px is scaled to 2560 px; narrower art is copied at its own size, never enlarged. Art 2560 px wide or narrower that already is a baseline JPEG gets no copy: it decodes as fast as a copy would, and re-encoding it would only lose quality. `HeroCache::isBaselineJpeg()` tells by the file's header (the first frame header before the first scan: `FF C0` or `FF C1` is baseline, `FF C2` progressive), not by decoding the picture. Copies in the cache made when PNG was allowed (`.png`) are no longer listed and are pruned, so those heroes get a JPEG copy. The original in `art/` is untouched and is what the editor, the art browser and bulk lookup show; only the `heroUrl` that `gameAt()` returns is the copy, and `heroArt` is always the original.

`HeroCache::make()` writes a copy (through a `.partial` name) and `HeroCache::list()` and `prune()` read and clean the folder. `Library` lists the folder once when it is created, so the first frame already uses the copies that exist. After the first frame (`startRefresh()`) it makes the missing ones in the background, one at a time, starting with the games nearest the selected row (`copyHeroesNear()`), and signals only that game's row when a copy is ready. When none are missing and no scan runs, it deletes the copies of art that is gone. A copy that can't be made leaves the original in use and logs once under `launcher.library`; it isn't tried again until the art changes. Everything in `heroes/` can be deleted: it is made again.

## Uninstalling

Uninstalling a Proton or Linux game deletes real folders, so `Uninstall::refusal()` checks every folder first, and again just before it goes. It refuses:

- a link,
- `/`, the home folder and its Desktop, Documents, Downloads…,
- a Steam library folder or one holding it, and Steam's own folders,
- the data dir or a folder holding it,
- a folder holding another game's files,
- anything not below the home folder or a Steam library.

Folders go to the trash. Where there's no trash for a folder (another drive, say), the dialog asks before deleting it for good. Every folder goes first, then the game: a folder that can't go stops the rest. Afterwards the game is kept in the list as "not installed", kept hidden, or removed along with its shortcut. A Proton game's Wine prefix (`compatdata/<shortcut>`, its saves) can also go on its own, which resets the game. The folder sizes the dialog shows are added up in the background, since a prefix alone is some 750 MB of small files.

## Format versions

`library/library.toml` holds the library's format version and its id. `LibraryVersion::prepare()` runs before anything reads the library:

- A library without `library.toml` (a new data dir, or one made by hand) gets one, at the current version. A new library also gets the default tags, and in the app `setup = "pending"`, which holds back the ROM scan and the Steam import until the first-start setup ends (`finishSetup()`).
- An older library is copied to `library/backup-v<old>/` and upgraded one version at a time. If a step fails, it stops there and the library opens read-only. There are no upgrades yet (`upgrades()`).
- A newer library, such as a backup from a newer version, opens read-only, since saving could damage a format this copy doesn't understand.
- A library without an id gets one. The id tells this library from an earlier one at the same path, which the Steam sync record relies on.

## Files

| File | Contents |
|---|---|
| `library.*` | `Library`: the list model and QML singleton |
| `librarystore.*` | `LibraryStore`: scanning, writing, watching, the cache; `scanLibrary()` |
| `libraryundo.*` | `LibraryUndo`: the last change's files, kept to put back |
| `librarycache.*` | `LibrarySnapshot`, the binary cache, file change detection |
| `libraryfiles.*` | Parsers for `library.toml`, `platforms.toml`, `tags.toml`, `steam.toml`, ROM indexes; programs' files; `buildLibrary()` |
| `gamefile.*` | The game file: `GameFields` (the key table), parsing, validation, formatting |
| `game.h` | `Game`: a game as the UI shows it |
| `gameid.*` | Ids: slugs, validation, `steam-<appid>` |
| `emulators.*` | Emulator presets, `emulators.toml`, the merge, detection, Flatpak permissions |
| `romscan.*` | ROM file names, the ROM index update, the folder scan, "+"'s inspection of a ROM |
| `romfoldermatch.*` | Matching folder names to platforms |
| `listsettings.*` | `[list]` in `settings.toml` |
| `searchtext.*` | Folding titles and searches for matching |
| `artimage.*` | Copying art, shrinking it when too large |
| `herocache.*` | The display copies of heroes in the cache: their names, making, listing and pruning, and the baseline JPEG check |
| `playhistory.*` | Play history: the session files, the Steam diff and its seen totals, a game's summary, the month statistics |
| `uninstall.*` | Which folders may be deleted, and deleting them |
| `libraryversion.*` | The format version check and upgrades |
| `lookupgame.h` | What "Look up missing info" needs to know about a game |
| `parsed.h` | `Parsed<T>`: a parsed value with its warnings |

The parsing and merging files (`libraryfiles`, `gamefile`, `emulators` apart from detection, `romscan`'s name handling, `gameid`, `searchtext`, `romfoldermatch`, `listsettings`) are pure functions: bytes or names in, values and warnings out. They run on any thread and are tested directly. Warnings read `<file>:<line>: …`.

## QML reference

Everything QML uses from `Library`. Methods that change something return an error message to show, or `""` when it worked. Paths from QML may be `file:` URLs from a FileDialog.

### Properties

**The list**

| Property | |
|---|---|
| `platformFilter`, `search` | The rail's platform and the search text |
| `groupBy` | `"platform"`, `"status"` or `"none"` |
| `sort`, `sortDescending` | `"title"`, `"last_played"` or `"play_time"`. Choosing an order sets its usual direction: A to Z, most recent first, most played first |
| `installedOnly`, `favoritesOnly`, `tagFilter`, `tagMatchAll`, `statusFilter`, `showHidden` | The filters. `statusFilter` takes the ids from `statuses`, with `"none"` for games without a status |
| `showSortValue` | Whether the rows show what they're sorted by (last played, play time); saved as `show_sort_value`. The theme's `list.show_sort_value` can turn it off too |
| `filtersActive` | A filter differs from its default |
| `notInstalledHidden` | How many games `installedOnly` is leaving out right now |

**The library**

| Property | |
|---|---|
| `loading` | First start without a cache: the list is empty until the first scan ends |
| `scanning` | A scan is running or queued |
| `readOnly`, `readOnlyNotice` | The library is never written, and why |
| `setupPending` | The first-start setup is open |
| `gameCount` | Games in the library, hidden ones aside |
| `problems` | Warnings to show on Settings → Library |
| `platforms` | `{id, name, icon}` for each platform with games, in rail order |
| `gameCounts` | `{id, name, count}` per platform, hidden games aside (the setup's Done step) |
| `statuses` | `{id, name, games}` per status, `"none"` last |
| `tags` | `{name, color, games, listed}`: tags.toml's in file order, then those only games use |
| `missingDefaultTags` | Default tags the library has no tag for |
| `hiddenGames` | `{id, title, platformName}`, by title |
| `emulators` | Every emulator, for Settings → Emulators (below) |
| `platformSettings` | Every platform, for Settings → Platforms (below) |
| `steam` | Settings → Steam (below) |
| `steamWaiting` | Games with edits waiting for Steam (below) |
| `download` | The game Steam downloads now, for the download bar: `gameId`, `name`, `icon`, `state` (`"downloading"` or `"paused"`), `percent` (`-1` unknown) and `installing`; empty when none. Steam runs one download at a time: the running game, else the first paused one in library order. `downloadChanged` is emitted only when the map changes |

### Roles

`isHeader`, `gameId`, `title` (a header's is its group's name), `platform`, `group` (the platform or status id that `toggleGroup()` takes), `count`, `collapsed`, `installed`, `hue`, `iconUrl`, `hidden`, `sortValue` (what the list is sorted by, as shown on the row: "3 days ago", "12 h").

### List navigation

| Method | |
|---|---|
| `toggleGroup(group)` | Collapses or expands a group |
| `nextGameRow(from, step)` | The next game row up or down, skipping headers |
| `nextGroupRow(from, step)` | The first game of the next group, or the start of this group and then the previous one |
| `collapseGroupAt(row)` | Collapses the game's group; returns the row to select next, or −1 if none is left |
| `expandGroupNear(row)` | Expands the nearest collapsed group below (else above); returns its first game's row |
| `rowOfGame(id)`, `gameIdAt(row)`, `gameAt(row)` | `gameAt()`'s `heroUrl` is the display copy when there is one and `heroArt` the original art; `addedText` is the date added (`Game::added`) in the short date format |
| `copyHeroesNear(row)` | The game on `row` is the one whose neighbors get their hero copies first; the game list calls it when the selection changes |
| `clearFilters()` | Every filter back to its default; the search, platform and sort stay |

### Games

| Method | |
|---|---|
| `gameForEditing(id)` | Everything the game editor shows (below) |
| `saveGame(id, values)` | Saves the editor's changed keys in one write. A new `rom` points the game at that file, merging in another game that already has it |
| `setGameValue(id, key, value)` | Saves one key |
| `inspectRom(file)` | What "+" makes of a picked file (below) |
| `romNameFilters()` | The "+" file picker's filters |
| `addGame(file, platform, title, values, art)` | Adds a game; returns `{id, error, existing}` |
| `locateProgram(id, file)` | Points a Proton game at its program again |
| `setArt(id, kind, file)`, `removeArt(id, kind)` | Removing a kind Steam has also stops the import copying it back |
| `removeGame(id, wipe)` | Hides a game. With `wipe`, an indexed game also forgets everything but where its ROM is, and any other game is deleted; a wipe offers an Undo |
| `beginEditUndo(id)`, `discardEditUndo()`, `undo(token)` | The editor's Undo around a Save; putting the last change back (see Undo) |
| `setGamesValue(ids, key, value)` | One key on many games (`hidden`, `favorite`, `emulator`) in one store batch: one reload and one Undo ("4 games hidden"). Returns the errors joined, or `""` |
| `retagGames(ids, add, remove)` | Adds and removes tags on many games the same way; a tag already there or absent is left alone |
| `unhideGame(id)` | |
| `uninstallChoices(id)` | The uninstall dialog (below) |
| `historyFor(id)` | A game's play history for its popup (below); read from its file when asked |
| `pageHistory(id)` | The game page's history fields (below); a long history comes later through `pageHistoryReady(id, played)` |
| `playTime(minutes, format)` | Play time as text in a `[game_page] play_time_format`: `hours` ("206.7 hours") or `hours_minutes` ("206h 42m", "45m"); `""` for none |
| `monthStats(year, month)` | Starts the month's statistics on a worker thread; `monthStatsReady(year, month, stats)` has the map (below) |
| `yearStats(year)` | The same for a calendar year; `yearStatsReady(year, stats)` has the map (below) |
| `deleteSession(id, end)` | Deletes the session ending at `end` (ISO text); returns `{error, session}`, and `session` goes back to `restoreSession()` for the Undo |
| `restoreSession(id, session)` | Puts a deleted session back; returns an error message or `""` |
| `emulatorChoices(platform)` | `{choices: [{id, name, installed, args}], defaultEmulator}` |
| `igdbPlatformIds(platform)` | The IGDB platforms a lookup searches; empty for every platform |
| `steamPresetOn(options, preset)`, `setSteamPreset(options, preset, on)` | The Launch card's preset check boxes |

### Settings pages

| Method | |
|---|---|
| `setPlatformValue(id, key, value)` | `name`, `icon`, `emulators`, `rom_folders` or `in_rail` (`false` is written; `true`, the default, removes the key) |
| `addPlatform(name, id)`, `removePlatform(id)`, `movePlatform(id, step)`, `movePlatformBefore(id, beforeId)`, `platformIdFor(name)` | A removed platform's games stay, under its id. `movePlatformBefore` (an empty or unknown `beforeId` means last) is what a drag writes; a platform only its games define gets its block, named after its id, as Define writes it. Both offer an Undo |
| `matchRomFolders(parentPath)` | "Find from one folder" (below) |
| `setEmulatorValue(id, key, value)` | `path` or `args`; for a user emulator also `name`, `platforms`, `flatpak`, `extensions` or `command` |
| `addEmulator(name, startFrom, id)`, `removeEmulator(id)`, `emulatorIdFor(name)` | `startFrom` is a preset's id, or empty for a blank emulator; removing takes it off every platform |
| `pinnedGames(emulatorId)` | How many games are pinned to it, for the remove confirmation |
| `emulatorsFile()`, `platformsFile()` | The file's path, created if missing so it can be opened |
| `scanNow()` | Scans the ROM folders and looks for emulators again |
| `addTag`, `addDefaultTags`, `setTagColor`, `renameTag`, `removeTag` | |
| `nextTagColor(paletteSize)` | The first palette color no tag uses |
| `igdbTags(facets, has)` | The tags that fit a game with these IGDB genres, themes and keywords |
| `setSteamValue(key, value)` | `enabled`, `path`, `user`, `default_presets` or `default_proton` |
| `steamShortcuts()`, `addSteamShortcuts(appIds)` | "Add existing shortcuts" |
| `steamCovers(limit)` | Up to `limit` installed Steam games with a cover, most recently played first: `name`, `cover` (a file: URL); for the setup guide's Steam step |
| `setSteamPresetForAll(preset, on)` | "Apply to all Steam games", "Remove from all" |
| `undoSteamEdits(id)`, `undoAllSteamEdits()` | Drops edits waiting for Steam |
| `finishSetup()` | Ends the first-start setup and brings the games in |

### Map shapes

`gameForEditing(id)`, empty if the game isn't shown:

```
id, title, platform, platformName, platformIcon, description,
year            0 = none
rating          -1 = none
ratingSource    "igdb" when a lookup set the rating, else ""
favorite, hidden, hasArt, installed, playMinutes
tags            [{name, color}]
art             {hero, logo, icon, cover} URLs
emulator        the pinned id, or ""
emulators, defaultEmulator   as emulatorChoices()
args
rom, romKey     a ROM game; its ROM is the file's `rom` (added with "+")
romPath         absolute, "" while not found
romWritten      `rom` as written
removeDeletes   wiping it deletes its file
igdb, steamgriddb   the matched ids: 0 = not there, -1 = not looked up
steamAppId      Steam games, else 0
saveFolder      the file's `save_folder` as written, or ""; Save writes one in the home folder with ~
Steam and Proton games:
  launchOptions, proton, pendingSteamEdits, installPath
  protonTools   [{name, displayName}]
  protonDefault Steam's default tool's display name
  steamPresets  [{id, name, adds}]
```

`inspectRom(file)`:

```
path, fileName, title, titleId
platforms   ids of the platforms that run it, in rail order
problem     why it can't be added (an update or DLC), or ""
existing    the game that already has the file, or ""
choices     [{id, name, icon, takes: ".nsp, .xci", runs}]: every platform with an emulator
```

`uninstallChoices(id)`; the sizes arrive later with the `uninstallSizes(id, installDir, prefix)` signal:

```
installDir          with ~; "" without one, or when it's gone
installDirRefused   why it can't be deleted, or ""
prefix              compatdata/<shortcut>, "" when there's none
```

`uninstallProtonGame(id, files, prefix, shortcut, action, permanent)` returns `{}` when done, `{trashless: path}` when a folder has no trash (call again with `permanent` to delete it for good), or `{error}`. `action` is `"keep"`, `"hide"` or `"remove"`.

`emulators`, one per emulator, presets first:

```
id, name, preset, custom (the keys emulators.toml sets)
platforms, platformIds
found, detecting (not looked for yet)
installKind     "flatpak", "appimage", "executable", "path" or ""
install         where it was found
path, pathMissing, args, flatpak, extensions
commandArgs     the `command` list as written, for editing
fileOnly        a user emulator's keys only emulators.toml can set (saves, keys…); empty for presets
command         the command as it would run
```

`platformSettings`, one per platform in rail order, those without games too:

```
id, name, icon, games
defined         it's in platforms.toml
emulators, emulatorNames, romFolders
inRail           false hides its rail button (`in_rail = false`)
problems        [{text, command}]: a missing ROM folder, no emulator installed, a Flatpak
                that can't read a ROM folder; `command` fixes it, or is ""
```

`matchRomFolders(parentPath)`, by folder name; empty if the folder doesn't exist:

```
folder, path (as rom_folders stores it), platformId, platformName
already     it's already one of that platform's ROM folders
```

`steam`, built on first read:

```
enabled, path, user     as set in [steam]; "" = found automatically
found, flatpak, root    Steam's folder
users       [{id, name, accountName, mostRecent}]
userId      the account used
installed, games
defaultPresets
presets     [{id, name, note, adds, builtIn, games}]
status      a line saying what was found, or why not
problem     the status is a problem
queue       [{id, title, changes: [{name, from, to}], adds}], by title
```

`steamWaiting`, by title. Cheaper than `steam.queue`, and a shortcut's icon alone doesn't count:

```
id      a game id, or a removed shortcut's queue key
title
adds    its shortcut isn't in Steam yet, so undoing removes the game
```

`steamShortcuts()`, Steam's shortcuts not in the library, in Steam's order:

```
appId, name
target  the program, with ~, and its arguments
tool    the compatibility tool, "" = none
checked it has a tool
art     a file: URL, or ""
```

`historyFor(id)`, empty if the game isn't shown. Minutes throughout; `Library` also emits `historyChanged(ids)` when a Steam diff added sessions:

```
id, fromSteam   Steam and Proton games: the total isn't the launcher's to change
total           the game's play time now
before          "Before history": total minus the sessions and deleted minutes, 0 at least
week, month     minutes in this week and this month
count           how many sessions
sessions        newest first: [{end, endMs, startMs, minutes, merged}]
problems        warnings from reading the file
```

`pageHistory(id)`, the numbers behind the game page's `times_played`, `first_played` and `recent_play_time` fields: `timesPlayed` and `firstPlayed` as text (`""` for none), and `recent`, the minutes played in each period (`week`, `two_weeks`, `month`, `year`), which the page writes with `playTime()` in the chosen format. It reads the history file on the spot when the file is 24 KB or smaller (about 450 sessions, which parse in about 1 ms; the demo's longest histories take about 0.2 ms) and returns `{pending: true}` for a longer one, which it reads on a worker thread (2000 sessions take about 4.5 ms) and delivers through `pageHistoryReady(id, played)`. It uses `QTimeZone::LocalTime`: the first `systemTimeZone()` loads the zone database, about 3 ms on the main thread. The numbers are `PlayHistory::pageStats()`'s, a pure function counting days as above:

```
timesPlayed     how many entries the history has; a merged Steam entry counts once
firstPlayed     the earliest session's start (a merged entry's end), in the short date format
recent          play time per period: week (from the locale's first day of the week), two_weeks
                (today and the 13 days before), month and year (the calendar month and year)
```

`monthStatsReady`'s map. Games carry `id, minutes, title, platform, iconUrl, hue`, platforms `id, minutes, name, icon`:

```
year, month, hasHistory   hasHistory: any session in any month
firstYear, firstMonth     the first session's month; 0 without history
minutes, sessions, games  the tiles' numbers
previousMinutes           the previous month's, or the same days of it
previousHasHistory
comparedThrough           the last day of the previous month compared; 0 = all of it
days            [{day, minutes, mergedMinutes, sessions, games}], games by minutes
weeks           [{number, firstDay, lastDay, minutes, mergedMinutes}], clipped to the month
platforms       by minutes
top             the five most played games
longest         {game fields, end, endMs}, or {} (merged entries don't count)
streakDays, streakFirstDay
averageMinutes, averageSessions   merged entries are left out unless they're all there is
strip           [{year, month, minutes, hasHistory, pickable}]: January to December of `year`;
                pickable = not after the current month
historyMonths   [{year, month, minutes}] for every month with history, oldest first
```

`yearStatsReady`'s map has the same `year, hasHistory, firstYear, firstMonth, minutes, sessions, games, activeDays, platforms, top, longest, averageMinutes, averageSessions, previousMinutes, previousHasHistory, historyMonths`, and:

```
columns         12 × {month, minutes, mergedMinutes, games, top, pickable}; top = the month's three
                most played, games = how many were played
weekdays        7 totals in minutes, starting on the locale's first day
comparedThroughMonth, comparedThroughDay   the last day of the previous year compared; 0 = all of it
streakDays, streakFirstMonth, streakFirstDay
years           [{year, minutes}] from the first session's year to the current one
```
