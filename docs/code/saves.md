# Saves

Finding where each game keeps its saves, from the [Ludusavi manifest](https://github.com/mtkennerly/ludusavi-manifest) (MIT; its data comes from PCGamingWiki). Nothing here runs before the user turns the feature on, and nothing is parsed at startup.

## savesindex

`SavesIndexFile::parse(yaml)` reads the manifest into a `SavesIndex`: per game with saves its `name`, `aliases` (other manifest entries that are only `alias: <name>`), `steamId`, `gogId`, `installDirs` and `paths`. A path is the manifest's text with its placeholders (`<home>`, `<base>`, `<winAppData>`…) and the `when` constraints (`os`, `store`; empty = any) that apply to Linux or Windows.

Kept: `files` entries tagged `save` (or untagged). Left out: entries tagged only `config`, entries only for macOS or DOS, the registry, games with no kept path. A file that is not a manifest (not YAML, not a map, no games) is `ok = false` with a warning; one unreadable game is skipped and counted in the warnings.

`SavesIndexFile::save(path, index)` / `load(path)` write and read the index with `QDataStream` (atomically). `load` returns nothing for a missing, cut or other-format file; bump `kFormatVersion` when what `parse` keeps changes.

## savesmanifest

Files in `<state>/saves-manifest/` (`defaultDir()`): `manifest.yaml` (the download), `index.bin` and `meta.json` (`etag`, `fetched`, `checked`). It is in the state dir, not the data dir, so a library backup skips it.

- `url()` is the public manifest, or `LATTIS_SAVES_MANIFEST_URL` (`file://` works).
- `update(dir, url, force, timeoutMs)` blocks, so call it from a worker thread, and only on the user's click or the daily check. It sends `If-None-Match` unless `force`; a 304 only moves `checked` (`Unchanged`). A new body is parsed first, and only a real manifest is written (`Updated`); anything else is `Failed` with `error` and leaves the files as they were. An index in an older format is made again from `manifest.yaml` without the network.
- `readMeta(dir)`, `checkDue(meta)` (never checked, or more than a day ago), `loadIndex(dir)`.

## savesfinder

Pure functions over a `SavesIndex`; nothing here knows the library, so a caller fills a `Query` per game.

- `Env` is what placeholders expand from: `home`, `xdgData`, `xdgConfig`, `user`, `steamRoot` (empty: `<root>` paths are skipped). `Env::system()` reads them from the environment.
- `Query` is a game: `id`, `title`, `steamAppId` (0 = none), `savesGame` (the `saves_game` from its file), `baseDir` (its install folder, for `<base>` and `<game>`), `prefix` (its Proton prefix, `…/pfx`; empty = it runs natively), and its own `saveFolder` and `savePaths` (`~` allowed).
- `Matcher(index)` holds name lookups built once; `find(query, &how)` returns the manifest game or null. `savesGame` wins (`Match::Chosen`; a name the manifest no longer has matches nothing). Else the Steam app ID (`SteamId`), else the folded title (`SearchText::fold`) against names and aliases (`Title`); an alias never takes a name another game has.
- `find(matcher, copies, env)` returns `Found { gameId, matched, match, paths }`. `copies` are the games of one `same_as` group, the main copy first: their paths are found together and joined into one list, named after the first. `paths` are absolute, exist, sorted and unique, with none inside another found folder.

How a path is found:

- A query with a `prefix` is a Proton game: entries for Windows (or no OS) expand into `<prefix>/drive_c` (`<home>` and `<winDocuments>` etc. in `users/steamuser`, `<winPublic>`, `<winProgramData>`, `<winDir>`, `<winDrive>`), and names are matched in any case, as Wine does. Without one, entries for Linux (or no OS) expand with `<home>`, `<xdgData>`, `<xdgConfig>`.
- `<base>` is `baseDir`; `<game>` its folder name and the manifest's `installDir` names; `<storeGameId>` the Steam app ID; `<storeUserId>` is `*`. A placeholder with no value for the game (a Windows one for a native game, `<base>` with no `baseDir`) drops the path. The manifest's `store` constraint isn't used: a path that exists is the game's.
- `*`, `?` and `[…]` match inside one name. A match that is a link is returned as itself but never entered; a link that is the home or the prefix path itself is followed by the system as usual.
- Refused: paths with `..`, paths that aren't absolute, and the home, XDG folders, the prefix's `drive_c` and `users/steamuser` themselves.
- A query's own `saveFolder` and `savePaths` are expanded the same way, for any match or none, in the home rather than the prefix.

## savesbackup

`SavesBackup::run(input)` makes one game's backup and blocks, so it runs off the main thread. `Input` is the group's main copy `gameId` and `title`, the `copies` (queries) for `SavesFinder::find()`, `env`, `manifestDir`, `options` (`folder`, `keep`, `maxBytes`), `appVersion`, `now`, `trash` (empty = the system trash) and `protonPrefix`, a launcher Proton game's compatdata folder.

- The index is loaded from `manifestDir` (none: only own paths count). Nothing found and no `protonPrefix` (or one without `tracked_files`): `Status::Nothing`, and no folder is made. With `protonPrefix` and no found path, the prefix is backed up the way the saves backup does (`ProtonSaves::skip`).
- The files are listed (names, sizes, modification times, links not followed). More than `maxBytes`: `TooBig`. A SHA-1 of that list equal to the `signature` in the newest archive's `manifest.toml`: `Unchanged`.
- Else `<folder>/<game id>/<yyyy-MM-dd-HHmmss>.tar.zst` is written through `<name>.partial` and renamed: `manifest.toml` (kind `game-saves`, format, `app_version`, `created`, `game`, `title`, `matched`, `files`, `bytes`, `signature`, and a `[[path]]` per source with `n`, `from` (`~` for the home), `type` and, for a whole prefix, `prefix = true`), then each source under `files/<n>/`. A failure is `Failed` with `error`, and leaves no partial file. After a write all but the newest `keep` go through `trash`; one that can't be trashed is only logged.
- `backups(folder, gameId)` lists a game's archives, newest first.

`SavesBackup::Service` is the glue: it counts a session from `started()` to `ended()`, and one shorter than `minSessionSeconds` makes no backup. The game is queued then and starts once `busy` (the game or one of its mod managers still running) says no, which `ended()` of any session re-checks. `gather` (main thread) returns the `Input` or nothing when automatic backup is off for the game; `savesbackuplibrary.cpp` builds it from the library: the game's `auto_backup` (`true`/`false`; absent = its platform's `auto_backup` in `platforms.toml`, off by default), its copies, Proton and Steam prefixes, and `[backup]` `auto_folder` (default `~/.local/share/lattis-saves`), `auto_keep` (5) and `auto_max_mb` (1024). Only Steam, Proton and Linux games are considered. The work runs with `QtConcurrent`; `message` carries a failure or a skipped-for-size notice (main.cpp forwards it to `GameRunner::message`, the toast), and `finished` every result. The destructor waits for running backups.

