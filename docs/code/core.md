# src/core

`src/core` handles everything that runs before the UI is ready and keeps the process running cleanly: finding the app's directories, enforcing a single running copy, migrating from the old app name, remembering window sizes, managing the autostart entry, editing settings files without destroying a user's formatting, and restarting the app after a library import.

Nothing here touches the game library, Steam, or the launcher. Those concerns live in `src/library`, `src/steam`, and `src/launch`.

---

## App directories

All persistent data lives in three XDG directories resolved by `AppPaths`:

- **Data dir** — `~/.local/share/lattis` (or `$LATTIS_DATA`). The game library, settings, credentials, and art all live here. Copying it is a complete backup.
- **Cache dir** — `~/.cache/lattis`. The binary library snapshot. Not backed up; rebuilt on demand.
- **State dir** — `~/.local/state/lattis`. Window sizes and the Steam sync record. Not backed up; losing it only means windows open at their default size.

`AppPaths::dataDirKey()` hashes the data dir's absolute path to a 12-hex-character key. The key names the library cache file and the single-instance lock, so two data dirs — a production one and a development scratch dir — never interfere with each other.

---

## One running copy per data dir

`SingleInstance` prevents two copies from writing the same library and cache simultaneously. On startup, `tryLock()` takes a `QLockFile` in `$XDG_RUNTIME_DIR` (named `lattis-<key>.lock`). If it fails, the lock is held by a running copy; `activateRunning()` sends it a message over a `QLocalServer` socket (also in the runtime dir) asking it to raise its window, and the new copy quits.

The lock and socket live in the runtime dir — not the data dir — for two reasons: the runtime dir is cleared at logout, so a crash cannot leave a stale lock behind for long; and a backup of the data dir never inadvertently contains one.

`listen()` starts the local server once the lock is held. It must be called after the first frame, since `activateRunning()` retries for a moment expecting a listener to appear.

---

## Startup housekeeping

**Name migration.** The app was previously named "game-launcher". `OldName::move()` runs once on the first start under the new name: it moves the data dir from `~/.local/share/game-launcher` to `~/.local/share/lattis` and the state dir from `~/.local/state/game-launcher` to `~/.local/state/lattis`, renaming the key-tagged files inside the state dir (window sizes, Steam sync records) for the new path. The cache is not moved — it is cheaper to rebuild. If the old data dir is still locked by a running copy of the old app, the move is skipped.

**General settings.** `General` is a QML singleton that reads `[general]` from `settings.toml` once at startup. It exposes `startFullscreen` (applied at the next start), `downloadBarCollapsed` (whether the user hid the download bar; applied at once), `railHidden` (whether the rail is hidden; applied at once, set by `setRailHidden()` from the `toggle_rail` action), the autostart entry state, and the data directory. `setStartFullscreen()`, `setDownloadBarCollapsed()` and `setStartAtLogin()` write back through `TomlPatch` and `Autostart`. The settings page calls `reload()` when it becomes visible to refresh the displayed state.

**Game page settings.** What the game's page shows is the user's choice, not the style's, so it lives in `[game_page]` in `settings.toml`, read by `parseGamePageSettings()` (in `gamepagesettings.*`, shaped like `parseGeneralSettings()`) whenever `General` loads. The keys:

