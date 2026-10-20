# Getting started

## Installing

Lattis comes as an AppImage: one file with everything it needs, which runs on x86_64 Ubuntu 22.04, Debian 12, Fedora 36, SteamOS 3 or newer. It uses your system's graphics libraries (OpenGL through libglvnd), which any desktop install has.

1. Download `Lattis-<version>-x86_64.AppImage`.
2. Make it executable: right-click it → Properties → "Allow executing as a program", or run `chmod +x Lattis-*.AppImage`.
3. Double-click it to start.

Keeping it in `~/Applications` or `~/AppImages` is a good habit; tools like Gear Lever can add it to your app menu. To open Lattis when you log in, turn on **Start at login** in Settings → General.

To build it yourself instead, see the project's README.

## The setup guide

The first time Lattis starts, a setup guide walks you through six short steps. Every one can be skipped, and everything it sets up can be changed later in Settings.

1. **Welcome.** Start fresh, or restore a library from a backup (see [Backups](backups.md)).
2. **Steam.** If Steam is installed, its installed games are imported; their covers show at the bottom of the step. If Lattis can't find Steam, **Choose folder** points it at the folder with `steamapps` in it. You can pick presets new Steam games start with, like MangoHud or Wayland, and bring in the non-Steam games you already have in Steam (see [Steam games](steam.md) and [Windows games](windows-games.md)).
3. **Emulators and ROM folders.** The emulators Lattis found, and the folders your ROMs are in. If you keep one folder per platform (`roms/switch`, `roms/3ds`…), **Find from one folder** matches them to platforms by name in one go. See [Emulators and ROM folders](emulators.md).
4. **Game info.** Free API keys for IGDB and SteamGridDB, so your games come in with a description, year, rating and art. Without them, games come in with their names only; you can add the keys later. See [Game info and art](game-info.md).
5. **Look.** Pick colors and a style. They apply right away. With many colors, the list scrolls and the styles stay in view below it.
6. **Done.** Lattis brings your games in and looks them up. You can start using it while that runs.

Nothing is imported or scanned until you finish or skip, so your games come in with their info already filled in. To run the guide again, use **Setup guide** in the app menu or **Run setup guide** in Settings → General.

## The window

- **The rail**, on the left: the logo, **All games**, one button per platform that has games, then **+** (add a game) and the settings button at the bottom.
- **The game list**: the search box and the filter button above your games, grouped by platform.
- **The game's page**: its art, the **Launch** button, play time and last played, buttons to favorite it, open its emulator or Steam page, open its folder, and open its **Properties**; then its description, tags and an info panel (click its **Platform** to show only that platform's games).

Two menus hold most of what isn't on screen:

- **The app menu**: click or right-click the logo, or press F10. Add game, play statistics and Settings, in the rail's order (so they stay reachable when you hide their rail buttons), theme, colors and style, fullscreen, hide the rail, backups, scanning ROM folders, looking up missing info, the setup guide, keyboard shortcuts, About and Quit.
- **A game's menu**: right-click a game in the list, or press the Menu key. Launch, favorite, open its folder, Properties, Hide, and Uninstall where it applies.

Press F1 at any time for the list of keyboard shortcuts.

## Settings

Open Settings with the button at the bottom of the rail, or Ctrl+,. Its pages:

| Page | What's on it |
|---|---|
| General | Start in fullscreen, use the system locale (or English, with dates as DD/MM/YY), start at login, Reduce motion, what the launcher does while a game runs, the data folder, the setup guide, About |
| Appearance | The theme in use: pick one, or mix colors and a style, and save the pair as a theme |
| Customize | Every color and style setting, by part of the window, with the colors and style in use |
| Library | Backups, hidden games, tags and their colors, and problems found in your library |
| Platforms | Your platforms, their emulators and their ROM folders |
| Emulators | The emulators Lattis knows, where each is installed, and their arguments; add your own |
| Steam | Steam's folder and account, presets, and changes waiting for Steam to close |
| Metadata | Your IGDB and SteamGridDB keys, and **Look up missing info** |
| Keyboard | Every shortcut, and changing them |

Changes in Settings are saved as you make them; there's no Save button.
