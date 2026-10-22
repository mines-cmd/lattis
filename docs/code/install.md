# Installing emulators (`src/install/`)

The install code downloads an emulator's AppImage from its project's releases, checks it, puts it where the library already looks for AppImages, and remembers that Lattis put it there. It never touches detection: an install ends by asking the library to look for emulators again, and the library finds the new file like any other AppImage in `~/Applications`.

## Contents

- [Where a download comes from](#where-a-download-comes-from)
- [Releases](#releases)
- [The download](#the-download)
- [The install record](#the-install-record)
- [The Installer](#the-installer)
- [Testing hooks](#testing-hooks)
- [Files](#files)
- [QML reference](#qml-reference)

## Where a download comes from

Three emulator keys (see [library](library.md#emulators)) name it:

- `release`: `github:<owner>/<repo>` or `gitlab:<host>/<group>/<project>`.
- `release_asset`: the file's name pattern in the latest release (case-insensitive, `;` separates patterns in order of preference).
- `release_inside`: for a zip or tarball, the pattern of the AppImage inside it.

An emulator with `release` and `release_asset` can be installed as an AppImage. Every built-in emulator has them except Dolphin, which publishes no AppImage. A value that doesn't parse is a warning when the file is read and the key is ignored.

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

`AppImageDownload` (`appimageinstall.*`) takes the network manager, one asset, the `release_inside` pattern and the target folder, and ends with a `Result` (`ok`, `cancelled`, `path`, `error`, `sha256`). Its stages are Downloading, Extracting (zips and tarballs only) and Installing.

1. Only an `https` URL is requested; redirects may not go to a less safe scheme (`NoLessSafeRedirectPolicy`). `setRelaxed(true)` also allows `http` and `file` URLs, for the tests and the fixtures.
2. The bytes go to `.<name>.partial` in the target folder while a SHA-256 is computed. At the end the size must match the asset's, when the release lists one, and the hash must match its `sha256`, when it has one. A mismatch deletes the file and fails.
3. A zip or tarball is unpacked on a worker thread by `AppImageInstall::extractOne()`: one entry, the first regular file whose name matches `release_inside`, written to `.<name>.partial`. An entry whose path is absolute or holds `..` makes the whole archive refused; a matching entry that is a link is refused; nothing larger than 2 GiB is written. The archive's partial file is then deleted.
4. The AppImage is made executable (0755) and renamed to its upstream name. A file with that name already in the folder fails the install; nothing is overwritten.
5. A failure or `cancel()` deletes every partial file. Destroying the object does too.

`setPace(bytes, tickMs)` reads that many bytes per tick instead of everything that has arrived, so a screenshot can catch the progress.

`AppImageInstall::targetFolder(applicationsDirs, home)` is the first of `~/Applications` and `~/AppImages` that exists, else `~/Applications`. Both are already searched by detection, so nothing is written to `emulators.toml`.

## The install record

`~/.local/state/lattis/installs.toml` (`installrecord.*`) says what Lattis installed on this computer, one table per emulator id: `route` (`appimage`; `flatpak` later), `file` (the AppImage, or the Flatpak id), `tag`, `sha256`, `asset_id`, `asset_date` and `installed` (the date). It is in the state dir, not the data dir, because it describes this machine: a backup doesn't carry it.

`InstallRecords::stillThere(record, userFlatpaks)` is what "installed by Lattis" means: the record exists and its AppImage is still a file (or its Flatpak is still in the user installation). A file Gear Lever moved stops counting, and the emulator shows only as found. An unreadable file is a warning and counts as empty; entries without a `route` and a `file` are skipped.

## The Installer

`Installer` (`installer.*`) is the QML singleton, created on first use like `Backups`; `Installer::setup()` at startup gives it the state and cache dirs and three callbacks (the emulator definitions, the install locations, and "look for emulators again", which is `Library::scanNow()`). Nothing is made, and nothing reaches the network, until QML uses it.

`install(id, route)` runs one job per emulator: Checking (the release is requested), then Downloading, Extracting and Installing from `AppImageDownload`. Several emulators can install at once. When a job ends, the record is written and the library is asked to detect again. A failure keeps its message until the next `install()` or `cancel()`; `cancel(id)` stops a job at any step and deletes what it wrote, and quitting cancels them all. Only the `appimage` route exists so far.

`emulators` (a `QVariantMap`, id to a map) has a row for each emulator that can install or has a record:

```
state           "idle", "checking", "downloading", "extracting", "installing" or "failed"
progress        0 to 1 while downloading, -1 when unknown
error           the message of a failure
routes          ["appimage"]
source          where the AppImage comes from: "GitHub" or a server name
installedByApp  the record exists and its file is still there
version         the recorded release tag, when installedByApp
updateVersion   "" for now
```

State changes update it at once; progress is coalesced to about 12 updates a second. `refresh()` re-reads the record and checks the files again; the Emulators page calls it when shown.

## Testing hooks

`LATTIS_INSTALL_FIXTURES=<dir>` makes the Installer read the release from `<dir>/<release value with ":" and "/" as "-">.json` (`github-mgba-emu-mgba.json`) after a short delay, and download the asset from `<dir>/<asset name>` as a `file:` URL. With no such file it makes a script that sleeps (2 MB, no checksum) under the cache dir. The download is paced (40 steps of 60 ms), so the progress shows. Tests and screenshots set `HOME` to a scratch folder, so nothing is installed for real.

## Files

| File | What it holds |
|---|---|
| `releasesource.*` | `ReleaseSource`, `Release`, `ReleaseAsset`; parsing a `release` value and the two APIs' JSON; asset patterns |
| `appimageinstall.*` | `AppImageDownload`; `targetFolder()`, `isArchive()`, `extractOne()` |
| `installrecord.*` | `InstallRecord`; reading, writing and judging `installs.toml` |
| `installer.*` | The `Installer` singleton |

## QML reference

```
Installer.emulators           the map above
Installer.install(id, route)  route defaults to "appimage"
Installer.cancel(id)
Installer.refresh()
```
