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

## Updates

An AppImage game can be updated from its GitHub or GitLab releases, like the emulators Lattis installs. Open the game's **Properties → Launch**: the **Updates** card (only for AppImage games) holds:

- **Source:** `github:owner/repo` or `gitlab:host/group/project`. Pasting a repository address (`https://github.com/owner/repo`, or its `/releases` page) turns it into the first form.
- **Release file name:** the release file to download, with `*` and `?` as wildcards (several patterns separated by `;`). A pattern ending in `.zsync`, like the ones Gear Lever and the AppImage itself use, works: the `.zsync` is dropped, because Lattis always downloads the whole AppImage. Under the field, Lattis shows the file it matches in the latest release, or says none does.
- **Allow pre-releases:** takes the newest release that has a matching file, pre-releases included (GitHub only).

Many AppImages carry their own source. Lattis reads it without running the program and uses it until you type your own; the card then says **From the AppImage**, and **Use the AppImage's** puts it back. With neither, the card says to add the game's GitHub or GitLab repository.

**Check now** looks at the latest release with what is typed, before you save, and the card says "Up to date", "Update available" with the version and an **Update** button, or why the check failed. An update whose release lists no checksum isn't downloaded until you choose **Install anyway**, because Lattis can't check the file. The first time a game is checked, Lattis compares the file's checksum with the release's; if the release lists none, it offers the update once, as an unknown version.

Lattis also checks once a day with the emulators (when **Check for updates** is on in Settings → General). A game with an update gets an **Update available** chip and an **Update** row in the info panel on its page (both can be turned off in Settings → Game page), an **Update to <version>** entry in its right-click menu, and a place in the app menu's update entry and the logo's dot. They open the **Updates** window, which lists emulators and games together, with release notes and **Update all**. While it downloads, the chip reads "Updating" with the percentage and Launch is off. When it's done, a message says so, with a **What's new** link.

The new file takes the **same path** as the old one, so shortcuts and menu entries keep working, and the old file goes to the trash. Play time, history and art stay as they are. If anything fails, the old file stays as it was. An update can't be undone from Lattis (restore the old file from the trash). It is refused while the game runs ("Close <game> first"), when Gear Lever or AppImageLauncher manages the file (update it there), and when the file's folder can't be written to.

The three settings are saved in the game's file, so they come back with a library backup.

## Saves

Each Linux game keeps its saves in its own place (`~/.local/share`, `~/.config`, or its own folder). Lattis can find them and back them up when the game closes: turn on **Back up saves** for the Linux platform, or **Backup when the game closes** in the game's Properties (Saves tab, **Backup options**). See [Automatic save backups](backups.md#automatic-save-backups). Add the game's save location in Properties (Saves tab) if it isn't in the list. **Back up saves** (Settings → Library, or the app menu) also takes every Linux game's saves that it finds this way, whether or not automatic backup is on for it.

## Uninstalling

**Uninstall**, in the game's Properties or its right-click menu, can delete the game's files: its install folder, or only the AppImage (never the folder it's in, which often holds your other AppImages). Then **Keep it**, **Keep it, hidden** or **Remove it from the library**. Deleted files go to the trash.
