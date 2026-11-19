# Heroic integration (`src/heroic/`)

The `src/heroic/` folder finds Heroic Games Launcher and reads its Epic files into plain structs. It never writes to Heroic's files and has no UI or library code. What the library does with the result is in [library.md](library.md#games-from-heroic), and launching is in [launch.md](launch.md#epic-games).

## Finding Heroic

`HeroicLocate::find()` takes a `HeroicLocations` (the home folder, `XDG_CONFIG_HOME`, `PATH` and Flatpak folders, so tests pass fake ones) and an optional `[heroic] path`. It returns a `HeroicInstall`: the root, native or Flatpak, and the program. Without a path it takes the first of `~/.config/heroic` and the Flatpak's folder that has a library file or an `installed.json`. When neither has, `found()` is false but `program` is still set if `heroic` is on the `PATH` or the Flatpak is installed, never to `xdg-open`: that's a Heroic with no library yet, as after a first start without signing in. An AppImage that isn't on the `PATH` can't be told apart from no Heroic. A `[heroic] path` that isn't a Heroic folder leaves `program` empty.

`findProgram()` tries `heroic` on the `PATH` and `flatpak run com.heroicgameslauncher.hgl`, the kind that was found first, then `xdg-open`, which is how an AppImage is reached through its `heroic://` handler. `HeroicProgram::noGui` is false for that last one, as it can't pass `--no-gui`. `launchArgs()` and `openArgs()` build the commands.

## Reading the files

All paths are under the root:

| File | What's read |
|---|---|
| `store_cache/legendary_library.json` | the whole library: app name, title, art URLs; DLC is left out |
| `legendaryConfig/legendary/installed.json` | what's installed, where, its size and its build (`version`, `platform`) |
| `legendaryConfig/legendary/assets.json` | the newest build per platform (`Windows[]`, `Mac[]`: `app_name`, `build_version`); Heroic and Legendary rewrite it |
| `store/timestamp.json` | total minutes and last played per app name |

`HeroicScan::scan()` returns a `HeroicState`. Passing the previous state skips parsing the library cache when its `HeroicStamp` (time and size) hasn't changed, since it can be tens of megabytes. The others are small and always read.

