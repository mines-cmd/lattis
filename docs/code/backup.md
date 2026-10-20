# Backups (`src/backup/`)

The backup code creates and restores the two `.tar.zst` archives the user reaches from Settings → Library: a **library backup** of the data dir and a **saves backup** of every emulator's and Proton game's saves. It owns the archive format, the manifest that names what's inside, and Import, which reads a backup's manifest, plans what each part replaces, and applies the restore atomically. The library's files and the emulator definitions are in `src/library/`; the Proton shortcut data a restore carries over is in `src/steam/`.

## Contents

- [The two backups](#the-two-backups)
- [Building a plan](#building-a-plan)
- [Writing the archive](#writing-the-archive)
- [The manifest](#the-manifest)
- [Saves next to ROMs](#saves-next-to-roms)
- [Proton saves](#proton-saves)
- [Import: reading and planning](#import-reading-and-planning)
- [Import: applying](#import-applying)
- [What moves aside](#what-moves-aside)
- [The archive format](#the-archive-format)
- [Files](#files)
- [QML reference](#qml-reference)

## The two backups

A **library backup** takes the data dir with three categories of content left out or made optional:

- **Regenerated files** (the emulator reference file, the library cache) are always omitted; the app writes them again on the next start.
- **ROMs kept inside the data dir** — a ROM folder that is the data dir, or a game file's `rom` that points into it — are omitted too. ROMs belong to the user, not the launcher; most live outside the data dir anyway.
- **The API keys** (`credentials.toml`) are included by default but the user can opt out.

A **saves backup** takes three things per configured emulator: its save folders, its key and firmware folders (optional, on by default), and the saves each game's emulator writes next to the ROM. It also takes every Proton game's saves from its Wine prefix, also optional.

Both backups land in a single `.tar.zst` file chosen by the user (a `QUrl` from a save dialog). The default name is `lattis-library-<date>.tar.zst` or `lattis-saves-<date>.tar.zst`.

## Building a plan

Planning is a two-step process: make the plan (cheap), then measure it (slow, off the main thread).

`BackupPlan::planLibrary()` and `BackupPlan::planSaves()` read the library sources — every game, every emulator definition and install, Steam's `compatdata` folder — and build a `LibraryPlan` or `SavesPlan`. This is cheap: it stats a handful of files and records which folders and files will go in, but does not walk any of them.

`BackupPlan::measure()` then adds up the sizes. It walks each folder, follows no symlinks, and fills in each `Place::bytes`. It runs on a worker thread. `Backups` drives both steps from QML and exposes the in-progress plan as `libraryPlan` and `savesPlan`, so the dialog can show sizes as they arrive. The `bytes` fields start at −1 and become non-negative as each part is measured.

`BackupPlan::Settings` (`[backup]` in `settings.toml`) remembers the user's choices for credentials, emulator keys and Proton saves. `saveSetting()` edits the file in place; a key is removed when it's restored to its default (`true`), so the file only records opt-outs.

## Writing the archive

`Backup::write()` runs on a worker thread and is the only function that touches the filesystem outside the plan. It:

1. Measures the plan if it hasn't been yet (the dialog always measures first, but the function is self-contained).
2. Opens a `<path>.partial` file. A failed or cancelled write removes it; only once everything is in does `finish()` rename it into place. No half-written archive is ever left behind.
3. Writes `manifest.toml` as the first entry (so `Restore::open()` can read it without extracting the rest).
4. Adds the content: for a library backup, the data dir under `data/`; for a saves backup, each emulator's save and key folders under `emulators/<id>/saves/<n>/` and `keys/<n>/` (one number per folder in the definition's order, found or not), saves next to ROMs under `emulators/<id>/beside-roms/<game id>/`, and each Proton game's saves under `proton/<game id>/`.

The archive skips entries via `Archive::Skip` — a callback that returns true for paths to leave out. The library's skip function (from `LibraryPlan::skip()`) uses the plan's `excluded()` list. Proton's skip function (`ProtonSaves::skip()`) leaves out everything in the prefix that Proton itself owns.

`BackupJob` wraps `write()` for the background: it holds one `QFutureWatcher`, enforces at-most-one-at-a-time, and records the last backup of each kind in the state dir (`backups-<data dir key>.toml`) when it finishes.

## The manifest

Every archive starts with a `manifest.toml` entry written by `BackupPlan::manifest()`. It records:

- the format version (`BackupPlan::kFormat`), bumped when an older app can't restore the archive,
- the library id (so Import can tell whether the backup came from this library or another),
- the library format version,
- for saves: which emulators were included, with their install type at backup, their save and key source folders, and how many bytes each part held; which Proton games, with their shortcut app ID and Proton version.

`Restore::open()` reads only this first entry — cheap — and hands back a `Manifest`. It returns an error for a file that isn't a backup or a newer format, and a warning for a library backup whose format version is newer than this app can read (it opens, but only for viewing; writes stay blocked).

## Saves next to ROMs

Some emulators (mGBA is the example) write a save file right next to the ROM, with the same base name and a save extension (`.sav`). These follow the ROM, not the emulator's own save folder.

`BesideRom::find()` scans each emulator's games: for every game the emulator runs whose ROM is found on disk, it lists the files next to that ROM whose name matches `saves_beside_rom` (the definition's list of save extensions). It reads each folder only once. The result, a `QList<GameSaves>`, goes into `EmulatorPart::besideRoms` after `measure()`.

A restore maps backed-up saves back to where the ROM now is (`BesideRom::restorePath()`): next to the ROM's current path, with the game's current base name and the save's extension. This way a renamed ROM, or one on a new drive, still gets its saves back correctly.

## Proton saves

A Proton game's saves live in its Wine prefix: `steamapps/compatdata/<shortcut>/`. The prefix is not all saves, though. Proton owns a large portion of it: `tracked_files` lists the files Proton put there (DLLs, runtimes, the Windows filesystem skeleton), and GE-Proton lists its upscaler DLLs in `upscaler_files`. Everything else in `pfx/` is the game's, the registry included (games store settings and sometimes progress there even though Proton listed it).

`ProtonSaves::Prefix::read()` reads both lists. `classify()` then sorts each entry into `Save` (backed up), `Proton` (left out, carried over on restore) or `Drop` (locks, old registry copies, temporary files). The registry gets `Save` even though Proton tracks it, because games use it.

On a restore, the backed-up saves go into a prefix Proton has already made on this machine. `carryOver()` lists the Proton-owned entries from the current prefix, and `apply()` copies them in first, before the backup's files land on top. This way the prefix keeps the Proton version installed now — its DLLs and runtime files — while the game's saves overwrite whatever the current game state is.

A game whose prefix folder doesn't exist yet (`Skipped::NeverLaunched`), isn't installed (`Skipped::Uninstalled`) or whose Proton doesn't write `tracked_files` (`Skipped::Unlisted`) is listed in the plan but skipped in the archive.

## Import: reading and planning

`Restore::plan()` maps a manifest's emulators and games onto the current library:

- The library part maps to the current data dir.
- Each emulator's n-th save folder maps to that emulator's n-th folder for its install now. The emulator may have moved from Flatpak to AppImage between backup and restore; the folder numbers still match because they follow the definition's order.
- Saves beside ROMs map to each game's ROM's current location.
- Each Proton game's prefix maps to its shortcut's `compatdata` folder now.

Each mapping produces a `Target` with where the restored content goes (`path`) and whether something is already there (`exists`). A target that can't be restored — because its path is missing, or a link is in the way — gets a `problem` string explaining why.

`Backups::openImport()` calls `plan()` then measures what each target holds now, in the background, so the Import dialog can show "Replaces 1.2 GB" for each part before the user clicks Import. The measuring runs in a `QFutureWatcher`; the `importPlan` property updates when it finishes.

## Import: applying

`Restore::apply()` does the restore in three stages:

1. **Extract.** Every checked part is extracted into a staging folder next to where it will go (e.g. `data.import-<stamp>` next to the data dir). All parts are staged before anything is moved, so a failure at this step changes nothing.

2. **Carry over.** Proton prefixes get the current prefix's Proton-owned files copied in, and the data dir gets the user's ROMs and credentials.toml moved or copied in (if the backup doesn't have them) — these are the `carriedOver` paths.

3. **Rename into place.** Each staged folder is renamed to its destination. What was there first is renamed aside as `<path>.before-import-<date>`. The renames happen one at a time; if any fails, everything already moved is reversed. A failure changes nothing.

Only once all staged folders are in place does `apply()` clean up the (now-empty) staging directories.

A library Import restarts the app. After restart, the new library is loaded from the restored data dir. The restarted copy finds the `moved-aside` record in the state dir (written before the restart) and shows a notice.

## What moves aside

Every renamed-aside path (and for Proton, `compatdata.before-import-<date>/<shortcut>`) is recorded in `imports-<data dir key>.toml` in the state dir. `MovedAside` reads and writes this file and is the source of truth for:

- the notice shown on the main window after an Import, until the user dismisses it,
- the list in Settings → Library where the user can see and delete what moved aside.

Because a library Import restarts the app, the notice data must survive the restart; the state dir file does.

`present()` filters out entries whose paths no longer exist — they've been deleted or cleaned up — keeping only Imports that still have something there. An Import that moved nothing aside (nothing was at the destination) stays in the list while its notice is still showing, then goes away on dismiss.

## The archive format

Archives are `.tar.zst` with pax extended headers (for long names and metadata). Compression is zstd at level 3, on several threads. Symlinks are stored as links and never followed during write or extract. Owners are not stored; permissions and timestamps are.

`Archive::Writer` manages one `libarchive` write context. `addData()` writes bytes directly (for the manifest). `addTree()` walks a folder recursively, applying a `Skip` callback; it follows a folder that is itself a symlink (save folders are sometimes symlinked elsewhere) but does not follow links inside. `addPath()` adds a single entry without descending.

`Archive::extract()` extracts into multiple `Destination`s in one pass over the archive. It validates every entry:

- no absolute paths or `..` components,
- links that would escape their part fail the extract, unless `anyLinks = true` (the library's own links, which the user placed),
- entries that aren't files, folders or links also fail.

A failed or cancelled extract removes the folders it created.

## Files

| File | Contents |
|---|---|
| `backuparchive.*` | `Archive::Writer`, `extract()`, `readFirst()`, `removeTree()`: the `.tar.zst` format |
| `backupplan.*` | `planLibrary()`, `planSaves()`, `measure()`, `manifest()`: what each backup takes and how to describe it |
| `backup.*` | `Backup::write()`, `BackupJob`: writing the archive and tracking the last backup of each kind |
| `restore.*` | `Restore::plan()`, `apply()`, `RestoreJob`: Import planning and applying |
| `backups.*` | `Backups`: the QML singleton |
| `protonsaves.*` | `ProtonSaves::Prefix`, `classify()`, `skip()`, `carryOver()`: sorting a Proton prefix into saves and Proton's own files |
| `besiderom.*` | `BesideRom::find()`, `restorePath()`: saves next to ROMs |
| `movedaside.*` | `MovedAside::Import`, `record()`, `present()`, `remove()`: what each Import renamed aside |

## QML reference

`Backups` is created on first use, not at startup. Methods that change something return an error message to show, or `""` when it worked.

### Properties

| Property | |
|---|---|
| `running` | A backup is in progress |
| `kind` | `"library"` or `"saves"` while running |
| `bytes`, `total` | Progress: file data written so far and the plan's total |
| `part` | The part being written ("Ryubing's saves") |
| `libraryPlan` | The library plan for the dialog (see shape below) |
| `savesPlan` | The saves plan for the dialog (see shape below) |
| `choices` | `{credentials, keys, proton}` — `[backup]` in `settings.toml` |
| `last` | `{library, saves}` — each `{path, folder, text}`, or empty |
| `notice` | After an Import, until dismissed: `{text, folder, canDelete, places}`, or empty |
| `movedAside` | What past Imports moved aside, newest first (Settings → Library) |
| `importPlan` | The opened backup's plan for the Import dialog (see shape below) |
| `importing` | An Import is in progress |
| `importBytes`, `importTotal` | Import progress |

### Methods

| Method | |
|---|---|
| `planLibrary()` | Builds the library plan and measures it in the background |
| `planSaves()` | Builds the saves plan and measures it in the background |
| `setChoice(key, value)` | Saves `credentials`, `keys` or `proton` to `[backup]` |
| `defaultFile(kind)` | The save dialog's starting URL: last backup's folder + default name, else home |
| `start(kind, file)` | Writes the plan of `kind` to `file`; refused if already running or (for saves) a game runs |
| `cancel()` | Cancels the running backup |
| `gamesRunning()` | Why saves can't be backed up now, or `""` |
| `openImport(file)` | Reads the manifest and plans the Import; measures what it replaces |
| `checkImportPart(index, checked)` | Checks or unchecks a restorable part |
| `startImport()` | Restores the checked parts; refused if a game runs |
| `cancelImport()` | Cancels the running Import |
| `importFolder()` | Where the Import file picker starts |
| `dismissNotice()` | Hides the after-Import notice |
| `deleteMovedAside(index)` | Trashes what a `movedAside` entry holds |
| `deleteNoticed()` | Trashes what the notice's Import moved aside |

### Map shapes

`libraryPlan`:

```
measured          sizes are in
dataDir           with ~
games             count
credentials       {exists, bytes}
roms              [{path, bytes}]   paths with ~, relative to the data dir; bytes -1 until measured
dataBytes         the data dir without what's left out; -1 until measured
```

`savesPlan`:

```
measured
emulators   [{id, name, describe, skipped ("", "notInstalled", "noSaves"),
              saves: [path], keys: [path],
              savesBytes, keysBytes, besideRoms (count), besideBytes}]
proton      [{id, title, skipped ("", "neverLaunched", "uninstalled", "unlisted"), bytes}]
```

`importPlan`:

```
kind          "library" or "saves"
file          its file name
made          "2026-09-25, by Lattis 0.1"
warning       non-empty: the library is newer than this app reads
sameLibrary   the backup came from this library
games         games in the backup
gamesNow      games in the library now
dataDir       with ~
aside         the name the data dir gets when moved aside
carriedOver   paths moved or copied from the current data dir into the restored one
bytes         the checked parts' bytes (from the manifest)
anyChecked
parts   [{index, kind ("library", "saves", "keys", "besideRoms", "proton"),
          id, name, installText, bytes, problem, restorable, checked,
          targets: [{label, problem, exists,
                     bytes (what's there now; -1 until measured or not measured)}]}]
```
