# Keyboard shortcuts

Everything in Lattis works from the keyboard. Press F1 (or Ctrl+/) for the list of shortcuts, and change any of them on Settings → Keyboard: click a shortcut, press the new keys. Tooltips and menus always show the keys you've set.

Typing in the main window starts a search, so the main window's shortcuts all use a modifier, a function key or a navigation key.

## Game list

| Keys | Action |
|---|---|
| Up / Down | Previous / next game (held down, the game page waits until you let go) |
| Page Up / Page Down | Up / down a page |
| Home / End | First / last game |
| Left | Collapse the selected game's group |
| Right | Expand the next collapsed group |
| Ctrl+Up / Ctrl+Down | Previous / next group |
| Shift+Up / Shift+Down | Select the game above / below too |
| Ctrl+A | Select every listed game |
| Escape | With several games selected, select only the current one |

## Grid view

| Keys | Action |
|---|---|
| Ctrl+G | Switch between List view and Grid view |
| Left / Right | Previous / next cover |
| Up / Down | The cover a row above / below |
| Page Up / Page Down | Up / down a page of rows |
| Home / End | First / last game |
| Shift with any of these | Select the games in between too |
| Enter | Launch |
| Ctrl+Enter | Open the game's page over the grid (Escape returns) |
| Ctrl+B | Show or hide the side panel |
| Ctrl+= / Ctrl+- (or Ctrl+scroll) | Bigger / smaller covers |

## Views

| Keys | Action |
|---|---|
| Ctrl+1 | All games |
| Ctrl+2 … Ctrl+9 | The 1st to 8th platform in the rail |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous entry in the rail |

## Search

| Keys | Action |
|---|---|
| Ctrl+F or / | Focus the search box |
| Ctrl+Shift+F | Filters |
| Ctrl+Shift+C | Collections |
| Ctrl+Shift+D | Show only favorites / all games |
| Ctrl+Shift+S | Next sort order |
| Ctrl+Shift+Backspace | Clear filters |
| Escape | Leave the search box, then clear the search |

## Selected game

| Keys | Action |
|---|---|
| Enter | Launch |
| Ctrl+D | Add to / remove from favorites |
| Ctrl+O | Open in Steam, Heroic or the emulator |
| Ctrl+Shift+O | Open the game folder |
| Alt+Enter | Properties |
| Menu or Shift+F10 | The game's menu (the selection's, with several selected) |

With several games selected, Ctrl+D adds them to favorites, or removes them when every one is a favorite. In the clip and screenshot viewer, Ctrl+D marks the item shown instead.

Open save folder, Open mod manager (with several managers it opens a menu of them), Steam properties, Hide and Uninstall have no keys at first; give them some on Settings → Keyboard if you use them often.

## Main window

| Keys | Action |
|---|---|
| Ctrl+N | Add a game |
| F7 | Library overview (Esc, or F7 again, returns) |
| F9 | Play statistics (Esc, or F9 again, returns; Left / Right change the month) |
| F8 | Gallery (Esc, or F8 again, returns; arrows move between pictures, Enter opens one) |
| F11 | Fullscreen |
| Ctrl+Shift+L | Look up missing info |
| F10 | The app menu |

Scan ROM folders now, Sync Steam library, Close Steam and apply, Close Heroic and apply, Setup guide and About have no keys at first.

## Everywhere

| Keys | Action |
|---|---|
| Ctrl+, | Open settings |
| F1 or Ctrl+/ | Keyboard shortcuts |
| Ctrl+Z | Undo the last change, while its message shows |
| Ctrl+Q | Quit |

Open the data folder, Back up the library, Back up saves and Import a backup have no keys at first.

## Properties

| Keys | Action |
|---|---|
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab |
| Up / Down | Previous / next tab, while the tab list has focus |
| Enter | Save, from a one-line field |
| Escape | Cancel |

The tabs are General, Images, Launch and Saves (Saves & mods for Steam, Windows and Linux games). Adding a game has no tabs.

## Settings window

| Keys | Action |
|---|---|
| Ctrl+F | Search the page |
| Ctrl+W | Close the settings window |
| Ctrl+Page Up / Ctrl+Page Down | Previous / next page |
| Ctrl+Shift+N | New colors |
| Ctrl+Alt+N | New style |
| Ctrl+E | Open the colors file |
| Ctrl+Shift+E | Open the style file |

Shortcuts are saved under `[shortcuts]` in `settings.toml`, so a [backup](backups.md) keeps them. If the file can't be written (the folder is read-only, say), Settings → Keyboard says so in red above the list and keeps the old keys.

While the Gallery, Play statistics or the Library overview covers the library, the keys that act on the selected game (Launch, favorite, Open folder, Properties and the rest of that group) do nothing, and so do the list's navigation keys. The game they would act on is hidden under the page.