An installed game whose installed.json `version` differs from the `build_version` that assets.json lists for the same `app_name` on its `platform` (Windows when installed.json has none) gets `update = GameUpdate::Waiting`; `HeroicGame::version` is the installed build. No assets.json, one that can't be parsed (no warning: Heroic rewrites it), a game it doesn't list or an empty `version` all mean no update. `HeroicMerge` passes `update` on in `HeroicInstalled`, and the library sets `Game::update` from it (see [steam.md](steam.md#updates) for what the chip, arrow, Launch button and bar do with it).

The flags say what the absence of a game means. `libraryKnown` is only true for a readable cache that lists something, because Heroic can blank a store it can't read. `installedOk` and `playTimeOk` are true when the file was read or doesn't exist. Nothing here decides anything from them: that's `HeroicMerge`.

`HeroicState::games()` joins the two lists, with `installed.json` deciding what's installed, as the cache only changes on a refresh. `watchPaths()` is what `LibraryStore` watches: all four files. `lastPlayed()` reads one game's date for a running session. `cachedImage()` finds an image in `images-cache/`, where Heroic names a copy by the SHA-256 of its URL plus its resize parameters.

`HeroicListing` is the part of a scan the disk cache keeps. It's keyed by game id and holds no art URLs, so the first frame doesn't read them.

## Where a game keeps its saves

`HeroicSaves::read(root, appName, systemUser)` reads four files and writes nothing. The Wine prefix is the game's own `GamesConfig/<appName>.json` `winePrefix`, else `config.json`'s `defaultSettings.winePrefix` (a prefix can be shared by many games; a Proton prefix's `pfx` links back to the folder itself, so `drive_c` is directly in it). The Wine user is `steamuser` when the game's `wineVersion.type` (else the default's) is `proton`, and `systemUser` otherwise; a prefix can hold both folders, so the type decides, not what exists. Epic's save folder is `CloudSaveFolder` in Legendary's `metadata/<appName>.json`, returned as written. `autoSync` is `autoSyncSaves` (the game's own setting, else the default's) and `native` is installed.json's `platform == linux`. A file that isn't JSON adds a warning and counts as empty.

`expandCloudSaveFolder()` fills `{AppData}` (`AppData/Local`), `{UserDir}` (`Documents`), `{UserSavedGames}`, `{InstallDir}` and `{EpicID}` (`*`, a glob) and cleans the `..` out. It returns nothing when a variable can't be resolved (one under the prefix with no prefix, or an unknown one).

## A game's own settings

`HeroicGameConfig::read(root, appName, out)` returns the keys Lattis shows (`launcherArgs`, `showMangohud`, `useGameMode`, `enviromentOptions` (sic), `wrapperOptions`, `wineVersion`): the game's own value in `GamesConfig/<appName>.json`, else `config.json`'s `defaultSettings`, else empty. `launcherArgs` and `showMangohud` aren't in the file until set. A missing file and an empty `{}` entry (a game Heroic never configured) read as the defaults; a file that isn't a JSON object is refused with an error.

`write(root, appName, edits)` changes only the keys set in `HeroicGameEdits`. It patches the file's text instead of re-serialising it, so every other key, the key order and the indentation stay (Heroic puts new keys in its own order, and `QJsonObject` would sort them). A key that's absent is added after the entry's last one, and `wineVersion` is patched inside, so its `wineserver` stays. A missing file or an empty `{}` entry becomes a full entry: `defaultSettings` plus the edits, with `"version": "v0"` and `"explicit": true`. In that new entry the keys are sorted, as `defaultSettings` is read through `QJsonObject`. A file that isn't JSON is refused and left alone.

Heroic holds a game's config in memory and writes all of it back on its next change, so an edit made while it runs is lost (the spike confirmed it: an edit made to the file while Heroic ran was gone after Heroic saved something else); callers write only while Heroic is closed. `editsToJson()`/`editsFromJson()`, `merge()` (a later edit wins key by key), `apply()` (an edit laid over a `HeroicGameSettings`) and `isEmpty()` support the queue below.

`HeroicPresets::isOn()`, `turnOn()` and `turnOff()` apply a `SteamPreset` to a `HeroicGameSettings`: the built-in `mangohud` and `gamemode` presets set `showMangohud` and `useGameMode`; any other preset adds its `env` pairs to `enviromentOptions` (`{key, value}`), its `wrapper` to `wrapperOptions` (`{exe, args}`) and its `args` to `launcherArgs` (through `LaunchOptions`, as words after `%command%`).

`HeroicWines::find(root, steamToolDirs, systemWine)` lists the Wine builds a game can use: `<root>/tools/proton/*` (a `proton` file), `<root>/tools/wine/*` (`bin/wine`), the Steam tool folders' Proton builds and the system `wine`, each as a `HeroicWine` and its source.

`Library::gameForEditing()` adds `heroic` for an Epic game, read when Properties opens: `settings` (the keys above), `wines` (`label` and `wine`, the current one listed first with "(not installed)" when it is gone), `presets` and `error`. `heroicPresetOn()` and `setHeroicPreset()` work on that settings map, and `saveHeroicSettings(id, settings)` works out the keys that differ from what Properties showed (the file plus anything already waiting) and passes them to `applyHeroicEdits()`, the one place that writes to Heroic.

## Waiting for Heroic to close

`HeroicLocate::running()` says whether Heroic itself runs, native or Flatpak: a process under `/proc` whose program is named `heroic`, leaving out its helper processes (those with a `--type=` argument). A Heroic that has only its tray icon is still running, and counts. The `/proc` directory is a parameter for tests. `HeroicLocate::mainPids()` returns those processes' pids, which `GameRunner::closeHeroicAndApply()` sends SIGTERM (a Flatpak's pids are visible from outside too).

While Heroic runs, `saveHeroicSettings()` doesn't write: it merges the edits into `Library`'s per-app queue (`m_heroicPending`), keeps the queue in `<state dir>/heroic-pending-<data dir key>.json` (`{"<appName>": {"launcherArgs": …}}`, removed when empty) so it survives a restart, and a 2 s timer checks for Heroic having closed. Then `applyHeroicPending()` writes every waiting game through `applyHeroicEdits()`, logs it, and keeps an entry that failed to write. It also runs at startup (`setHeroicPendingFile()`), so edits left by a session that ended with Heroic open are applied if Heroic has since closed. Nothing is polled while the queue is empty.

A game's row has `pendingEdits` (`"heroic"` while the app name has an entry, `"steam"` for Steam's queue, else empty; on `Game` it's the enum `PendingLauncher::None/Steam/Heroic`), which the settings button's badge and tooltip use. The Launch card says "Waiting for Heroic to close to apply this", or before saving "Heroic is running: saving applies this when it closes" (the editor asks `Library.heroicRunning()` while it's open). `Library.heroicWaiting` lists the games waiting (`id`, `title`, `changes`), rebuilt by `updateHeroicWaiting()` whenever the queue or the games change; `TopNotices` puts them in the same banner as `Library.steamWaiting`. Unlike Steam there's no "close and apply" action: Lattis doesn't close Heroic.

`LATTIS_HEROIC_RUNNING=<1|0>` replaces the process check (`Library::setHeroicRunningCheck()`) for tests and screenshots.

## Legendary jobs

`LegendaryJobs` (`legendaryjobs.*`, owned by `GameRunner`, reached from QML as `GameRunner.legendary`) is the only place Lattis runs Legendary. It starts one process at a time, in the order jobs were queued, as `legendary <verb> <appName> [extra args] -y` with `LEGENDARY_CONFIG_PATH` set to the root's `legendaryConfig/legendary`. The program and config path come from a function given to the constructor, asked again for each job; no Heroic root or no Legendary (a Flatpak) means `enqueue()` is refused.

- **Kinds:** `Update`, `Install` and `Repair` are the jobs the UI lists; `Uninstall` and `Check` are hidden. A game has one job at a time (a second `enqueue()` for the same app name is refused); a failed one stays listed as `failed` until it is queued again or `dismiss()`ed.
- **`jobs`** (property, `jobsChanged`) is a list of `{appName, kind, percent, state, error}`; `state` is `queued`, `running` or `failed`; `job(appName)` gives one. `finished(Result)` follows every job: `ok`, `cancelled`, `started` (false when the process didn't start) and `error`, the line to show.
- **Progress** is read from the merged output, line by line: `LegendaryOutput::progress()` takes the percentage of `= Progress: 47.06% (272/578), …` (chunks, not bytes; the first line says 0.00%). The detail lines under it are dropped; the other lines are kept, 300 at most, and logged in full (`lattis.legendary`) when a run fails.
- **Cancel:** `cancel(appName)` removes a queued job, or sends SIGINT to the running one, as Ctrl-C does: Legendary stops its workers and keeps the resume file (`<config>/tmp/<appName>.resume`), so running the job again resumes. SIGTERM would kill only the parent. Legendary exits with 0 after SIGINT, so the job is marked cancelled, not done; one that ignores SIGINT for 10 s is killed. The destructor sends SIGINT too.
- **Failure:** `LegendaryOutput::failureLine()` is the last `ERROR:` or `CRITICAL:` line's text, else the last line that isn't a progress line; `GameRunner` shows it in a toast ("Couldn't update <title>: …") and answers an uninstall with it.
- **Install:** `enqueue(Kind::Install, appName, EpicInstall::installArgs(folder, platform))` runs `legendary install <app> --base-path <folder> --platform <Windows|Linux> --skip-sdl -y`; Legendary adds the game's folder name to the base path, and its progress lines are the same as an update's. A running or queued install is a job like any other (the list, the Launch button and the download bar read it through `Library::setEpicJobs()`).
- **Verify:** `GameRunner::verifyEpic(gameId)` queues `legendary repair <app> -y` (checks every file, downloads the broken ones; refused while the game runs, allowed while Heroic is open). Verification's progress is on stdout, one `Verification progress: N/219 (x%)` per carriage return, so the runner splits output on \r as well as \n and `LegendaryOutput::progress()` reads both formats; the download that follows restarts the percentage. `LegendaryOutput::repairedFiles()` counts the files in the `Verification failed, N file(s) corrupted, M file(s) are missing.` line (0 when the check found none) into `Result::repaired`; the toast is "<game>: all files are fine" or "<game>: fixed N files". The job sets its game Downloading or Queued with `Game::verifying` (shown as Verifying in the list, chip and info row, the Launch button and the download bar); Verify files is in the game's menu and in Properties → Launch, hidden without Legendary.
- **Size and space:** `EpicInstall::cachedInfo()` reads `download_size` and `disk_size` from Heroic's `store_cache/legendary_install_info.json` (only for games Heroic opened an install dialog for); without them `queryInfo(appName)` runs `legendary info <app> --json` beside the queue (stdout only; the answer is the `infoReady` signal with the JSON or the failure line) and `EpicInstall::parseInfo()` reads it. The platform is Linux when the info's `game.platform_versions` has a Linux build, else Windows. `freeSpace()` is the bytes free on the drive of the folder or its nearest existing parent. The folder offered is `[heroic] epic_install_folder` (the last one used in Lattis), else `config.json`'s `defaultSettings.defaultInstallPath`, else `~/Games/Heroic`.
- **Daily check:** `startDailyCheck(file, enabled)` (from `GameRunner::start()`, so after the first frame) looks 10 s later, then every hour, at `[heroic] check_updates` (on by default), whether a Legendary and a Heroic root were found, and whether the time in `<state dir>/epic-update-check-<data dir key>.txt` is over a day old. A due check runs `legendary list-installed --check-updates` as a hidden job; it is skipped, and tried again a minute later, while another job is queued or running. Legendary rewrites `assets.json` and `entitlements.json`, which the scan watches, so the chip and arrow follow without more code. The time is written only after a check that worked; a failure (offline) is logged only.

## Files

| File | Contents |
|---|---|
| `heroiclocate.*` | `HeroicLocations`, `HeroicInstall`, `HeroicProgram`; `find()`, `findProgram()`, `findLegendary()`, `launchArgs()`, `openArgs()` |
| `heroicscan.*` | `HeroicState`, `HeroicGame`, `HeroicPlayTime`, `HeroicListing`; `scan()`, `lastPlayed()`, `cachedImage()` |
| `heroicgameconfig.*` | `HeroicWine`, `HeroicGameSettings`, `HeroicGameEdits` (also `winePrefix`, only set by the Install dialog); `HeroicGameConfig::read()`, `write()`, `path()`, `defaultPrefixDir()` |
| `heroicpresets.*` | `HeroicPresets::isOn()`, `turnOn()`, `turnOff()`: a launch option preset in a game's Heroic settings |
| `heroicwines.*` | `HeroicWineChoice`; `HeroicWines::find()` |
| `heroicsaves.*` | `HeroicSavesInfo`; `HeroicSaves::read()`, `ownPrefix()`, `expandCloudSaveFolder()` |
| `heroicsettings.*` | `HeroicSettings` and its parser: `[heroic]` in `settings.toml` (`enabled`, `path`, `whole_library`, `check_updates`, `epic_install_folder`, `epic_install_wine`) |
| `legendaryjobs.*` | `LegendaryJobs`, `LegendaryOutput::progress()`, `failureLine()`: the Legendary job runner |
| `epicinstall.*` | `EpicInstallInfo`; `EpicInstall::parseInfo()`, `cachedInfo()`, `defaultFolder()`, `platformFor()`, `installArgs()`, `freeSpace()`, `expandHome()`: what the Install dialog reads |
