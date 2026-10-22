# Installing emulators (`src/install/`)

The install code puts an emulator on the computer from inside the app, as the upstream AppImage or as the Flathub Flatpak, remembers that Lattis did it, and can update or remove what it installed. It never touches detection: an install ends by asking the library to look for emulators again, and the library finds the new file (or Flatpak) like any other.

## Contents

- [Where a download comes from](#where-a-download-comes-from)
- [Releases](#releases)
- [The download](#the-download)
- [Flatpak](#flatpak)
- [The install record](#the-install-record)
- [The Installer](#the-installer)
- [Updates](#updates)
- [Uninstall](#uninstall)
- [Testing hooks](#testing-hooks)
- [Files](#files)
- [QML reference](#qml-reference)

## Where a download comes from

Three emulator keys (see [library](library.md#emulators)) name it:

- `release`: `github:<owner>/<repo>` or `gitlab:<host>/<group>/<project>`.
- `release_asset`: the file's name pattern in the latest release (case-insensitive, `;` separates patterns in order of preference).
- `release_inside`: for a zip or tarball, the pattern of the AppImage inside it.

An emulator with `release` and `release_asset` can be installed as an AppImage. Every built-in emulator has them except Dolphin, which publishes no AppImage. A value that doesn't parse is a warning when the file is read and the key is ignored. An emulator with a `flatpak` id can be installed as a Flatpak.

## Releases

`releasesource.*` is pure: no network, no files.

| Function | What it does |
|---|---|
| `ReleaseSources::parse(text, &error)` | A `ReleaseSource` (host, server, project) from a `release` value, or why not. Names are checked: no `..`, no `@`, a GitLab host needs a dot |
| `ReleaseSource::apiUrl()` | GitHub: `https://api.github.com/repos/<project>/releases/latest`. GitLab: `https://<server>/api/v4/projects/<project, percent-encoded>/releases?per_page=1` |
| `ReleaseSource::label()` | "GitHub", or the GitLab server's name, for the interface |
| `ReleaseSources::parseRelease(source, json, &error)` | A `Release`: tag, date and the assets (id, name, URL, size, sha256, date). GitHub's answer is one release; GitLab's is a list whose first entry counts. An API error body (`{"message": …}`) becomes the error. GitHub's `digest` (`sha256:…`) is the checksum; GitLab gives none, and uses an asset's `direct_asset_url` when it has one |
| `ReleaseSources::matches(name, patterns)` | `*` and `?` patterns, anchored, case-insensitive, `;`-separated |
| `ReleaseSources::pickAsset(release, patterns)` | The first pattern that matches any asset wins, then the asset's place in the release |

## The download

`AppImageDownload` (`appimageinstall.*`) takes the network manager, one asset, the `release_inside` pattern and the target folder, and ends with a `Result` (`ok`, `cancelled`, `path`, `error`, `sha256`, `replaced`). Its stages are Downloading, Extracting (zips and tarballs only) and Installing.

1. Only an `https` URL is requested; redirects may not go to a less safe scheme (`NoLessSafeRedirectPolicy`). `setRelaxed(true)` also allows `http` and `file` URLs, for the tests and the fixtures.
2. The bytes go to `.<name>.partial` in the target folder while a SHA-256 is computed. At the end the size must match the asset's, when the release lists one, and the hash must match its `sha256`, when it has one. A mismatch deletes the file and fails.
3. A zip or tarball is unpacked on a worker thread by `AppImageInstall::extractOne()`: one entry, the first regular file whose name matches `release_inside`, written to `.<name>.partial`. An entry whose path is absolute or holds `..` makes the whole archive refused; a matching entry that is a link is refused; nothing larger than 2 GiB is written. The archive's partial file is then deleted.
4. The AppImage is made executable (0755) and renamed to its upstream name. A file with that name already in the folder fails the install, unless `setReplace(true)` (an update): then the old file is first moved to `.<name>.replaced/<name>` beside it, the new one renamed in, and `Result::replaced` names the moved file for the caller to trash. If the new file can't be put in place, the old one is moved back.
5. A failure or `cancel()` deletes every partial file. Destroying the object does too.

`setPace(bytes, tickMs)` reads that many bytes per tick instead of everything that has arrived, so a screenshot can catch the progress.

`AppImageInstall::targetFolder(applicationsDirs, home)` is the first of `~/Applications` and `~/AppImages` that exists, else `~/Applications`. Both are already searched by detection, so nothing is written to `emulators.toml`. An update goes to the folder the old file is in.

## Flatpak

`flatpakinstall.*`:

| Function | What it does |
|---|---|
| `FlatpakInstall::program()` | `$LATTIS_FLATPAK` when set, else `flatpak` on `PATH`; empty when there is none (the Flatpak route isn't offered) |
| `FlatpakInstall::validId(id)` | Letters, digits, `_`, `.` and `-`, not starting with `-`: the only ids passed to `flatpak` |
| `FlatpakInstall::installSteps(id)` | `flatpak remote-add --user --if-not-exists flathub <flatpakrepo URL>`, then `flatpak install --user --noninteractive -y flathub <id>` |
| `FlatpakInstall::uninstallStep(id)` | `flatpak uninstall --user --noninteractive -y <id>`; the app's data in `~/.var/app` stays |
| `FlatpakInstall::failureText(stderr, stdout, code)` | The last non-empty line of stderr, else of stdout, else "flatpak stopped with code N" |

`FlatpakJob` runs a list of steps one after another in a `QProcess` and stops at the first failure. `finished(Result)` has `ok`, `cancelled` and `error`; a program that won't start is "Couldn't run flatpak: …". `cancel()` terminates the running step (and kills it after 3 s); destroying the job kills it. The environment given is the program environment (without the AppImage's own variables), or the app's own when empty.

Without a terminal `flatpak install` prints no percentages, so a Flatpak job has no progress (-1).

## The install record

`~/.local/state/lattis/installs.toml` (`installrecord.*`) says what Lattis installed on this computer, one table per emulator id: `route` (`appimage` or `flatpak`), `file` (the AppImage, or the Flatpak id), `tag`, `sha256`, `asset_id`, `asset_date` and `installed` (the date). A Flatpak record has only `route`, `file` and `installed`. It is in the state dir, not the data dir, because it describes this machine: a backup doesn't carry it.

`InstallRecords::stillThere(record, userFlatpaks)` is what "installed by Lattis" means: the record exists and its AppImage is still a file (or its Flatpak is still in the user installation). A file Gear Lever moved stops counting, and the emulator shows only as found. An unreadable file is a warning and counts as empty; entries without a `route` and a `file` are skipped.

## The Installer

`Installer` (`installer.*`) is the QML singleton, created on first use like `Backups`; `Installer::setup()` at startup gives it the state and cache dirs and callbacks: the emulator definitions, the install locations, "look for emulators again" (`Library::scanNow()`), the environment `flatpak` runs with, the trash (`Uninstall::systemTrash`) and "the title of a game running in the emulator with this name" (`GameRunner::runningIn`). The last three are optional. Nothing is made, and nothing reaches the network, until QML uses it.

Each emulator has at most one job; several emulators can have one at once. A job has a kind (install, update, check, uninstall) and a state. A failure keeps its message until the next job for that emulator or `cancel()`; `retry(id)` runs the failed job again (the same kind, and for an install the same route). `cancel(id)` stops a job at any step and deletes what it wrote; quitting cancels them all.

`install(id, route)` with route `appimage` (the default): Checking (the release is requested), then Downloading, Extracting and Installing from `AppImageDownload`. With route `flatpak`: Installing, for both steps. When a job ends, the record is written and the library is asked to detect again. Any other route fails.

`emulators` (a `QVariantMap`, id to a map) has a row for each emulator that can install by either route or has a record or a job:

```
state           "idle", "checking", "downloading", "extracting", "installing", "removing" or "failed"
progress        0 to 1 while downloading, -1 when unknown
error           the message of a failure
jobRoute        "appimage" or "flatpak" while a job exists (for its step text), else ""
routes          ["appimage", "flatpak"]: those on offer, AppImage first; flatpak only when the program exists
noFlatpak       the emulator has a Flatpak id but flatpak isn't installed
source          where the AppImage comes from: "GitHub" or a server name
installedByApp  the record exists and its file (or Flatpak) is still there
route           the installed route, when installedByApp
file            the AppImage's path or the Flatpak id, when installedByApp
installed       the date it was installed, when installedByApp
version         the recorded release tag, when installedByApp (empty for a Flatpak)
updateVersion   the newer release's tag (its date without a tag), "" when none is known
upToDate        an installed AppImage checked this session (or just installed) with nothing newer
project         the release's project ("mgba-emu/mgba"), for "installed from"
size            the download's size in bytes while downloading, else 0
canCancel       false only during an AppImage's short Installing step (the rename into place)
```

State changes update it at once; progress is coalesced to about 12 updates a second. `refresh()` re-reads the record, looks for `flatpak` again and checks the files again; the Emulators page calls it, then `checkUpdates()`, when shown.

## Updates

Only for an AppImage Lattis installed; Flatpaks are updated by the system. Nothing is checked at startup or in the main window.

`checkUpdates(id)` fetches the latest release of one emulator (state `checking`, an error shown in the row), and `checkUpdates()` of every installed AppImage, quietly (a failure is only logged). The answer is cached an hour in `<cache dir>/releases/<release value with ":" and "/" as "-">.json` (GitHub allows 60 unauthenticated requests an hour); only checks read the cache, an update always asks again.

A release counts as newer when the matching asset's sha256 differs from the recorded one, or, when either side has none (mGBA's release lists no digest), when its id or date differs. Not by tag: DuckStation's is always `latest`. `updateVersion` then holds the release's tag.

`update(id)` runs the install flow into the folder the old file is in, in replace mode. When the new file is in place and the record is written, the old file goes to the trash: the moved-aside copy when the name was the same, else the recorded file. A failure leaves the old file and its record as they were. Refused with "Close <game> first" while a game runs in that emulator.

## Uninstall

`uninstall(id)` works only on what Lattis installed (the record, still there): "<emulator> wasn't installed by Lattis…" otherwise. An AppImage goes to the trash; a Flatpak is removed with `flatpak uninstall` (state `removing`) and keeps its data. The record is deleted and the library detects again. Refused with "Close <game> first" while a game runs in that emulator.

## Testing hooks

`LATTIS_INSTALL_FIXTURES=<dir>` makes the Installer read the release from `<dir>/<release value with ":" and "/" as "-">.json` (`github-mgba-emu-mgba.json`) after a short delay, and download the asset from `<dir>/<asset name>` as a `file:` URL. With no such file it makes a script that sleeps (2 MB, no checksum) under the cache dir. The download is paced (40 steps of 60 ms), so the progress shows. Fixtures skip the release cache.

`LATTIS_INSTALL=<id>[/<route>],…` starts those installs three quarters through a screenshot's delay, before `LATTIS_OPEN_SETTINGS` opens the Emulators page, so the page shows them running (with the fixtures, an emulator with no canned release shows a failure).

`LATTIS_FLATPAK=<program>` runs that program instead of `flatpak` (a script; the tests' logs its arguments and makes or removes the app's folder in the fake user installation).

Tests and screenshots set `HOME` to a scratch folder, so nothing is installed for real.

## Files

| File | What it holds |
|---|---|
| `releasesource.*` | `ReleaseSource`, `Release`, `ReleaseAsset`; parsing a `release` value and the two APIs' JSON; asset patterns |
| `appimageinstall.*` | `AppImageDownload`; `targetFolder()`, `isArchive()`, `extractOne()` |
| `flatpakinstall.*` | `FlatpakJob` and the `FlatpakInstall` helpers |
| `installrecord.*` | `InstallRecord`; reading, writing and judging `installs.toml` |
| `installer.*` | The `Installer` singleton |

## QML reference

```
Installer.emulators           the map above
Installer.install(id, route)  route defaults to "appimage"; "flatpak" is the other
Installer.update(id)
Installer.uninstall(id)
Installer.checkUpdates(id)    no id: every installed AppImage, quietly
Installer.retry(id)           runs a failed job again
Installer.cancel(id)          also clears a failure
Installer.refresh()
```
