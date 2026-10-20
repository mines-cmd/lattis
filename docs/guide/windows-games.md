# Windows games

Windows games run through Proton, the way Steam runs them: Lattis adds each one to Steam as a non-Steam game (a shortcut), keeps that shortcut up to date, and launches the game through Steam. You need Steam installed for this. These games show on the **Proton** platform.

## Adding a game

Click **+** and pick the game's `.exe`. The dialog fills in:

- **The install folder:** the program's folder, skipping generic ones like `Binaries/Win64`, so `Hollow Knight Silksong/Binaries/Win64/Silksong.exe` gets `Hollow Knight Silksong`.
- **The title**, from that folder's name.
- **The Proton version:** the one set as **Proton version for new games** on Settings → Steam, else the one Steam uses for other games.
- **The presets** you've set as defaults, and the launch options.

Change any of them, then Add. The shortcut goes into Steam right away if Steam is closed, or as soon as it closes (see [Changes wait for Steam to close](steam.md#changes-wait-for-steam-to-close)).

### Shortcuts you already have in Steam

**Add existing shortcuts**, on Settings → Steam (or the setup guide's Steam step), lists the non-Steam games in Steam that aren't in Lattis yet. Those Steam runs with Proton are checked; uncheck anything that isn't a game. They come in as they are: their saves, art and play time stay with them, and nothing changes in Steam.

Picking an `.exe` with + that's already a shortcut in Steam is refused, so Steam doesn't get it twice; use Add existing shortcuts instead.

## Playing

**Launch** starts the game through Steam. Play time and last played are Steam's, as for [Steam games](steam.md#launching-and-play-time).

If the `.exe` is moved or deleted, the game shows "Game not found" with **Locate** in place of Launch: point it at the program's new place. Its saves, art and play time stay.

## Properties

A Windows game's Properties have its install location, the program (the pencil changes it), **Start in another folder** (Steam's "Start in"), and the Launch card with the Proton version, presets and launch options. "None (runs natively)" runs it without Proton.

The title, program, start folder, launch options and Proton version are kept in step with the shortcut both ways: a change in Lattis waits for Steam to close, and a change in Steam's Properties comes back into Lattis.

If you delete the shortcut in Steam, the game stays in Lattis as not installed; **Locate** puts it back.

## Saves

Each game's saves live in its Wine prefix, in Steam's `steamapps/compatdata/` folder. **Back up saves** includes them (see [Backups](backups.md)).

## Uninstalling

**Uninstall**, in the game's Properties or its right-click menu, asks what to delete. Each is off at first, and offered only while it's there:

- **Delete the game's files** (its install folder, with its size).
- **Delete its Wine prefix**, which holds its saves and settings. Deleting only the prefix resets the game: Steam makes a fresh one the next time it starts.
- **Remove its shortcut from Steam.** Deleting the files removes it too.

Then choose what happens in Lattis: **Keep it** (as not installed), **Keep it, hidden**, or **Remove it from the library**. The button says what will happen.

Deleted files go to the trash. Lattis refuses to delete folders that hold anything else: your home folder, a Steam library, another game's files, an emulator.

Hiding a Windows game only hides it in Lattis; its shortcut stays in Steam.
