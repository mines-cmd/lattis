# Linux games

Native Linux games (a shell script, a program or an AppImage) run straight from Lattis, without Steam, and Lattis records their play time. They show on the **Linux** platform.

A Steam game that runs natively on Linux stays a Steam game; this page is for games you got elsewhere.

## Adding a game

Click **+** and pick the game's program. The picker's "Linux programs" shows the usual kinds (`.sh`, `.x86_64`, `.AppImage`…); many programs have no extension, so use "All files" to find those. Lattis tells a program by what's inside it, not by its name.

The dialog fills in:

- **The install folder:** the program's folder, skipping generic ones like `bin/x86_64`, so `SuperTuxKart/bin/x86_64/supertuxkart` gets `SuperTuxKart`. An AppImage is the whole game, so it gets no install folder.
- **The title**, from that folder's name (or the AppImage's).
- **The presets** you've set as defaults (see [Presets](steam.md#presets)), and the launch options.

A program that isn't marked as executable gets **Make it executable**, checked; without it, the game can't start.

New Linux games aren't looked up by themselves. Use the dialog's **Look up in IGDB**, which searches IGDB's Linux and PC games.

## Launch options

Linux games take launch options the way Steam does: `gamemoderun %command% --fullscreen`. Variables come first, a wrapper (like `mangohud` or `gamemoderun`) goes before `%command%`, and the game's own arguments after it. Without `%command%`, it's all arguments. The presets work the same as for Steam games.

## Playing

**Launch** runs the program in its own folder (or the one set in **Start in another folder**). Its output goes to `~/.cache/lattis/launch/<game id>.log`, which is the place to look when a game won't start.

The game counts as running while anything it started is still running, so a script that starts the real game and exits still records the whole session.

Play time and last played are recorded as for an emulated game (see [Playing](playing.md)), and can be edited in Properties.

If the program is moved or deleted, the game shows "Game not found" with **Locate** in place of Launch.

## Saves

Each Linux game keeps its saves in its own place (`~/.local/share`, `~/.config`, or its own folder), so Lattis's backups don't include them yet. Back them up yourself, or with a tool like Ludusavi.

## Uninstalling

**Uninstall**, in the game's Properties or its right-click menu, can delete the game's files: its install folder, or only the AppImage (never the folder it's in, which often holds your other AppImages). Then **Keep it**, **Keep it, hidden** or **Remove it from the library**. Deleted files go to the trash.
