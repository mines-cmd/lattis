| `heroicsaves.*` | `HeroicSavesInfo`; `HeroicSaves::read()`, `expandCloudSaveFolder()` |
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
| `legendaryConfig/legendary/installed.json` | what's installed, where, and its size |
| `store/timestamp.json` | total minutes and last played per app name |

`HeroicScan::scan()` returns a `HeroicState`. Passing the previous state skips parsing the library cache when its `HeroicStamp` (time and size) hasn't changed, since it can be tens of megabytes. The other two are small and always read.

The flags say what the absence of a game means. `libraryKnown` is only true for a readable cache that lists something, because Heroic can blank a store it can't read. `installedOk` and `playTimeOk` are true when the file was read or doesn't exist. Nothing here decides anything from them: that's `HeroicMerge`.

`HeroicState::games()` joins the two lists, with `installed.json` deciding what's installed, as the cache only changes on a refresh. `watchPaths()` is what `LibraryStore` watches: all three files. `lastPlayed()` reads one game's date for a running session. `cachedImage()` finds an image in `images-cache/`, where Heroic names a copy by the SHA-256 of its URL plus its resize parameters.

`HeroicListing` is the part of a scan the disk cache keeps. It's keyed by game id and holds no art URLs, so the first frame doesn't read them.

## Where a game keeps its saves

`HeroicSaves::read(root, appName, systemUser)` reads three files and writes nothing. The Wine prefix is the game's own `GamesConfig/<appName>.json` `winePrefix`, else `config.json`'s `defaultSettings.winePrefix` (a prefix can be shared by many games; a Proton prefix's `pfx` links back to the folder itself, so `drive_c` is directly in it). The Wine user is `steamuser` when the game's `wineVersion.type` (else the default's) is `proton`, and `systemUser` otherwise; a prefix can hold both folders, so the type decides, not what exists. Epic's save folder is `CloudSaveFolder` in Legendary's `metadata/<appName>.json`, returned as written. A file that isn't JSON adds a warning and counts as empty.

`expandCloudSaveFolder()` fills `{AppData}` (`AppData/Local`), `{UserDir}` (`Documents`), `{UserSavedGames}`, `{InstallDir}` and `{EpicID}` (`*`, a glob) and cleans the `..` out. It returns nothing when a variable can't be resolved (one under the prefix with no prefix, or an unknown one).

## Files

| File | Contents |
|---|---|
| `heroiclocate.*` | `HeroicLocations`, `HeroicInstall`, `HeroicProgram`; `find()`, `findProgram()`, `launchArgs()`, `openArgs()` |
| `heroicscan.*` | `HeroicState`, `HeroicGame`, `HeroicPlayTime`, `HeroicListing`; `scan()`, `lastPlayed()`, `cachedImage()` |
| `heroicsettings.*` | `HeroicSettings` and its parser: `[heroic]` in `settings.toml` |
