# Adding and editing games

Most games come in by themselves: ROMs from your [ROM folders](emulators.md#rom-folders), and installed [Steam games](steam.md). The **+** button adds anything else.

## Adding a game with +

Click **+** at the bottom of the rail (Ctrl+N), or "Add a game" in an empty library, and pick a file. What it becomes depends on the file:

- **A ROM** becomes a game on the platform whose emulators run it. When several could (an `.iso` can be PS2, GameCube or PSP), the first in rail order is picked, and you can choose another. The file stays where it is; nothing is copied or moved.
- **A Windows program** (`.exe`, offered when Steam is installed) becomes a Proton game. See [Windows games](windows-games.md).
- **A Linux program** (a script, a program or an AppImage) becomes a Linux game. See [Linux games](linux-games.md).

Before adding, a dialog shows what Lattis worked out: the title (cleaned up from the file name), the platform, and room for art, tags and the rest. **Get the title right before adding**: the game's id is made from it and never changes (the title itself can change later, the id stays). "Look up in IGDB" can fill in the rest (see [Game info and art](game-info.md)).

A file that's already in the library isn't added twice: its game is selected instead.

## Properties

The gear button on a game's page (Alt+Return), or **Properties** in its right-click menu, opens its settings:

- **About:** title, description, release year, rating (in half stars; hovering the stars shows faintly the rating a click would set, and double-clicking them clears it, which hovering the Rating name reminds you of) and favorite.
- **Details:** the status (Backlog, Playing, Finished, Completed, Dropped), and play time and last played for games whose play time Lattis records.
- **Tags:** see [Tags](#tags) below.
- **How it runs:** for a ROM game, its ROM, its emulator and its launch arguments; for Steam, Windows and Linux games, their install folder, program and launch options.
- **Images:** the hero (the wide image at the top of the page), the cover, the logo and the icon, each with **Choose** and a remove button. **Browse SteamGridDB** picks them from SteamGridDB.

**Save** (or Enter in a one-line field) writes what you changed. **Cancel** or Escape throws everything away, art included.

**Launch arguments** for a ROM game go after the emulator's own arguments, shown under the field. Quote an argument that has spaces: `--mod "Farewell Map"`.

**Locate** appears when a game's file is missing: point it at the file, wherever it is now. If that file had already shown up as a separate game (a renamed ROM), the two are merged: play time is added together, and favorites, tags and art are kept.

## The logo on the game's page

Logos come in all shapes, so each game's logo can be sized and placed on its own. A logo's empty transparent border is trimmed off, so it sits where you put it: when you set it, when it's copied from Steam, and in the background for logos already in your library or copied into its folder by hand. Right-click the logo (or the title, when there's no logo) on the game's page:

- **Adjust logo** frames the logo. Drag it anywhere on the art: its edges snap to the margins and its middle to the center lines, with a line showing where (hold Shift to place it freely). Drag the corner handle to make it bigger or smaller. The arrow keys nudge it 1 pixel (Shift: 10), Enter or a click outside finishes, and Esc puts everything back as it was when you started. The logo stays inside the art, and keeps its distance from its nearest edges when the window is wider.
- **Behind logo** is what keeps the logo readable on bright art, for this game. **Theme default** follows your theme; **Gradient** is a dark gradient, **Shadow** a soft shadow that follows the logo's outline, and **None** turns it off. Each shows even if the theme has it off. **Side** picks where a gradient goes: Auto (above or below the logo), Top, Bottom, Left, Right, or Full for an even dim over the whole art. **Strength** makes it fainter or stronger as you drag, and the art changes while you do; 0% shows nothing. **Reset to theme** clears all three. The menu stays open while you try them.
- Nine positions (top left, top center, center right, bottom left…) put the logo exactly there; the current one is ticked unless you've moved the logo away from it.
- **Reset size** and **Reset position** go back to the theme's.

## Tags

Tags are your own labels, each with a color, for grouping games any way you like and filtering by them.

- In a game's Properties, its tags are filled chips with ×. Your other tags are outlined chips under them: click one to add it. Type in "Create a new tag" to narrow the list, and press Enter to add what you typed, as a new tag if there's none by that name.
- To tag many games at once, select them in the list and use the Tags submenu of the right-click menu (see [Finding games](finding-games.md#selecting-several-games)).
- Click a tag's color to change it for every game.
- Click a tag under a game's description to filter the list by it, as the filter panel does; click it again to stop.
- **Settings → Library** lists every tag with how many games have it: add, recolor, rename (every game is updated) or delete.
- **A new library starts with 24 tags**, each narrow enough to be worth filtering by: Metroidvania, Roguelike, Soulslike, Deckbuilder, Shoot 'em up, JRPG, Immersive sim, Platformer, Fighting, Racing, Sports, Puzzle, Beat 'em up, RTS, Turn-based strategy, Tactics, Visual novel, Point-and-click, Horror, Stealth, Survival, Open world, Co-op and Split screen. They're ordinary tags: rename or delete any of them. **Add default tags** on Settings → Library brings back the ones you don't have.
- **Suggest tags from IGDB**, on Settings → Library, finds the tags that fit each of your games. You review them before anything is added. See [Game info and art](game-info.md#tags-from-igdb).
- Tag names are case-sensitive: `RPG` and `rpg` are two tags.

Tag colors come from the theme's tag palette, which you can edit on Settings → Library. Each theme keeps its own.

## Hiding and removing games

**Hide** (in a game's right-click menu or its Properties) takes a game out of the list. Its play time, tags and art are kept, and a scan or Steam won't bring it back. Hiding from the right-click menu happens at once, with Undo in the message that follows. The same goes for saving a game's Properties, deleting a game, deleting a tag on Settings → Library and moving a platform: the message offers **Undo** for 15 seconds (or press Ctrl+Z while it shows), and only the last change can be undone. A deleted game's art and play history are kept until the Undo runs out. A Save that picks a new ROM or program can't be undone, and says so.

- Hidden games are listed on Settings → Library, with **Unhide**.
- The filter panel's **Hidden games** button shows them in the list too, dimmed with an eye icon. They launch like any other game.

When hiding from Properties, **Also delete its info and art** goes further: the game's description, tags, art and the rest are deleted too. A game that a scan or Steam would find again stays hidden, so unhiding it later brings it back fresh. A game that only exists because you added it with + is removed for good. The game's own files (the ROM, the Steam install) are never touched.

Steam, Windows and Linux games can also be **uninstalled**; see their pages.
