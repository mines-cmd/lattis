# Your data folder

Everything Lattis keeps lives in one folder, `~/.local/share/lattis/`. **Open** beside "Data folder" on Settings → General (or "Open the data folder" in the app menu) opens it. Copy it and you have a complete backup.

| Path | What's in it |
|---|---|
| `settings.toml` | Your settings: the colors and style, the list's sort and filters, Steam, launching, shortcuts |
| `credentials.toml` | Your IGDB and SteamGridDB keys, readable only by you |
| `themes/` | Your themes (a name for colors and a style), in `colors/` your colors and in `styles/` your styles, each with a `reference.toml` explaining every setting |
| `library/platforms.toml` | Your platforms, in rail order, with their emulators and ROM folders |
| `library/emulators.toml` | Changes to the built-in emulators, and emulators of your own |
| `library/tags.toml` | Your tags and their colors |
| `library/games/` | One file per game, holding what you've set on it |
| `library/history/` | Each game's play sessions, one file per game, made with its first session; `steam-seen.toml` holds the play time last seen in Steam, so only new play counts |
| `library/roms/`, `library/steam.toml` | The games found in your ROM folders and in Steam |
| `art/<game>/` | Each game's hero, cover, logo and icon |

You can put art in `art/<game>/` yourself, as `hero`, `cover`, `logo` or `icon` in PNG, JPEG or WebP. Art larger than Lattis ever shows is shrunk in place in the background (heroes to 3840 px, covers to 1800, logos to 1280, icons to 512), however it got there, and the larger file is not kept. If you want the full-size original, keep a copy somewhere else. The game you're looking at is shrunk once you move to another.

Outside it, Lattis keeps only what it can rebuild: a cache in `~/.cache/lattis/` (safe to delete; it holds the library's startup snapshot and display copies of hero art, which decode faster, made again when missing) and window sizes in `~/.local/state/lattis/`.

## Editing by hand

All of these are plain text (TOML), and you can edit them while Lattis runs: it notices a saved change within a moment and updates. When Lattis saves something itself, it changes only that value and keeps the rest of the file, comments included.

Some things are quicker by hand, like fixing many titles at once or sharing a theme. A game's file looks like this:

```toml
title = "Hades"
year = 2020
rating = 4.5
tags = ["Roguelike"]
favorite = true
status = "finished"
```

A game you've never changed has no file of its own: it comes from `library/roms/` or `library/steam.toml`, and a file appears the first time you change something about it. To remove such a game, set `hidden = true` in its file; deleting the file only forgets your changes.

`emulators.reference.toml` and the `reference.toml` in `themes/colors/` and `themes/styles/` are rewritten at each start: read them, but put your changes in `emulators.toml` and your colors or style.

## Problems

When a file has a mistake (a typo in a setting, a value out of range), Lattis uses the default for that value and lists the problem, with the file and line, under **Problems** on Settings → Library. A file with a syntax error keeps its last good version until it's fixed.

## A read-only library

A library saved by a newer version of Lattis (say, a backup restored under an older one) opens read-only: you can browse and launch, but nothing is saved, not even play time. A bar above the game says so. Update Lattis to use it normally.

When Lattis is updated and its library format changes, it upgrades your library at start, after copying it to `library/backup-v<old version>/`.

## Another library

Starting Lattis with `LATTIS_DATA=<folder>` uses another data folder, which is handy to try things out without touching your library. Only one copy of Lattis runs per data folder: starting it again brings up the window already open.
