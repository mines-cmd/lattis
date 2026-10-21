# Metadata (`src/metadata/`)

The metadata code looks up game information on IGDB and art on SteamGridDB, and gives QML the tools to show, review and apply what it finds. It owns the network requests, the API keys, the matching logic and the two ways lookups reach the library: the per-game editor and the bulk workflows. Everything about the library's own game files lives in `src/library/`; metadata only writes through the callbacks it's handed.

## The two services

**IGDB** (the Internet Game Database) supplies title, description, year, rating, platforms and genre/theme/mode/keyword facets. Every request posts an [Apicalypse](https://api-docs.igdb.com/#apicalypse) query to its API. IGDB requires an OAuth token from Twitch; the token is fetched before the first IGDB request, cached to `<cache dir>/igdb-token`, and refreshed automatically on a 401.

**SteamGridDB** supplies art in four kinds — cover, hero, logo and icon — from community contributors. It's found by Steam app ID when a game has one, or by title otherwise. When the title search finds nothing, the search is retried with the sequel number the other way (`SteamGridDb::otherNumerals`): "Mortal Shell 2" → "Mortal Shell II" and vice versa, since SteamGridDB often indexes a game by only one spelling.

**The rating** is IGDB's members' rating (`rating`, 0-100) and nothing else: `total_rating` blends in critic scores, which run high and bunch most well-known games into the top step of the scale. `Igdb::stars(rating, ratingCount)` turns it into stars to a tenth (`rating / 20`) and pulls it toward `kTypicalStars` (3.5) the fewer ratings it rests on, as if the game had `kPriorRatings` (30) extra ratings at 3.5, so a game with a few hundred ratings moves a little and one with thousands not at all. A game with fewer than `kMinRatings` (20) ratings has no rating (-1). Ratings you set yourself stay in half stars; only IGDB's come in tenths, and `Stars` fills any fraction. A lookup that writes the rating also writes `rating_source = "igdb"` (BulkLookup applying, or the editor saving a lookup's rating); `LibraryStore::setGameValues` removes it whenever a rating is written without it, so a rating you set yourself is never marked. The game editor shows "from IGDB" beside Rating while the rating is unchanged and its source is `igdb`.

Neither service is touched before a lookup is actually started. `MetadataClient` is created on first use, not at startup.

## The HTTP layer

`MetadataHttp` handles all network I/O:

