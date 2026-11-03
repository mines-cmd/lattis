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
