# Launching games (`src/launch/`)

`src/launch/` starts games, opens emulators, and records play time when a game exits. It doesn't know how the game list is built or where game files live — it works through `Library` for that. Settings changes (`[launch]`) are picked up here too.

## How a launch works

Everything starts with `GameRunner::launch(gameId)`, which returns a message for a toast or an empty string when there's nothing to say. It looks up the game in the library, picks the right path for its platform (ROM, Steam, Proton, or Linux), and hands control to the appropriate helper. The outputs are logged to `<logDir>/launch/<game id>.log`.

`GameRunner` is a QML singleton, so QML calls it directly. `start()` must be called after the first frame to load settings without blocking startup.

## ROM games

`chooseEmulator()` picks the emulator to use: the game's pinned emulator if it's installed, otherwise the first installed emulator for the platform. If no emulator is installed, `launch()` returns a message saying which one to install, and `missingEmulator()` tells the Emulators page which one to highlight.

`buildLaunchCommand()` fills in the emulator's command template: `{exe}` becomes the emulator's program (or `flatpak run <app id>` for a Flatpak install), `{args}` becomes the emulator's argument list plus the game's `gameArgs`, spliced in, and `{rom}` becomes the ROM's path. Without a ROM (when opening the emulator without a game), `{rom}` and the arguments between `{args}` and it are left out — so a template like `-b -e {rom}` becomes just `-b`, not `-b -e`.

## Steam games

A Steam game is handed to Steam by running `steam://rungameid/<app id>`. With `-silent`, a running Steam starts the game without showing its window.

The calls that open Steam's own window (the downloads page, a game's page) pass `raiseWindow` to `runSteam()`. A running Steam can't take focus on Wayland without an activation token, so on Hyprland the runner also asks Hyprland to focus the `steam` window half a second later: `hyprctl dispatch 'hl.dsp.focus({ window = "class:^steam$" })'` (Lua, Hyprland 0.56), falling back to the legacy `focuswindow class:^steam$` when that fails.

After handing the game to Steam, `GameRunner` polls `/proc` for the game's process (via `SteamLocate::gamePid()`), then once found, watches it through a `pidfd` (via `SteamExitWatch`). This two-phase approach handles Steam starting up, shader caching, or running an update before the game appears: the game is given up to `kSteamStartSeconds` (90 s) to appear before its session is dropped. When the watched process exits, the game is given another `kSteamGoneSeconds` (5 s) to reappear — Proton games sometimes run an install script under the same app ID before the game itself starts. Steam records the play time itself; the launcher doesn't.

When a Steam game has launch option edits queued and Steam is running, those edits can't be applied yet (Steam would overwrite them). `restartSteamAndLaunch()` handles this: it closes Steam, writes the queue once Steam is gone, then starts Steam again with the game. The same flow powers the "Close Steam and apply" button (`closeSteamAndApply()`). Both use `SteamExitWatch` to know when Steam is gone, and give up after `kSteamCloseSeconds` (30 s) with a message.

## Proton games

A Proton game is a non-Steam shortcut — it runs through Steam the same way as a Steam game, but using its shortcut's 64-bit game ID rather than an app ID. The game must be located (its `install_dir` set, or its program file found) before it can launch; a game that isn't installed says so and points the user to locate it.

## Linux games

A Linux game runs its program directly through `DetachedProcess`, with its launch options applied: the wrapper runs first (e.g. `gamemoderun ./game`), then the program, then the trailing arguments. The working directory is `start_dir` from the game file, falling back to the program's folder.

Linux games need special session tracking because launcher scripts are common: a script starts the real game binary and exits. `DetachedProcess` can be told to follow the session (`setFollowSession(true)`): after the program exits, it polls `/proc` every `sessionPollInterval()` ms to check whether any non-zombie process is still in the session (which was created with `setsid`). The game is counted as running until the session is empty.

A program that exits with an error sooner than `kQuickExitSeconds` (10 s) didn't really start; that's treated differently from a session that runs to completion.

## Sessions and play time

`GameRunner` has two separate session maps: `m_sessions` for ROM and Linux games (each holds a `DetachedProcess`), and `m_steamSessions` for Steam and Proton games (each holds a `SteamSession`). The union of their keys is `running()`, the QML property.

`forceClose()` kills with SIGKILL, so the usual exit handling follows: a ROM or Linux game's session is recorded as any other. `DetachedProcess::killSession()` kills every process in the session (it was created with `setsid`); `killTree()` kills a process and its descendants, for Steam games, where the launch process is Steam's, not the game's own session.

