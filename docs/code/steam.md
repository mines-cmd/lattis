# Steam integration (`src/steam/`)

The `src/steam/` folder handles everything the launcher reads from or writes to Steam's own files. It knows Steam's two file formats (VDF text and binary), where a Steam installation lives and who's using it, how to read the Steam library incrementally, how to manage non-Steam shortcuts for Proton and Linux games, how to keep launch options and Proton versions in sync across the game file and Steam's files, how to put Proton game art into Steam's grid, and what the launch option presets do. The library (`src/library/`) and launcher (`src/launch/`) are the consumers: they decide which games are in the library and handle launching; this folder reads and writes Steam's side. It never decides which games the launcher shows.

## Contents

- [VDF formats](#vdf-formats)
- [Finding Steam](#finding-steam)
- [The Steam scan](#the-steam-scan)
- [Updates](#updates)
- [Non-Steam shortcuts](#non-steam-shortcuts)
- [The launch options sync](#the-launch-options-sync)
- [Art in Steam's grid](#art-in-steams-grid)
- [Launch option presets](#launch-option-presets)
- [Files](#files)
- [Reference](#reference)

## VDF formats

Steam uses two file formats. Most of its files are **text VDF** (`vdf.*`): an indented KeyValues format where each entry is a string key and either a string value or a nested block. The parser keeps each node's byte offsets in the source, so the editor can change one value in place while leaving every other byte alone. Keys are matched ignoring case, as Steam does. `#include` and `#base` directives are read over without being followed; the files the launcher touches don't rely on them.

`shortcuts.vdf` is **binary VDF** (`binaryvdf.*`): each field is a one-byte type tag, a NUL-terminated name and a typed value (NUL-terminated string, little-endian integers, and a few others). Maps end with `0x08`, and so does the file. The reader keeps every field in order with its raw bytes, so writing an unchanged document gives back the original file exactly.

Both readers leave untouched bytes exactly as they are. This keeps diffs minimal and the files readable to Steam if something ever needs manual repair.

## Finding Steam

`SteamLocate::find()` tries three locations: a configured path from `[steam] path` (with `~` expanded), then the native symlink at `~/.steam/steam`, then the Flatpak's folder at `~/.var/app/com.valvesoftware.Steam/.local/share/Steam`. A folder counts as a valid Steam installation only when it contains `steamapps/libraryfolders.vdf`; the first one that does wins.

The active user account comes from `[steam] user` (matched as an account ID or a SteamID64, whichever it looks like), falling back to the most recent login in `config/loginusers.vdf`. If a configured user isn't in `loginusers.vdf`, the install is reported as `userMissing` but the scan continues without a user, which means no play time, no launch options from `localconfig.vdf` and no shortcuts.

`runningPid()` reads `steam.pid`, then checks that `/proc/<pid>/exe` points to that installation's Steam binary (Steam leaves `steam.pid` behind after a crash, so the check matters). A Flatpak Steam runs in its own PID namespace and its PID isn't visible that way; for Flatpak, the function walks `/proc/*/exe` searching for the binary instead.

`SteamExitWatch` uses a Linux `pidfd` to be notified when Steam exits without polling. The library holds one of these and uses it to apply the queue of pending edits as soon as Steam closes.

## The Steam scan

`SteamScan::scan()` runs on a background thread. It takes the previous `SteamState` and a `SteamSettings`, finds Steam, and re-reads only the files whose `SteamStamp` (mtime and size) changed since last time. If the Steam folder changed entirely—different path, different installation—the previous state is discarded and everything is re-read.

The files it reads, in order:

- `steamapps/libraryfolders.vdf`: the list of library folders; the Steam folder's `steamapps/` is always first. Library folders for drives that aren't mounted are left out.
- `steamapps/appmanifest_<appid>.acf` for each app: name, install directory, `StateFlags` (fully installed, and the update bits, see [Updates](#updates)), `buildid` and `TargetBuildID`, `BytesToDownload`, `BytesToStage`, `BytesStaged`, size on disk.
- `userdata/<user>/localconfig.vdf`: launch options, play time (minutes) and last-played timestamp for each app, keyed by app ID. Shortcuts' entries are keyed by their signed app ID (the 32-bit app ID reinterpreted as a signed integer, as Steam stores it: the same number, different type).
- `userdata/<user>/config/shortcuts.vdf`: the non-Steam shortcuts.
- `config/config.vdf`: `CompatToolMapping`, which maps each app ID to the Proton version it runs with. App ID `0` is the default for every game that has no individual entry.
- `*/compatibilitytool.vdf` in each `compatibilitytools.d` folder: the installed compatibility tools and their display names.

`SteamState::games()` filters the manifest list to actual games: `isBlocked()` excludes Proton versions, Steam runtimes and redistributables by app ID range and name patterns. The tools list puts Valve's Protons first (Experimental, then Hotfix, then versions newest first with a natural-order collator), then custom tools. `valveToolName()` derives the internal name from the display name ("Proton 9.0" → `proton_9`).

`SteamState::watchPaths()` returns the folders and files the library should watch for changes so it knows when to run another scan.

## Updates

Whether a Steam game has an update comes from Steam's own files; Lattis never contacts Steam's servers, so a patch Steam hasn't seen yet shows nothing. `readManifest` turns `StateFlags` into `SteamApp::update`, checked in this order:

| State | When |
|---|---|
| `Paused` | bit `512`, or bit `1024` (update started) without bit `256` (update running). Pausing in Steam clears "running" and keeps "started"; no real manifest has had `512` |
| `Downloading` | bit `256` |
| `Waiting` | bit `2` (update required), or `TargetBuildID` not `0` and different from `buildid` |
| `None` | otherwise |

A game that's updating has lost bit `4` (fully installed) in some states, so it counts as installed when bit `4` is set or the state isn't `None` and `buildid` isn't `0` (a first install keeps `buildid` at `0`, so it stays not installed, but its update state still shows: the download bar says Installing). `updateBytes` (`BytesToDownload`) and `updatePercent` are set only while the state isn't `None`, because Steam keeps the last update's numbers after it finishes. `updatePercent` is `BytesStaged` / `BytesToStage`, which is what Steam shows; it falls back to the download bytes without staging sizes, and is `-1` without sizes.

**Steam rewrites the manifest only when an update starts or stops**, not while it runs: a running and a paused update have the same flags, and the bytes stay at their last value. So `SteamDownloads` (`steamdownloads.*`) tells them apart while an update has started:

- **Running or paused** comes from the end of `<Steam>/logs/content_log.txt` (the last 512 KB, read again when the file changes). The last `AppID <id> state changed :` line for the app says `Update Running` while it runs. The `update started : … stage <done>/<total>` line gives the stage total.
- **The percentage** is the allocated bytes (`st_blocks`, not file sizes: Steam preallocates its files, so sizes run about 30% ahead) of `steamapps/downloading/<appid>` over the stage total, summed on a background thread once a second while some update runs, and once when it pauses. That matched `BytesStaged` within 0.3% and Steam's own percentage exactly.
- It watches and polls nothing unless some Steam game's manifest says an update started. Without a stage total the percentage stays the manifest's.

`Library` hands it the games whose manifest state is `Downloading` or `Paused`, and overlays its result on them (`Library::updateDownloads()`, `applyDownloads()`), so the selected game and the list refresh by themselves.

**Starting an update.** `GameRunner::update(gameId)` is what the Launch button calls for a game with an update (`launch()` redirects there too): Waiting opens the game's page in Steam (`steam://nav/games/details/<id>`), where Steam's own Update button is; Downloading and Paused open Steam's downloads (`steam://open/downloads`). Steam ignores `steam://update/<id>` and `steam://install/<id>` on an installed game, and `steam://validate/<id>` would check every file first, so none of them is used.

## Non-Steam shortcuts

Proton games and Linux games added with the "+" button appear in Steam as non-Steam shortcuts in `shortcuts.vdf`. The launcher creates a shortcut for each such game when it's added, updates it when the title, program, start folder, launch options, icon or Proton version changes, and removes it (along with the `CompatToolMapping` entry) when the game is uninstalled.

Shortcut app IDs are assigned the same way Steam ROM Manager does: `crc32(exe_field + name)` with the top bit set, where `exe_field` is the program quoted in double quotes followed by any extra arguments. If the result collides with an existing shortcut's ID, it's incremented. This makes IDs stable: re-adding the same program with the same name produces the same ID. The 64-bit game ID that `steam://rungameid/` needs is a separate computation from the 32-bit app ID: `gameId()` derives it, and it differs from the app ID.

The `exe` field in shortcuts.vdf stores the program quoted, then any arguments that aren't in `LaunchOptions` (because Steam treats the two fields differently). `exeField()` builds the field from a program and arguments; `splitExe()` unpacks it back. The launcher keeps this split consistent so Steam reads it correctly.

`SteamShortcuts::add()`, `update()` and `remove()` each touch only the affected shortcut's bytes and leave the rest of the binary file intact. `remove()` renumbers the remaining shortcuts, as Steam does.

## The launch options sync

Launch options and Proton versions each live in two places: the game file (`launch_options`, `proton`) and Steam's files (localconfig.vdf for launch options, config.vdf's `CompatToolMapping` for Proton). Either side can be changed independently: the user can edit the game file in the launcher, or change settings inside Steam. The sync record (`SteamSync::Record`, stored in the state dir as `steam-sync-<data dir key>.toml`) is what lets the launcher tell which side changed.

For each value, the comparison is:

| Game file | Steam | Record | What happens |
|---|---|---|---|
| Same as Steam | — | — | In sync; record updated |
| Differs | Same as record | Present | Changed in the launcher; queued for Steam |
| — | Differs from record | Present | Changed in Steam; copied to the game file |
| Both differ | Differs from record | Present | Steam wins; copied to the game file |
| — | — | Absent | Game file value queued; no game file value → Steam's copied |

`SteamSync::sync()` walks every game and produces a `Result` with two maps: `queued` (game id → the keys that need to go to Steam) and `copies` (game id → the values to write back into the game file). The library applies the copies and stores the queue for later. A Proton game's shortcut follows the same table field by field, plus a few extra cases: a shortcut with no record and not in Steam is added; a shortcut that was deleted in Steam but has a record marks the game `installed = false`.

`SteamSync::apply()` writes the queue to Steam's files. It writes `shortcuts.vdf` first, then `config.vdf` (CompatToolMapping), then `localconfig.vdf`. The order matters: a new shortcut's Proton version in `config.vdf` should follow the shortcut being created. Each file is re-parsed after the edit to confirm correctness, backed up once into `backupDir` under its path within the Steam folder (an existing backup is kept), and replaced atomically. Before and again immediately before each file is written, `steamRunning` is called: Steam overwrites its files on exit, so nothing is written while it runs. An `Edit` carries the value Steam had at the last scan; if the file now says something different, the edit is set aside as stale for the next scan rather than overwriting the newer value.

The record lives in the state dir, not the data dir, and includes the library's ID. This means: after a backup restore the state dir is fresh (no record), so the game files' values are queued back to Steam instead of being overwritten by what Steam had at restore time. And a newly created library at the same path as an old one doesn't inherit the old record, since the IDs won't match.

## Art in Steam's grid

Steam's UI shows art for non-Steam shortcuts if the files exist in `userdata/<user>/config/grid/`. `SteamArt::writeGrid()` keeps that folder in step with the launcher's art for every Proton game:

- A kind (hero, cover, logo, icon) that has no grid file gets one written from the game's art. Hero, cover and logo are written as PNG or JPEG (other formats are converted to PNG); icons are always written as PNG.
- A kind the launcher wrote follows the source art: rewritten when the source changes, deleted when the art or the game is gone from the library.
- Any file the launcher didn't write is never touched, and neither is a file the launcher wrote but that has since changed size or mtime—that file is the user's now, and `GridFile`'s recorded size and mtime are how the launcher detects the replacement.

For Steam games (not shortcuts), the launcher copies art once into `art/<id>/` during the scan. It checks `userdata/<user>/config/grid/` first for custom art, then `appcache/librarycache/<appid>/` where Steam caches downloaded art. The library cache stores files either in the main `<appid>/` folder or in one of several hashed subfolders; when there are multiple versions, the launcher takes the one with the latest mtime.

## Launch option presets

A preset is a named combination of environment variable assignments (before `%command%`), a wrapper program (just before `%command%`) and arguments (just after it). The built-in presets are mangohud, gamemode and a `PROTON_*` variable group for Wayland (which does nothing for Linux games and is Proton-only). Users can add their own presets in `[steam.presets.<id>]` or change a built-in by setting only the keys they want different.

`LaunchOptions::isOn()` checks whether all of a preset's parts appear in a launch options string, at their expected positions. `turnOn()` adds what's missing; `turnOff()` removes the preset's parts and removes `%command%` too when nothing is left before it, so a round-trip gives back the original string.

New Steam and Proton games start with `SteamSettings::defaultPresets` applied via `SteamSync::withDefaults()`, which takes the current launch options in Steam and turns on any missing defaults. Linux games skip the Proton-only presets.

## Files

| File | Contents |
|---|---|
| `vdf.*` | `Vdf::Document`, `Node`; `parse()`, `setValue()`, `remove()`, `quoted()` |
| `binaryvdf.*` | `BinaryVdf::Document`, `Node`; `parse()`, `write()` |
| `steamshortcuts.*` | `SteamShortcut`; `read()`, `add()`, `update()`, `remove()`; `exeField()`, `splitExe()`, `gameId()`, `newAppId()` |
| `steamlocate.*` | `SteamUser`, `SteamInstall`, `SteamLocations`; `SteamLocate::find()`, `runningPid()`, `gamePid()`; `SteamExitWatch` |
| `steampresets.*` | `SteamPreset`, `SteamSettings`; `parseSteamSettings()`; `LaunchOptions::isOn()`, `turnOn()`, `turnOff()` |
| `steamart.*` | `SteamArt::find()`, `writeGrid()`; `GridFile`, `GridFiles` |
| `steamscan.*` | `SteamStamp`, `SteamApp`, `SteamAppConfig`, `CompatTool`, `SteamState`; `SteamScan::scan()`, `isBlocked()`, `valveToolName()` |
| `steamupdate.h` | `SteamUpdate` (`None`, `Waiting`, `Downloading`, `Paused`) |
| `steamdownloads.*` | `SteamDownloads`: `setApps()`, `progress()`, `changed()`; `parseLog()`, `directorySize()` |
| `steamsync.*` | `SteamSync::Record`, `Result`, `Edit`, `ShortcutEdit`; `sync()`, `apply()`, `compare()`, `withDefaults()`, `readRecord()`, `writeRecord()` |

## Reference

### VDF text (`vdf.*`)

| | |
|---|---|
| `Vdf::parse(text)` | Parses a text VDF file; `Document.ok` is false and `error` is set on failure |
| `Vdf::setValue(text, path, value)` | Sets the value at `path` (blocks, then the key); adds missing keys and blocks |
| `Vdf::remove(text, path)` | Removes the key or block at `path`; no-op when missing |
| `Vdf::quoted(value)` | The quoted, escaped form of a value as Steam writes it |

`Vdf::Node`:

```
key        the key string
value      the value string; empty for a block
isBlock    true for a block
line       1-based line of the key
begin      byte offset of the key
valueBegin byte offset of the value token or the block's '{'
end        byte offset past the value or the block's '}'
```

`Node` methods: `child(key)` (first child by that name, case-insensitive), `find(path)` (descends by each name), `valueOf(key, fallback)` (a child's string value, or fallback).

### VDF binary (`binaryvdf.*`)

| | |
|---|---|
| `BinaryVdf::parse(bytes)` | Parses a binary VDF file; `Document.ok` false on failure |
| `BinaryVdf::write(root)` | Serialises `root`'s fields back to bytes |

`BinaryVdf::Node`:

```
type      Map, String, Int32, UInt64, etc.
key       NUL-free name as bytes
data      value bytes as stored (string without NUL, integers little-endian)
children  a map's fields, in order
```

Factory methods: `Node::string(key, value)`, `Node::int32(key, v)`, `Node::uint64(key, v)`, `Node::map(key, children)`.

### Finding Steam (`steamlocate.*`)

| | |
|---|---|
| `SteamLocate::find(where, configuredPath, configuredUser)` | Finds the Steam installation and active user |
| `SteamLocate::readUsers(root)` | `loginusers.vdf`'s accounts in file order |
| `SteamLocate::accountId(steamId64)` | The account ID (userdata folder name) for a SteamID64; empty if not one |
| `SteamLocate::runningPid(install, where)` | The PID of the running Steam, or 0 |
| `SteamLocate::gamePid(appId, procDir)` | The PID of the reaper Steam launched for `appId`, or 0 |
| `SteamInstall::userDataDir()` | `<root>/userdata/<userId>`, or empty without a user |

`SteamInstall`:

```
kind          None, Native or Flatpak
root          the Steam folder, links resolved
pathMissing   [steam] path is set but there's no Steam there
users         accounts from loginusers.vdf
userId        the account used
userMissing   [steam] user is set but hasn't logged in here
```

`SteamExitWatch`: `watch(pid)` starts watching; `exited()` fires when Steam exits.

### The Steam scan (`steamscan.*`)

| | |
|---|---|
| `SteamScan::scan(settings, where, previous)` | Incremental scan; re-reads only files whose stamp changed |
| `SteamScan::isBlocked(appId, name)` | True for Proton versions, runtimes and redistributables |
| `SteamScan::valveToolName(appId, name)` | Internal name from the manifest display name ("Proton 9.0" → `proton_9`) |
| `SteamState::games()` | App manifests that aren't blocked, by app ID |
| `SteamState::tools()` | Installed tools: Valve's Protons newest first, then custom ones |
| `SteamState::shortcutConfig(appId)` | `localconfig.vdf`'s block for a shortcut (play time) |
| `SteamState::watchPaths()` | Folders and files to watch for changes that should trigger a re-scan |

`SteamStamp`: `mtimeMs`, `size` (`−1` when missing). `SteamApp`: `appId`, `name`, `installDir`, `installed` (fully installed, or updating over a build on disk), `sizeBytes`, `update` (`SteamUpdate`), `updateBytes`, `updatePercent` (`-1` unknown). `SteamAppConfig`: `playMinutes` (`−1` = never), `lastPlayed`, `launchOptions`. `CompatTool`: `name` (internal), `displayName`.

### Downloads (`steamdownloads.*`)

| | |
|---|---|
| `SteamDownloads::setApps(logPath, apps)` | The apps whose update started (`App`: `appId`, `downloadDir`); with none it watches and polls nothing. Reads the log at once |
| `SteamDownloads::progress()` | `appId` → `Progress`: `running`, `percent` (`-1` unknown) |
| `SteamDownloads::changed()` | Emitted when any `Progress` changed |
| `SteamDownloads::parseLog(tail, appIds)` | `appId` → `LogState`: `running` (the last state line has `Update Running`), `stageTotal` |
| `SteamDownloads::directorySize(path)` | Allocated bytes of the files under a folder |

### Non-Steam shortcuts (`steamshortcuts.*`)

| | |
|---|---|
| `SteamShortcuts::read(root)` | The shortcuts in a `shortcuts.vdf` document, in file order |
| `SteamShortcuts::add(root, shortcut)` | Appends a shortcut; false if its app ID already exists |
| `SteamShortcuts::update(root, shortcut)` | Updates the matching shortcut's fields; false if not found |
| `SteamShortcuts::remove(root, appId)` | Removes the shortcut; renumbers the rest; false if not found |
| `SteamShortcuts::newAppId(exeField, name, taken)` | Derives a stable app ID from the exe and name |
| `SteamShortcuts::gameId(appId)` | The 64-bit ID for `steam://rungameid/` |
| `SteamShortcuts::signedAppId(appId)` | The localconfig.vdf key for a shortcut (signed cast of the app ID) |
| `SteamShortcuts::exeField(program, args)` | Builds the `exe` field: `"<program>" <args>` |
| `SteamShortcuts::splitExe(field, program, args)` | Unpacks `exeField` into program and arguments |

`SteamShortcut` fields:

```
appId          appid (stored in the file as a signed int32)
name           appname
exe            the program in `exe`, unquoted
exeArgs        anything after the program in `exe` (not LaunchOptions)
startDir       StartDir, unquoted
icon           a path
launchOptions  LaunchOptions
lastPlayTime   LastPlayTime, Unix time; 0 = never
hidden         IsHidden
```

### Art (`steamart.*`)

| | |
|---|---|
| `SteamArt::find(install, appId, kinds)` | Steam's art for `appId` by kind, as absolute paths |
| `SteamArt::gridDir(install)` | `userdata/<user>/config/grid`, or empty without a user |
| `SteamArt::gridIconPath(install, appId)` | The launcher's icon path for a shortcut in grid/ |
| `SteamArt::writeGrid(gridDir, art, written)` | Brings grid/ up to date with Proton game art; returns log lines |

`GridFile`:

```
name        file name in grid/: 2320877088_hero.png
size        as written; differs → someone replaced it
modified    ms since epoch, UTC
art         source file in art/<id>/: hero.png
artSize, artModified  source file's size and mtime at write time
```

`GridFiles` is `QHash<qint64, QHash<QString, GridFile>>` (by shortcut app ID, then by kind).

### The sync (`steamsync.*`)

| | |
|---|---|
| `SteamSync::sync(games, steam, record, shortcutGames)` | Compares every game with Steam and the record; returns the queue and copies |
| `SteamSync::apply(steam, edits, backupDir, steamRunning, shortcuts)` | Writes the queue to Steam's files; returns what was applied and what was stale |
| `SteamSync::compare(gameFile, steam, record)` | Which side changed one value: `InSync`, `Queued` or `FromSteam` |
| `SteamSync::withDefaults(launchOptions, settings, forLinux)` | Turns on the default presets in a launch options string |
| `SteamSync::readRecord(path, library, warnings)` | Reads the record; empty when missing or a different library's |
| `SteamSync::writeRecord(path, record, dataDir, error)` | Writes the record atomically |
| `SteamSync::recordOf(shortcut, proton)` | A `ShortcutRecord` from a current `SteamShortcut` |
| `SteamSync::recordPath(stateDir, dataDirKey)` | `<stateDir>/steam-sync-<dataDirKey>.toml` |
| `SteamSync::sameShortcut(a, b)` | True when name, program, arguments, start folder, launch options and icon match |
| `SteamSync::removedKey(appId)` | Queue key for a shortcut whose game was removed from the library |

`SteamSync::Record`:

```
library      the library's id; empty record returned when it doesn't match
launchOptions, proton  by app ID: what Steam had when they last matched
shortcuts    by shortcut app ID: ShortcutRecord (title, exe, startDir, launchOptions, proton)
grid         GridFiles the launcher wrote, tracked for the art sync
removed      app ID → title for shortcuts pending removal after Uninstall
```

`SteamSync::Result`:

```
queued   game id → keys waiting for Steam (launch_options, proton; kAddShortcut, kRemoveShortcut for shortcuts)
copies   game id → values to write into the game file
record   the record, updated
log      values where Steam won; shortcuts deleted in Steam
```

`SteamSync::Edit`: `appId`, `key` (`launch_options` or `proton`), `value` (empty = removed), `steam` (what the last scan saw; if the file differs now, the edit is left as stale).

`SteamSync::ShortcutEdit`: `kind` (Add, Update, Remove), `shortcut`, `steam` (what the last scan saw; `nullopt` for Add).