| Key | Meaning |
|---|---|
| `chips` | The chips beside Launch, in order: `play_time`, `last_played`, `release_date`, `platform`, `status`, `install_size`, `rating`, `times_played`, `first_played`, `recent_play_time`, `date_added`, and the Steam chips `steam_waiting` and `steam_update`. Default `play_time last_played steam_waiting steam_update` |
| `buttons` | The icon buttons at the right of the action row, in order: `favorite`, `open_platform`, `folder`, `settings`, `save_folder`, `menu` (⋯, opens the game's menu under it). `open_platform` is left out for Linux games, and `save_folder` for a game with no save folder (`GameRunner.saveFolder()`, looked up only for the shown game and only while the button is in the list). Default `favorite open_platform folder settings` |
| `info_rows` | The info panel's rows, in order; the same fields as the chips without the Steam chips. Default `release_date rating status install_size platform`. `[]` hides the panel |
| `chips_order`, `buttons_order`, `info_rows_order` | Every choice of that list in the order the settings page shows them, hidden ones included, so a hidden entry keeps its place. Only written when it differs from the shown ones followed by the rest; the shown ones' order in the list above wins |
| `recent_period` | The period of `recent_play_time`: `week`, `two_weeks` (default), `month`, `year` |
| `play_time_format` | How the `play_time` and `recent_play_time` fields write it: `hours` (default, "206.7 hours") or `hours_minutes` ("206h 42m") |
| `show_description`, `show_tags` | Both default to true |

A name that isn't a choice is dropped with a warning in `General.problems`; a choice the file leaves out is hidden (`gamePageOrder()` still lists it, after the others). The three lists are `GamePage::chips`, `buttons` and `infoRows` (the names `"chips"`, `"buttons"` and `"info_rows"`); `gamePageChoices(place)` has each one's choices. The game page's bindings listen to `General.gamePageChanged`, not `settingsChanged` (which re-shows the main window).

The style used to set `details.stats`, `action_buttons`, `info_rows`, `show_description` and `show_tags`. `GamePageMigration::run()` copies what the active style (and what it extends) set into `[game_page]` the first time the app starts after that change, under the new names (`stats` is `chips`, `action_buttons` is `buttons`), never over a key `[game_page]` already has. `Theme` hands it the values (`legacyGamePage()`, collected while the style loads, since the keys are no longer in the schema). A marker file in the state dir (`game-page-migrated-<data dir key>`) makes it run once, so a theme that sets them later isn't copied; after that the keys are unknown settings and show in Problems like any other. A write error stops it before the marker, so the next start tries again. `main.cpp` runs it right after `Theme` is created, before `General` reads the file.

**Window sizes.** `WindowState` is a QML singleton that persists each window's size and maximized state across starts in `~/.local/state/lattis/windows-<key>.toml`. QML reads the saved sizes in its property initializers — before the first frame — so the window opens at the right size immediately rather than snapping after paint. `track()` wires up the change signals and saves on quit. The Settings window ignores its saved size when `settings_window.remember_size` is off and opens at `settings_window.width` and `height` instead (applied on slider release, to the open window too, via `applyFixedSize()`); the schema flag `onRelease` marks settings whose slider applies on release.

`canMinimize` is false on tiling compositors (Hyprland, sway, niri, river) that do nothing useful when a window is minimized, so the window-controls component omits the minimize button on those desktops. `fillsScreen()` guards the maximized check: tiling compositors mark floating windows maximized to suppress client-side decorations, so a window only counts as maximized if it also fills its screen.

---

## Autostart

`Autostart` manages the XDG autostart desktop entry (`~/.config/autostart/lattis.desktop`). The entry uses `env LATTIS_DATA=<dir>` in its `Exec` line when a custom data dir is set, so the autostarted copy uses the same dir the user configured.

`readEntry()` checks whether `lattis.desktop` exists, whether it belongs to Lattis (`X-Lattis-Autostart=true`), and whether it is enabled (`Hidden` and `X-GNOME-Autostart-enabled` are absent or affirmative). A foreign entry is reported as `ours = false` — General shows it in the UI rather than overwriting it.

`General::refreshAutostart()` runs after the first frame. If the entry's `Exec` line is stale — the program path changed, e.g. after an AppImage update — it rewrites it. Foreign entries are never touched.

---

## Editing settings files

`TomlPatch` edits a TOML file's raw text rather than parsing it into a table and writing back. This keeps a user's comments, blank lines, and key order intact when the settings window writes a value.

The callers pass already-formatted TOML right-hand sides — string literals in TOML syntax, integers, arrays — and `TomlPatch` splices the new value into the right place in the file. Adding a key inserts it after the section's last key; adding a section appends it at the end; the rest of the bytes are untouched.

`setValue` / `removeValue` operate on `[section]` / key pairs. `setBlockValue` / `removeBlockValue` / `appendBlock` / `removeBlock` / `moveBlock` work on `[[array]]` blocks identified by one of their key values (e.g. the `[[platform]]` whose `id` is `"switch"`). The file-level helpers `setValueInFile` and `removeValueInFile` wrap read → patch → atomic write and are no-ops if the value is already correct.

---

## Restarting after import

A library Import replaces the data dir while the app runs. Afterward the app restarts itself: `Restart::start()` launches a new copy with the same program, arguments, and environment (so `LATTIS_DATA` carries over), plus `--wait-for-lock`. The new copy calls `waitForLock()` instead of `tryLock()`, waiting up to 30 seconds for the old copy's lock to clear rather than treating the held lock as a conflict to resolve by asking a window to show itself.

When running as an AppImage, `Restart::command()` uses `$APPIMAGE` as the program rather than the binary path inside the mount, which disappears when the old copy exits.

---

## Did you mean

`suggestion()` returns `" — did you mean "<candidate>"?"` for the closest match to an unknown name, or an empty string if no candidate is close enough. It is appended to warnings the parsers emit when a user's file references a name that doesn't exist.

---

## Reading a settings section

Each part of `settings.toml` has its own parser (`[general]`, `[launch]`, `[metadata]`, `[list]`, `[steam]`), and they all start the same way, through `TomlSection`. It parses the file and finds the `[name]` table. A syntax error makes `ok` false, with the line in `warnings`. A missing section leaves `table` null with no warning, and a `name` that isn't a table leaves it null with the warning "`name` must be a [`name`] section". The parser then reads the keys it owns from `table` and words its warnings with `where(node)` (`settings.toml:<line>`). The object owns the parsed file, so it can't be copied, as `table` points into it.

The header also has `toQString(std::string_view)`, which every file that reads TOML uses.

---

## Files

| File | Responsibility |
|---|---|
| `apppaths.*` | Data, cache and state dir paths; the data dir key |
| `autostart.*` | XDG autostart entry: read, write, remove, the Exec line |
| `general.*` | General QML singleton: `[general]` settings, autostart state |
| `generalsettings.*` | `[general]` struct and parser |
| `oldname.*` | One-time migration from the "game-launcher" folder names |
| `restart.*` | Restarting the process after a library Import |
| `singleinstance.*` | One running copy per data dir: lock, socket, activate |
| `suggestion.*` | "did you mean" hint for unknown names in user files |
| `tomlpatch.*` | In-place TOML editing preserving comments and formatting |
| `tomlsection.*` | A settings section parsed: the table, its warnings, `where()`; `toQString()` |
| `windowstate.*` | Window size and maximized state across starts |

---

## Reference

### AppPaths

| Function | Returns |
|---|---|
| `dataDir()` | `~/.local/share/lattis`, or `$LATTIS_DATA` |
| `cacheDir()` | `~/.cache/lattis` |
| `stateDir()` | `~/.local/state/lattis` |
| `dataDirKey(dataDir)` | First 12 hex digits of SHA-1 of the absolute path |

### Autostart

| Function | Returns |
|---|---|
| `entryExec(program, dataDir)` | The `Exec=` value for our entry |
| `readEntry(folder)` | `Entry{exists, ours, enabled, exec}` |
| `writeEntry(folder, program, dataDir)` | Error string, or `""` |
| `removeEntry(folder)` | Error string, or `""` |
| `autostartFolder()` | `QStandardPaths::ConfigLocation + "/autostart"` |
| `currentProgram()` | `$APPIMAGE` if set, else the app binary |

### General (QML singleton)

| Property | Description |
|---|---|
| `startFullscreen` | Start the window fullscreen next launch |
| `startedFullscreen` | `startFullscreen` as read at startup, constant: the main window uses it so toggling the setting does not change the open window |
| `downloadBarCollapsed` | The user hid the download bar (shown as a pill); kept until shown again |
| `systemLocale` | Format dates and numbers in the system locale (false: English with dates as `dd/MM/yy`, applied at the next start via `QLocale::setDefault` and `DateFormat::setSystemLocale` in `main.cpp`) |
| `reduceMotion` | The person turned on Reduce motion (`reduce_motion`); it has its own signal, `reduceMotionChanged`, because `settingsChanged` re-shows the main window |
| `railHidden` | The rail is hidden (`rail_hidden`; the app menu's Hide rail / Show rail, action `toggle_rail`); applied at once, with its own signal, `railHiddenChanged`, for the same reason |
| `railHiddenButtons` | The rail's bottom buttons turned off in its menu, by action name (`add_game`, `play_statistics`, `open_settings`; `rail_hidden_buttons`). `setRailButtonShown(name, shown)` writes it, removing the key when none is hidden; its own signal, `railHiddenButtonsChanged`, for the same reason |
| `railButtonOrder` | The same buttons in the order the rail shows them (`rail_button_order`; a button the key leaves out follows the listed ones, via `orderedRailButtons()`). `setRailButtonOrder(order)` takes all three names and removes the key when the order is the default; its own signal, `railButtonOrderChanged` |
| `gamePageChips`, `gamePageButtons`, `gamePageInfoRows` | The game page's shown chips, buttons and info rows, in order (`[game_page]`, above) |
| `recentPeriod`, `playTimeFormat`, `showDescription`, `showTags` | The rest of `[game_page]` |
| `startAtLogin` | An autostart entry exists and is enabled |
| `startAtLoginOurs` | That entry was written by Lattis |
| `problems` | Parse warnings from `[general]` and `[game_page]` |
| `gamePageProblems` | Those from `[game_page]` alone, for the Game page; notifies with `gamePageChanged` |
| `dataDir` | The app's data directory |
| `dataDirFromEnv` | The data dir comes from `$LATTIS_DATA` |

| Invokable | Returns |
|---|---|
| `setStartFullscreen(bool)` | Error string, or `""` |
| `setDownloadBarCollapsed(bool)` | Error string, or `""` |
| `setReduceMotion(bool)` | Error string, or `""` |
| `setRailHidden(bool)` | Error string, or `""` |
| `gamePageOrder(place)` | Every choice of `"chips"`, `"buttons"` or `"info_rows"` in the order the settings page shows them |
| `setGamePageShown(place, name, shown)` | Error string, or `""`; the entry keeps its place when hidden |
| `setGamePageOrder(place, order)` | Error string, or `""`; `order` has every choice. Writes the list and, when it differs from the default arrangement, `<place>_order`; a default leaves no key |
| `setRecentPeriod(period)`, `setPlayTimeFormat(format)`, `setShowDescription(bool)`, `setShowTags(bool)` | Error string, or `""`; a default removes the key |
| `setStartAtLogin(bool)` | Error string, or `""` |
| `reload()` | — re-reads settings and autostart |

### SingleInstance

| Method | Description |
|---|---|
| `tryLock()` | Take the lock; false if a live copy holds it |
| `waitForLock(timeoutMs)` | Wait for the lock to clear, then take it |
| `activateRunning()` | Ask the running copy to show its window |
| `listen()` | Start answering `activateRunning()` |

Signal: `activateRequested()` — emitted when another copy asked this one to show itself.

### TomlPatch

All functions return `Result{ok, text, error}` except the file-level helpers (return `bool`).

| Function | Description |
|---|---|
| `setValue(text, section, key, value)` | Set a key in `[section]`; add section/key if absent |
| `removeValue(text, section, key)` | Remove a key from `[section]`; no-op if absent |
| `setBlockValue(text, block, key, value)` | Set a key inside an `[[array]]` block |
| `blockValues(text, array, key)` | String value of `key` in every block of `array` |
| `blockKeys(text, block)` | Keys of the block, in file order |
| `removeBlockValue(text, block, key)` | Remove a key from the block |
| `appendBlock(text, array, values)` | Add a new `[[array]]` block at the end |
| `removeBlock(text, block)` | Remove the block and its comments |
| `moveBlock(text, block, beforeValue)` | Reorder the block within its array |
| `setValueInFile(path, section, key, value)` | Read → patch → write atomically |
| `removeValueInFile(path, section, key)` | Read → patch → write atomically |

A `Block` identifies an `[[array]]` block by one of its key-value pairs:
```
Block{ array: "platform", key: "id", value: "switch" }
```

### WindowState (QML singleton)

| Invokable | Description |
|---|---|
| `width(name, fallback)` | Saved width of window `name`, or `fallback` |
| `height(name, fallback)` | Saved height, or `fallback` |
| `maximized(name)` | Whether the window was maximized |
| `fillsScreen(window)` | True if the window fills its screen (guards maximized check) |
| `track(window, name)` | Watch and persist this window's state |
| `compositorRounding()` | Hyprland's `decoration:rounding` (asked once through `hyprctl`), or -1 outside Hyprland or when it can't be read |

Property: `canMinimize` — false on tiling compositors that ignore minimize.
