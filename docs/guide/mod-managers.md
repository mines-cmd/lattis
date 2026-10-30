# Mod managers

A mod manager (Mod Organizer 2, Vortex, Fluffy Mod Manager, a native tool) can be added to Lattis and linked to the games it mods. You open it from the game's page, and it never counts as a game: no play time, no history, no place in the game list.

Lattis only points at a manager that is already on your disk. It doesn't download or install one.

## Adding a manager

Settings → **Mod managers**, then **+**. The dialog asks for:

- **Name** and **Program.** A `.exe` makes a Windows manager, run through Proton; anything else (a program, a script, an AppImage, or a Flatpak app id such as `org.example.App`) is a native one that Lattis runs itself.
- **Games.** The Steam, Windows and Linux games it serves (searchable). A manager can serve several games, and a game can have several managers. Emulator games can't be linked.
- **Prefix** (a Windows manager linked to exactly one Steam or Windows game): **The game's prefix** or **Its own prefix**. Mod Organizer 2 and Vortex need the game's registry, `Documents/My Games` and `AppData`, so the game's prefix is the default. A manager linked to several games or none has its own.
- **Proton version** (own prefix only). With the game's prefix, the manager uses the game's version and the field is off: another version would upgrade or downgrade the game's prefix.
- **Launch options** and **Start in** (the program's folder unless you change it).

A Windows manager is added to Steam as a non-Steam game, kept in step like a [Windows game](windows-games.md)'s, so it waits for Steam to close (see [Changes wait for Steam to close](steam.md#changes-wait-for-steam-to-close)). It stays visible in Steam's library: Steam no longer honors the hidden flag for shortcuts.

If you already added the manager to Steam yourself, use **Add existing shortcuts** on Settings → Steam and choose **Add as mod manager** on its row. It keeps its Steam entry and its own prefix.

## Opening a manager

- The game's page can show a **mod manager button** (icon only; add it under Settings → Game page). It shows only when the game has a linked manager. With one, a click opens it; with several, a menu of them. A dot shows while one runs.
- The game's right-click and ⋯ menus have **Open <name>** per manager and **Add a mod manager**.
- The **Open mod manager** shortcut has no key at first (Settings → Keyboard).
- On Settings → Mod managers, each row has **Open**.

If the manager uses a game's prefix and the game has never been run, Lattis says "Run <game> once first, so Proton creates its prefix" and offers **Launch**. It never lets the manager create a fresh prefix in the game's place.

Nothing is blocked while a manager runs: the game's Launch still works, since a manager often starts the game itself. A game can't be uninstalled while one of its managers is running.

## Links and missing games

Link games from the manager's dialog, or from the game's **Properties → Mod managers** (**Link** picks an existing manager, **Add** opens the dialog with the game already ticked, × unlinks; Save writes it).

If you remove a game from the library, it drops out of every manager. A game that is only hidden, uninstalled or not installed stays linked and the manager's row marks it missing. A manager with no games stays listed.

If you delete a Windows manager's shortcut in Steam, its row says **Not in Steam**; **Locate** adds it back.

## Removing a manager

**Remove** asks what to delete, showing each option only while the thing exists:

- **The Steam shortcut** (on by default).
- **Its own prefix** (off; never offered for a game's prefix).
- **Its folder** (off; to the trash, and refused for folders like your home).

A game's prefix is never touched.

## Playing and backups

Steam still counts time on a Windows manager's shortcut, but Lattis records nothing for any manager.

The library [backup](backups.md) includes your managers. Programs and prefixes aren't in it, and a Windows manager's shortcut is added back to Steam after an Import.

## Editing by hand

Each manager is a file in `library/mod-managers/` in your [data folder](data-folder.md), with the keys `name`, `exe`, `start_dir`, `launch_options`, `games`, `prefix`, `proton` and `shortcut`.
