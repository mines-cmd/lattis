# Getting started

## Installing

Lattis comes as an AppImage: one file with everything it needs, which runs on x86_64 Ubuntu 22.04, Debian 12, Fedora 36, SteamOS 3 or newer. It uses your system's graphics libraries (OpenGL through libglvnd), which any desktop install has.

1. Download `Lattis-<version>-x86_64.AppImage`.
2. Make it executable: right-click it → Properties → "Allow executing as a program", or run `chmod +x Lattis-*.AppImage`.
3. Double-click it to start.

Keeping it in `~/Applications` or `~/AppImages` is a good habit. The first time Lattis runs from an AppImage it adds itself to your applications menu, with its icon, so you don't need Gear Lever or AppImageLauncher for that; if you move the file, the entry follows it on the next start. If one of those tools already keeps an entry for the file, Lattis leaves it alone. Turn the entry off, and delete it, with **Applications menu entry** in Settings → General.

To keep the file where Lattis puts emulator AppImages, use **Move** in Settings → General (or on the setup guide's last step): it moves the AppImage to `~/Applications` (or `~/AppImages` if that's the one you have), and the menu entry and Start at login follow it. It isn't offered when the file is already there, when Gear Lever or AppImageLauncher manages it, or when a file with that name is already in the folder. A Steam shortcut to Lattis would need its path changed by hand.

To open Lattis when you log in, turn on **Start at login** in Settings → General.

To build it yourself instead, see the project's README.

## Updating Lattis

The released AppImage looks for a newer version by itself, once a day. When there is one, the app menu gets an **Update to <version>** entry at the top, the logo gets a small dot, and Settings → General and About show "Update available". Nothing pops up on its own.

Open the entry to see the release notes, then click **Update**. Lattis downloads the new AppImage, checks it, and replaces the file you started it from, keeping its name and place, so your menu entries, autostart and Steam shortcut keep working. The old file goes to the trash. Then **Restart now** starts the new version; **Later** leaves it for the next start. Restart is off while a game runs. The first start afterwards shows "Updated to <version>" with a **What's new** link to the notes.

- **Skip this version** hides the entry and the dot until a newer one comes out; Settings → General still lists it.
- **Check now** in Settings → General looks right away.
- The check is one request to `api.github.com` for the project's latest release, and nothing else is sent. Turn it off with **Check for updates** in Settings → General.
- If the AppImage's folder can't be written to, or Gear Lever or AppImageLauncher manages it, the button is **Open release page** instead; update it in that tool.
- A build from source, a distro package and a local test AppImage never check.

## The setup guide

The first time Lattis starts, a setup guide walks you through six short steps. Every one can be skipped, and everything it sets up can be changed later in Settings.

1. **Welcome.** Start fresh, or restore a library from a backup (see [Backups](backups.md)).
2. **Steam.** If Steam is installed, its installed games are imported; their covers show at the bottom of the step. If Lattis can't find Steam, **Choose folder** points it at the folder with `steamapps` in it. You can pick presets new Steam games start with, like MangoHud or Wayland, and bring in the non-Steam games you already have in Steam (see [Steam games](steam.md) and [Windows games](windows-games.md)).
3. **Emulators and ROM folders.** Two lists, laid out like Settings → Emulators. **Ready** holds the platforms whose emulator Lattis found; open one to add the folders its ROMs are in. **Install an emulator** holds the platforms whose emulator is missing, each tagged **Install**: open it to install the emulator as an AppImage or from Flathub, or, under **Already have it somewhere else?**, choose its program if it's installed somewhere Lattis doesn't look. A platform moves up to Ready as soon as its emulator is found, and its ROM folders show there. Installs carry on while you move to the next step. If you keep one folder per platform (`roms/switch`, `roms/3ds`…), **Find from one folder**, beside the Ready heading, matches them to platforms by name in one go. See [Emulators and ROM folders](emulators.md).
4. **Game info.** Free API keys for IGDB and SteamGridDB, so your games come in with a description, year, rating and art. Without them, games come in with their names only; you can add the keys later. See [Game info and art](game-info.md).
5. **Look.** Pick colors and a style. They apply right away. With many colors, the list scrolls and the styles stay in view below it.
6. **Done.** Lattis brings your games in and looks them up. You can start using it while that runs.

Nothing is imported or scanned until you finish or skip, so your games come in with their info already filled in. To run the guide again, use **Run setup guide** in Settings → General.

## The window

- **The rail**, on the left: the logo, **All games**, one button per platform that has games, then **+** (add a game) and the settings button at the bottom.
- **The toolbar** (off by default): a pill of icon buttons above the game's page with Add game, Play statistics, Gallery and Settings. Turn it on with **Show toolbar** in Settings → General, in the setup guide's Look step, or in the rail's right-click menu. Right-click it to tick the buttons you want, or to hide it; drag a button sideways to reorder it. The rail keeps its own list, so a button can be in both, one or neither.
- **The game list**: the search box and the filter button above your games, grouped by platform.
- **The game's page**: its art, the **Launch** button, play time and last played ("None" and "Never" until you've played it), buttons to favorite it, open its emulator or Steam page, open its folder, and open its **Properties**; then its description, tags and an info panel (click its **Platform** to show only that platform's games). Click the **Description**, **Tags** or **Media** heading to fold that section away on every game's page; click it again to unfold it. What it shows is yours to change: right-click the chips beside Launch, the buttons or the info panel to tick what shows, or choose **Customize game page** for the full list with the order (see Game page under Settings below). Add a save folder button or a ⋯ menu button there too.

Two menus hold most of what isn't on screen:

- **The app menu**: click or right-click the logo, or press F10. Add game, play statistics and Settings, in the rail's order (so they stay reachable when you hide their rail buttons), looking up missing info, scanning ROM folders, syncing your Steam library (when you're signed in), theme, fullscreen, hide the rail, backups, keyboard shortcuts, About and Quit.
- **A game's menu**: right-click a game in the list, press the Menu key, or click ⋯ on its page (add that button in `[game_page] buttons` as `menu`). Launch, favorite, open its folder or its save folder, Properties, Hide, and Uninstall where it applies.

Press F1 at any time for the list of keyboard shortcuts.

## Settings

Open Settings with the button at the bottom of the rail, or Ctrl+,. Its pages:

| Page | What's on it |
|---|---|
| General | Show toolbar, Show rail, start in fullscreen, use the system locale (or English, with dates as DD/MM/YY), start at login, Reduce motion, what the launcher does while a game runs, the data folder, updates (the daily check, Check now), the setup guide, About |
| Game page | What the game's page shows and in what order: the chips beside Launch (the eye beside a chip hides or shows its icon), the buttons, the info panel rows (drag a row by its dots to move it; holding it near the top or bottom of the page scrolls it), how play time is written ("206,7 hours" or "206h 42m"), the period Recent play time counts, and which of the description, tags and media (Steam clips and screenshots, an emulator's screenshots) show and in what order, and whether the media shows screenshots and video |
| Appearance | The theme in use: pick one, or mix colors and a style, and save the pair as a theme |
| Customize | Every color and style setting, by part of the window, with the colors and style in use |
| Library | Backups, hidden games, tags and their colors, and problems found in your library |
| Platforms | Your platforms, their emulators and their ROM folders |
| Emulators | The emulators Lattis knows, where each is installed, and their arguments; add your own |
| Steam | Steam's folder and account, presets, and changes waiting for Steam to close |
| Metadata | Your IGDB and SteamGridDB keys, and **Look up missing info** |
| Keyboard | Every shortcut, and changing them |

Changes in Settings are saved as you make them; there's no Save button.