## savessettings

`SavesSettings` (QML singleton, made on first use; `setup({dataDir, manifestDir})` at startup) is what Settings → Library → Game saves and the choices read and write.

- `status`, `downloaded`, `updating`: the manifest's state from `meta.json`. `update()` forces a download on a worker thread; `ensureManifest()` downloads only when there is none, and is what turning automatic backup on (a platform's switch, a game's "On") calls, so the manifest is never fetched before that.
- `folder` (the default `~/.local/share/lattis-saves` when unset) and `keep`; `setFolder()` and `setKeep()` write `[backup]` `auto_folder` and `auto_keep` through `BackupPlan::saveAutoFolder` and `saveAutoKeep` (the default is the key's absence) and return an error or nothing.
- `startDailyCheck(anyOn)` (main.cpp, after the first frame): ten seconds later, then every six hours, it updates the manifest when `anyOn()` (`SavesBackup::anyOn`: a platform or game has it on) and the last check is over a day old.

The choices: a platform's `auto_backup` is written by `Library.setPlatformValue(id, "auto_backup", on)` (absent = off; `platformSettings()` returns it as `autoBackup`). Steam, Proton and Linux show a switch on their row, which first adds the platform to `platforms.toml` when it only exists as a built-in. A game's is `auto_backup` in its file, set through `saveGame` (`true`, `false`, or cleared to follow the platform); `gameForEditing()` returns `autoBackup` (-1, 0, 1) and `platformAutoBackup`. `SavesBackup::autoBackupResolved(own, platformOn)` is the rule.

## gamesaves

`GameSaves` (QML singleton, made on first use; `setup({input, choose})` at startup) is what a game's page reads. `input(gameId)` is `SavesBackup::inputFor(…, ignoreChoice = true)`: the backup input whatever the automatic setting, or nothing for a game that is no Steam, Proton or Linux game (or a ROM game). `choose(gameId, name)` writes `saves_game` through `Library.saveGame` (an empty name clears it).

- `load(gameId)` reads on a worker thread (the index once, kept for the session; the finder; the backups folder) and fills `state`: `requested` (the game asked for), `eligible`, `id` (the group's main copy, whose folder holds the backups), `title`, `hasManifest`, `matched` (the manifest name) and `match` (`steam`, `title`, `chosen`), `paths` (found now, `~` for the home), `fallback` (nothing found, so a launcher Proton game's whole prefix is what is backed up), `backups` (`[{path, when, size}]`, newest first), `folder`, `keep`. A later `load` wins over an earlier one still running.
- `backUpNow(gameId)` runs `SavesBackup::run` on a worker thread; `working` is true meanwhile and `message` says what came of it (written, unchanged, too big, nothing found, failed). It reloads the state afterwards.
- `search(text)` is the names in the loaded index that contain every word of `text` (folded like the library search; at most 30), for the "Choose game" popup; `choose(gameId, name)` saves the choice and reloads.

The page's **Restore** is `Backups.restoreGameSaves(path)`; see [backup](backup.md#import-reading-and-planning). The `game-saves` kind is read by `Restore` and the Import dialog the same way, so a backup copied to another machine restores there.
