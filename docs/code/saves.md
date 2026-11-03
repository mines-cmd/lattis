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