- **IGDB requests** are queued to stay within IGDB's limit of four requests per second. Each request fetches the token first when there is none, and once more when IGDB answers 401. The path of a logged request shows only the endpoint, not the query, to keep credentials out of logs.
- **SteamGridDB requests** are straightforward GETs to its API v2.
- **Downloads** (art picked in an editor or bulk dialog) are saved under `<cache dir>/downloads/`, under a name that includes the kind (`cover`, `hero`, `logo`, `icon`), so two concurrent editors don't collide. `removeDownloads()` empties that folder when the editor is discarded.
- **Timeouts** apply per chunk of data arriving, not to the total transfer time, so a large image on a slow line still arrives. Tests shorten the timeout.
- **Fixtures** replace the network for screenshots and tests: `setFixturesDir(dir)` points every request at canned JSON files in that directory (named after the request; see `MetadataHttp::setFixturesDir()`). A missing fixture is an empty answer. Art images in fixture mode are generated placeholders — a two-color diagonal gradient shaped like the kind's art — so tests and screenshots run without keys or a network connection.
- **Format conversion**: the library takes PNG, JPEG and WebP; anything else (SteamGridDB's `.ico` icons) is converted to a PNG of its largest image before saving.

## The client

`MetadataClient` is the QML singleton (`Metadata`). It is the only part of the metadata code QML touches directly. Its request model is asynchronous: every call (`lookup`, `findArt`, `artChoices`, `download`, `testKeys`) returns an integer request ID immediately, and `finished(id, result)` signals when the result is ready. `cancel(id)` drops a result that's no longer needed.

`BulkLookup` and `ScanLookup` use the same client, not their own — there is one rate-limited queue for all lookups in the session.

## Searching and matching

The search for a game title sends an Apicalypse query to IGDB's `/games` endpoint. The query filters by platform when the library knows IGDB platform IDs for it (`Library::igdbPlatformIds()`). Editions, DLC, bundles, mods, episodes, seasons, packs and updates are excluded. Quotes and backslashes are stripped from the title before searching, since Apicalypse strings have no reliable escapes.

A **clear match** is an unambiguous result. For IGDB (`Igdb::clearMatch`): the only game in the results whose name or alternative name folds to the same text as the title; or, for a Steam game, the only one that lists the game's Steam app ID. For SteamGridDB (`SteamGridDb::clearMatch`): the only game whose name folds to the same text, with sequel numbers treated interchangeably as digits or Roman numerals; when several fold to the same text, the one released in the right year.

Without a clear match, the results are offered as a **pick list** for the user to choose from.

The year in an IGDB result is the earliest full release on one of the search's platforms, or, when no platform filter was given, the first release anywhere.

## Bulk lookup

`BulkLookup` runs "Look up missing info": a dialog that looks up a filtered set of games, reviews what was found and writes what the user approves. One `BulkLookup` is created per dialog instance.

**Choosing** which games to list respects three filters: which fields are missing (`missing`), whether to offer replacements for games that already have values (`replaceExisting`), and whether to include games that are missing nothing (`includeComplete`). Checked games are the ones to look up. A game is checked when first shown if it's missing something and not already marked as not on IGDB; games revealed later by changing a filter start unchecked.

**What the sidebars show.** The choosing screen's counts say how many games each choice would list with the other sections as they are now. A section's own filter is cleared when its counts are made, and the search is ignored, so picking one platform doesn't zero the others and typing doesn't make the counts flicker. They are all made in one pass in `refilter()`:

- `missingCounts` (category → games, those on IGDB only), `platforms` (`{id, name, count}`), `tagCounts` (tag → games) and `notOnIgdbCount`.
- `gameModel` is the shown games; `checkedCount` counts the checked ones among them.

The review screen reads `stateCounts` (state → results, plus `all`) and filters with `resultFilter` (`""` = all, or one state: `running`, `matched`, `pick`, `notFound`, `error`, `notOnIgdb`). `resultModel` holds only the results in that state and is updated per result; `results`, `applyCount` and the counts see every result, so a filter never changes what Apply writes. `start()` clears the filter. Results keep the order they arrived in; nothing re-sorts while they come in, so a row never moves under the pointer. `markNotOnIgdb(id)` marks a game and keeps the state it had, and `unmarkNotOnIgdb(id)` puts the result back in it (`pick` or `notFound`).

**Looking up** works in three stages:

1. Games whose IGDB id is already known are prefetched ahead in batches of up to `Igdb::kBatchSize` per request (500), so they cost one request per batch rather than one each. A game whose IGDB id is already in a batch for other platforms is fetched on its own, since its year could differ.
2. The remaining games are searched one by one.
3. For each result a match is resolved: missing fields are filled in and checked; with `replaceExisting`, the game's own value is offered as a replacement (unchecked) when the match's differs; SteamGridDB's first image of each missing kind of art is set as the default. With `replaceExisting` and `art` among `missing` (Look for), the kinds the game has are fetched too (`artKinds()`), as replacements (`replaces`, unchecked); only `art` gates them, as each kind costs a SteamGridDB request. A replacement SteamGridDB has no image for is dropped rather than shown as "none found". Apply writes a replacement only if the game's art URL for that kind is the one it had at the lookup, so art set since isn't overwritten.

**Art without IGDB.** A game IGDB has no match for (`notFound`) is searched on SteamGridDB by the title as searched (`findArtByTitle`; only a clear match counts, no candidates are offered), if it lacks art and a SteamGridDB key is set. The art shows in the row as for a match, and Apply saves a `notFound` row's checked art (with `steamgriddb`) without touching `igdb`; a `notOnIgdb` row saves it along with `igdb = 0`. Marking a pick or a match as not on IGDB searches its art the same way; `unmarkNotOnIgdb` drops that art unless the row was `notFound`.

**Checking a whole category.** `setCategoryChecked(category, checked)` checks or unchecks one category across all results: `description`, `year` or `rating` (that field's replacements only), `tags` (every suggested tag), `art` (every missing kind SteamGridDB found; checking one the user cleared brings back its default image) or `replaceArt` (the same for the replacements). `categories` gives each of the six as `{total, on}`: how many of its items the results offer and how many are checked. The review sidebar's rows read it for their count and for whether their box is checked, dashed (some) or empty, and a group row (Replace existing, Add) adds its categories up and sets them together.

`BulkLookup` keeps up to a few lookups in flight at once (`kInFlight`). Results build the `resultModel`, an `IdListModel`, rather than rebuilding the whole list each time — with hundreds of games, one incoming result would otherwise freeze the dialog for a visible moment.

**Applying** writes what the user left checked. It is careful: it re-reads the library's current state first, writes only fields still missing (or replacements whose value hasn't changed since the lookup), adds only tags the game still doesn't have, and skips art for a game whose file failed to write. Art is downloaded and set in batches so the list rebuilds once per batch, not once per image. Games written leave the results; those that fail stay so the user can see why.

## Scan lookup

`ScanLookup` runs a background lookup when new games are discovered. When the library scan reports new games (`LibraryStore::gamesAdded`) — ROM files the scan found, Steam games added, shortcuts brought in as Proton games — `ScanLookup` runs a `BulkLookup` on them automatically. Only clear matches are written; games without one are left for the user to handle in "Look up missing info". Suggested tags are applied without review.

A batch runs without user interaction. `ScanLookup` batches the incoming ids to pace the rate-limited queue. Progress and the result message ("3 games filled in; 2 need review") are exposed to QML so the UI can show a toast and a "Look up missing info" prompt.

`[metadata] look_up_new_games = false` in settings.toml turns the whole thing off. The setting is re-read from the file each time, so a hand edit applies immediately.

## API keys

`Credentials` reads and writes `credentials.toml` in the data dir's root. It's separate from `settings.toml` so the settings file — and themes — can be shared freely without leaking the keys. A backup of the data dir does restore them.

The file is created with a mode of 0600, and an existing one is set to 0600 before every write. The atomic write (`QSaveFile`) gives the new file the permissions of the one it replaces, so setting them afterwards would leave a moment where the keys are readable by others; a new file would get the umask's 0644, which is why the file is created first. The IGDB token in `<cache dir>/igdb-token` is written the same way. A missing file is treated as no keys, without a warning. Unreadable values and unknown keys warn with `file:line` and are ignored.

## `IdListModel`

`IdListModel` is a list model of maps keyed by `id` that signals only the rows that changed. `setItems(ids, maps)` computes the minimal set of row removals, insertions and moves, signals them, and then signals `dataChanged` only for rows whose map actually changed. This keeps a QML delegate for unchanged rows alive and in place, which matters when a hundreds-entry bulk result arrives one lookup at a time.

## Files

| File | Contents |
|---|---|
| `metadataclient.*` | `MetadataClient`: the QML singleton (`Metadata`); async request dispatch |
| `metadatahttp.*` | `MetadataHttp`: the network layer; IGDB token, rate limit, downloads, fixtures |
| `bulklookup.*` | `BulkLookup`: "Look up missing info"; the game list, lookup queue, apply |
| `scanlookup.*` | `ScanLookup`: automatic lookup of newly discovered games |
| `igdb.*` | IGDB search and fetch queries, parsing, platform list, clear-match |
| `steamgriddb.*` | SteamGridDB request paths, parsing, art filtering, clear-match |
| `credentials.*` | `credentials.toml`: parsing and writing API keys |
| `metadatasettings.*` | `[metadata]` in `settings.toml` |
| `idlistmodel.*` | `IdListModel`: a list model that signals only changed rows |
| `thumbnailcache.h` | `ThumbnailCache`: disk cache for QML network images (pick lists, art browser) |

## QML reference

`MetadataClient` is the `Metadata` singleton. Every method returns a request ID; results arrive via `finished(id, result)`.

### Properties

| Property | |
|---|---|
| `hasIgdbKeys` | Both IGDB keys are present |
| `hasSteamGridDbKey` | The SteamGridDB key is present |
| `keys` | `{igdbClientId, igdbClientSecret, steamGridDbKey}`, for the Metadata settings page |

### Methods

| Method | |
|---|---|
| `reloadKeys()` | Reads `credentials.toml` again |
| `setKey(section, key, value)` | Saves a key; `""` removes it. Returns an error, or `""` |
| `lookup(title, igdbPlatforms, igdbId, steamAppId, limit)` | Searches IGDB, or fetches a remembered match when `igdbId > 0` |
| `igdbPlatforms()` | The built-in IGDB platforms as `[{id, name}]`, sorted by name |
| `findArt(name, year, steamAppId)` | Finds the game on SteamGridDB |
| `searchArtGames(text)` | One plain SteamGridDB search: no numeral retry, no clear-match rule |
| `artChoices(steamGridDbId, kind, style, page)` | A page of art images |
| `download(url, kind)` | Downloads a picked image |
| `testKeys(service)` | Checks the keys of `"igdb"` or `"steamgriddb"` |
| `cancel(id)` | Drops a result |
| `discardDownloads()` | Removes images picked but not saved |
| `styles(kind)` | SteamGridDB styles for a kind, as `[{id, name}]` |

### Map shapes

`lookup()` result:

```
candidates   [{igdb, name, year, platforms (names), summary, rating,
               cover (thumbnail URL), coverLarge, steamAppId, facets}]
match        clear match index, or -1
```

`findArt()` result:

```
steamgriddb   its id, or 0
name
candidates    [{steamgriddb, name, year}]
```

`searchArtGames()` result:

```
games   [{steamgriddb, name, year}]
```

`artChoices()` result:

```
images   [{id, url, thumbnail, width, height, style, author}]
end      whether this was the last page
```

`download()` result:

```
path   file URL of the saved image
```

`testKeys()` result: `{}` on success, `{error}` on failure.