When a non-Steam session ends, the runner measures its length. Sessions shorter than `minSessionSeconds` (default 30, configurable in `[launch]`) are too short to count — a crash on startup shouldn't affect play time or last-played date. Longer sessions add their minutes to the game file's `play_minutes` and update `last_played`. `Library::recordPlaySession()` also appends the session to the game's play history (`library/history/<id>.toml`), in the same call, so the history holds exactly the sessions that count. Steam and Proton games record nothing here: their history is made from Steam's play time totals (see [library.md](library.md#play-history)).

## The environment

`programEnvironment()` builds the environment for a launched program. It starts from the launcher's own environment, then applies the command's variables on top. When the launcher runs as an AppImage, `programEnvironment()` strips what AppImage adds — `APPDIR`, `APPIMAGE`, `ARGV0`, `OWD`, and the folders inside `APPDIR` that AppImage injected into `PATH`, `LD_LIBRARY_PATH`, `QT_PLUGIN_PATH` and similar — so games and emulators don't accidentally load the launcher's Qt or libraries.

## Settings

`[launch]` in `settings.toml` has two keys:

- `while_running`: what the main window does while a game runs. `"minimize"` minimizes it; `"stay"` (the default) leaves it open. Hyprland doesn't support minimize, so it stays regardless.
- `min_session_seconds`: sessions shorter than this don't count toward play time or last-played (default 30, range 0–3600).

`GameRunner::start()` reads the file and watches it; edits take effect without a restart.

## Files

| File | What it does |
|---|---|
| `gamerunner.*` | `GameRunner` singleton: launching all game types, session tracking, play-time recording, Steam restart flow, settings |
| `detachedprocess.*` | `DetachedProcess`: starts a program that outlives the app, watches it via pidfd, optionally follows its session |
| `launchcommand.*` | `LaunchCommand` struct and `buildLaunchCommand()`, `buildProgramCommand()`, `programEnvironment()` |
| `launchoptions.*` | Parsing Steam launch options strings into env vars, wrapper and arguments |
| `launchsettings.*` | `LaunchSettings` struct and `parseLaunchSettings()` |

## QML reference

### `GameRunner` properties

| Property | Type | Description |
|---|---|---|
| `running` | `QStringList` | IDs of the games running now |
| `minimizeWhileRunning` | `bool` | `while_running = "minimize"` is set |
| `minSessionSeconds` | `int` | Minimum session length that counts |
| `problems` | `QStringList` | Settings parse errors |
| `closingSteam` | `bool` | `closeSteamAndApply()` is waiting for Steam to close |

### `GameRunner` invokables

| Method | Returns | Description |
|---|---|---|
| `launch(gameId)` | `string` | Launch a game; toast message or `""` |
| `restartSteamAndLaunch(gameId)` | `string` | Close Steam, apply queued edits, launch; toast or `""` |
| `openEmulator(gameId)` | `string` | Open the emulator without the game (or Steam's game page) |
| `openFolder(gameId)` | `string` | Open the game's folder in the file manager |
| `saveFolder(gameId)` | `string` | The game's save folder, or `""`: the first that exists of its own `save_folder`, then `foundSaveFolder()`'s rules |
| `foundSaveFolder(gameId)` | `string` | The save folder looked for, ignoring `save_folder`: for a ROM game, the ROM's folder if its emulator (the one it would launch with, else its first) has `saves_beside_rom`, then the first existing entry of the emulator's `saves` for the install found; for a Proton game, its prefix's `pfx/drive_c/users/steamuser` (`Library::protonPrefix()`); for a Steam game, `compatdata/<appid>/pfx/drive_c/users/steamuser` in the first Steam library that has it. Linux games have none |
| `openSaveFolder(gameId)` | `string` | Open `saveFolder()` in the file manager; toast or `""` |
| `shownPath(path)` | `string` | The path with `~` for the home folder, as written to the game files |
| `openDataFolder()` | `string` | Open the data dir in the file manager |
| `openFile(path)` | `string` | Open a file in its default app; a terminal editor gets a terminal window. The path on failure |
| `uninstall(gameId)` | `string` | Uninstall a Steam game through Steam |
| `forceClose(gameId)` | `string` | End a running game with SIGKILL: its whole session (ROM and Linux games), or the process tree under Steam's launch process (Steam and Proton games). Returns a message if a Steam game has no process yet |
| `uninstallProton(gameId, files, prefix, shortcut, action, permanent)` | `map` | Uninstall a Proton or Linux game; `{error}` or `{trashless}` |
| `missingEmulator(gameId)` | `string` | Emulator ID to install for a ROM game with none installed, or `""` |
| `closeSteamAndApply()` | `string` | Close Steam, apply queued edits, restart it; toast or `""` |
| `steamRunning()` | `bool` | Whether Steam is running |
| `setLaunchValue(key, value)` | `string` | Write a `[launch]` key to settings.toml; error or `""` |

#### `setLaunchValue` keys

| Key | Values | Default |
|---|---|---|
| `while_running` | `"stay"` / `"minimize"` | `"stay"` |
| `min_session_seconds` | `"0"`–`"3600"` | `"30"` |

### `LaunchOptions::Parts` (map keys)

```
env       list of VAR=value pairs set in the process environment
wrapper   program to run with the game's command after it (e.g. gamemoderun)
args      arguments appended after the game's command
problem   why the string can't be run as is; empty when none
```
