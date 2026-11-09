# Finding games

## Search

Start typing anywhere in the main window, or press Ctrl+F or /, to search. The search matches titles:

- ignoring case, accents and punctuation: `pokemon` finds "Pokémon Ultra Sun", `baldurs` finds "Baldur's Gate 3";
- with every word in any order: `zelda tears` finds "The Legend of Zelda: Tears of the Kingdom";
- while you're still typing a word: `zel` already finds it.

Press Enter to launch the first match. Escape leaves the search box and keeps the results; Escape again clears the search, as does the × at the end of the box while it has text.

## The rail

The rail picks the platform: **All games**, or one platform. With more platforms than fit, the platform buttons scroll with the mouse wheel while the logo, All games and the buttons at the bottom stay in place; choosing a platform with the keys scrolls it into view. Right-click a platform button for its menu: **Edit platform** opens it in Settings → Platforms, and **Hide from the rail** takes it out of the rail (the toast that follows has an Undo, and Ctrl+Z works while it shows). Its games stay in All games, and Settings → Platforms → Show in the rail brings it back. Right-click an empty part of the rail for a menu of every platform and of the buttons at the bottom (Add game, Play statistics, Settings): click one to show or hide it in the rail. The menu stays open, so you can change several at once. Their shortcuts work either way. Ctrl+1 is All games, Ctrl+2 the first platform, and so on; Ctrl+Tab moves to the next. To reorder the rail, press a platform button, hold still for a moment (400 ms; `drag_hold` in the theme's `[rail]` changes it, 0 turns the wait off) until it fades and grows, then drag it up or down; the other buttons move aside to show where it will land. Esc, or letting go outside the rail, cancels. A click or a quick swipe never moves one. Ctrl+2, Ctrl+3… follow the new order. The buttons at the bottom (Add game, Play statistics, Settings) reorder the same way, among themselves, and the rail's menu and the logo's menu list them in that order; the order is kept as `rail_button_order` in `[general]` of `settings.toml`.

To get the room back, hide the rail: **Hide rail** in the logo's menu, under Fullscreen. The rail fades out, only the logo stays, and the game list grows left into the space. The rail's shortcuts (Ctrl+1, Ctrl+2…, Ctrl+Tab, Ctrl+N, F9, Ctrl+,) keep working, and so does the platform you had picked. The same entry, now **Show rail**, brings it back, and so does **Show rail** in Settings → General. It stays hidden, also after a restart, until you show it (`rail_hidden` in `[general]` of `settings.toml`). To bind a key to it, look for "Show or hide the rail" in Settings → Keyboard. With the rail hidden, the filter panel lists the platforms (see Filters below).

## Filters

The filter button beside the search box (Ctrl+Shift+F) opens the filter panel:

- **Installed only** (on at first): leaves out games that aren't installed, like a ROM whose file is missing or a Steam game that isn't installed. A Steam game that Steam is installing, or has queued to install, stays in the list. They're counted at the end of the list ("2 not installed · Show"), so they're never lost.
- **Favorites only** (Ctrl+Shift+D).
- **Hidden games**: shows hidden games too, dimmed.
- **Platforms**, only while the rail is hidden: **All**, then every platform that has games, each with its count. Pick one; picking the selected one does nothing. The panel scrolls when the window is too short.
- **Status**: Playing, Backlog, Finished, Completed, Dropped or No status; a game matching any of the ones you pick is shown.
- **Tags**: a game with any of the picked tags, or with all of them when "Match all selected tags" is on. The section starts folded to leave room for Collections above it; its heading shows how many tags are picked, so a tag filter stays visible while folded. Click the heading to open or fold it; Lattis remembers which.

Filters add up: a game is listed only when it passes every one.

Each filter you've turned on shows as a chip under the search box. Click × on a chip to turn it off, or **Clear filters** (Ctrl+Shift+Backspace) to turn them all off. The search and the sort stay as they are, and so does the platform, unless the rail is hidden: then the platform shows as the first chip, × and Clear filters return to All, and the dot lights. A dot on the filter button means a filter is on, so a short list always tells you why.

## Collections

A collection is a named group of games: ones you pick by hand, ones that match saved filters, or both. The **Collections** section at the top of the filter panel lists them A to Z, each with its game count; click one to open it. The list then shows only its games, uninstalled ones too, with a bar above it giving the name and count and an × to leave. Search, sort, grouping and the other filters work inside it, and **Clear filters** goes back to the collection's full list. The × puts back the filters and platform from before. One collection is open at a time, and Lattis reopens it after a restart.

- **New collection** (the last row of the section) asks for a name and makes an empty collection.
- Right-click the selected game or games, then **Collections**: each collection has a check, on when every selected game is in it, a dash when some are. Click to add them all, or to take them all out. **New collection** there makes one holding the selection. This works on one game too.
- Inside a collection, **Remove from collection** in the game's menu takes the game out of it. The game stays in your library.
- **Save as collection** (above Clear filters, shown while filters are on) asks for a name and makes a collection from the filters you have now: the platform, Installed, Favorites, status, tags (and match all) and Steam ownership. It then keeps itself up to date: a game you tag or favorite later joins it. Opening it shows its games as a fixed list, and its rules never appear in the filter panel, so using the filters inside it only narrows what you see. A rule that names a tag you delete ignores that tag (undoing the delete brings it back); renaming a tag updates the rules.
- Right-click a collection (or press the Menu key on it) for **Rename**, **Edit** and **Delete**. **Edit** shows the rules (platform, Installed, Favorites, status, tags, Steam ownership) behind "Keep up to date with these rules", and the games you added by hand and the ones you left out of it. Uncheck a game to drop that exception. Turning the rules off leaves the games you added by hand. Delete removes only the collection, never its games.
- **Import from Steam** (the last row of the section, shown when Lattis finds Steam) brings in the collections you made in Steam. Pick the account if there are several, then tick the collections to import; all are ticked to start with. Collections Steam fills by its own rules ("dynamic" ones) can't be copied, since Lattis can't run Steam's rules and Steam doesn't say which games they match; they are left out and counted. Steam's own Favorites and Hidden aren't offered either (Lattis has its own), and the collections Steam made from its old categories are offered like any other, under Steam's name. When it's done, a summary lists what was added, what was skipped, and the games that aren't in your library; they join their collection if you add them later. Importing again updates the collections from before to Steam's lists and adds new ones, and keeps any name you changed. Games you added to an imported collection in Lattis are removed by that, which the dialog says before it runs. If you already have a collection of the same name, Steam's arrives as "Name (Steam)". Lattis only reads Steam's file; nothing is changed in Steam, and Steam's favorites aren't copied into Lattis's Favorites.

Each change is one step with **Undo** in the message that follows. Collections are kept in `library/collections.toml`, so a backup of the library has them.

## Sorting and grouping

Also in the filter panel:

- **Sort** by title, last played or play time, either way. Ctrl+Shift+S goes to the next order. Sorted by last played or play time, each row shows it ("3 days ago", "12 h"). Turn off **Last played on rows** (or **Play time on rows**) under Show in the filter panel to hide it. Games never played go last.
- **Group by** platform (the default), by status (Playing first), or None, which gives one list across every platform. With None and "last played", your most recently played game is at the top whatever its platform.

Click a group's header to collapse it (or press Left; Right expands). While you search or filter, every group opens, so nothing that matches is out of sight.

Lattis remembers the sort, the grouping and the filters, the platform you were on and which groups you had collapsed, and opens the way you left it.

## Selecting several games

- **Ctrl+click** adds a game to the selection or takes it out. **Shift+click** selects every game from the last one you clicked to this one (headers and collapsed groups are skipped); Ctrl+Shift+click adds that range. **Shift+Up** and **Shift+Down** grow or shrink the range from the keyboard, and **Ctrl+A** selects every listed game.
- A bar under the list says "4 selected", with an × that clears it. Esc clears the selection too (a second Esc clears the search). A plain click, or any change to the search, the filters or the rail's platform, also clears it, so you never act on games you can't see.
- The details show the game you clicked last. Right-click one of the selected games for a menu that acts on all of them (right-clicking another game acts on that game alone). The Menu key and Shift+F10 open it under the current row.

The selection's menu has:

- **Add to favorites** or **Remove from favorites**: Remove when every one is a favorite.
- **Tags**: each tag has a check that is on (all have it), off (none do) or half (some do). Click an off or half tag to add it to all of them, an on tag to remove it from all. The menu stays open, so you can set several tags in a row. **New tag** adds a tag and puts it on all of them.
- **Emulator**: when they're all ROM games on one platform, pins an emulator for all of them, or **Platform default** to clear the pin. With mixed platforms the entry is greyed out and says why.
- **Collections**: the same checks as in a single game's menu (see above).
- **Hide** or **Unhide**: Hide when any is visible, Unhide when all are hidden.

Each change is one step with **Undo** in the message that follows ("4 games hidden"). The menu leaves out what is about one game: Launch, Open in Steam, Open game folder, Open save folder, Play history, Properties and Uninstall.
