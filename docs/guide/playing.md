# Playing

## Launching

Select a game and click **Launch**, press Enter, or double-click it in the list. The Launch button's tooltip says what it will use ("Launch with Ryubing (AppImage)").

- **Emulated games** start in the game's emulator: the one pinned in its Properties if it's installed, else the first installed one in its platform's list. If none is installed, a message says which to install, and the game's page shows **Install** and the emulator's name in place of Launch (the first emulator in the platform's list, if it can be installed from the app). The click opens a window, closed with its X, with the same install choices as [Settings → Emulators](emulators.md#installing-an-emulator): a tile for each way to install it, and "Already have it somewhere else?" to point at a program you installed yourself. While it installs the window shows the progress, and the button on the page shows the step ("37 %", "Unpacking"; click it to bring the window back). The window closes by itself once the emulator is found, and the button becomes Launch. This works for any emulated game, with or without a ROM. Pressing Enter or double-clicking in the list never installs anything: it shows the message.
- **Steam and Windows games** start through Steam.
- **Epic games** start through Heroic (see [Epic games through Heroic](heroic.md)).
- **Linux games** start directly.

**Open emulator** starts the emulator without a game, and **Open folder** opens the folder the game is in. **Open save folder**, in the game's menu, opens where its saves are (see [Properties](adding-games.md#properties) for how it's found); it's also a button you can add to the game's page in `[game_page] buttons`, shown only when the game has a save folder.

**Open a mod manager** is in the game's menu (one **Open <name>** per manager linked to the game, with **Add a mod manager** under them) and a button you can add to the game's page in `[game_page] buttons` as `mod_manager`: one icon, shown only when the game has a linked manager, with a dot while one runs. With several managers it opens a menu of them. If the manager shares the game's Proton prefix and the game has never run, Lattis says to run the game once first, with a Launch button. Add and link managers on Settings → Mod managers, or under **Mod managers** in the game's Properties (**Link**, **Add**, and × to unlink; Save writes the links).

When a game won't start, its emulator's or program's output is in `~/.cache/lattis/launch/<game id>.log`.

## While a game runs

The game's row in the list shows a green "Running" label. Its text and color are `running_label` and `running_color` in `[list]`; an empty label hides it. The game's title takes the same color; turn `running_title` off to keep it as it is.

The Launch button says "Close" ("X STOP" in the Steam style), and launching again does nothing. Click it and it asks first, then ends the game at once. Prefer closing the game itself, since stopping an emulator this way loses unsaved progress.

On Settings → General, **While a game runs** chooses whether the launcher stays open or minimizes and comes back when the game ends. Some desktops (Hyprland, sway) can't minimize, so it stays open there.

## Play time

When an emulated or Linux game closes, the session is added to its play time and its last played is set.

- Sessions shorter than the **Minimum session length** on Settings → General (30 seconds at first) don't count, so a game that crashes on start changes nothing.
- A game that exits with an error within a few seconds didn't really start: a message says so.
- The game keeps running if you quit Lattis, but that session isn't recorded. Quitting or closing the window while a game runs asks first.
- Play time and last played can be corrected by hand in the game's Properties.

Steam and Windows games show the play time Steam records instead, including sessions started from Steam. Epic games add what Heroic records to the time Lattis keeps (see [Play time](heroic.md#play-time)).

### The same game from two sources

When you have one game twice, for example bought on Steam and also added as a Windows game, right-click either copy and choose **Same game as**, then the other copy. The menu lists games with the same title, or the same IGDB entry after a lookup. Linked copies count as one game:

- Both show the combined play time and last played, and their play history lists the sessions of both, each marked with the platform it was played on.
- The statistics count the game once in Most played, under the Steam copy (between two copies not from Steam, under the one you picked in the menu); each copy's hours still go to its own platform.
- Their media is shared: each copy's page shows the clips and screenshots of both, and the Gallery lists the game once.
- Nothing is moved between the copies: each keeps its own play time, and Steam's own numbers stay separate. **Unlink** in the right-click menu (Undo is offered) puts each copy back on its own time. Unlinking the copy the others point at unlinks them all.

You can hide one of the copies and keep the other on the list; the hidden copy's sessions still count.

## Play history

Lattis keeps each session, so you can see when you played and for how long. Click the play time on a game's page, or choose **Play history** in its right-click menu, to open the game's history: its total, this week and this month, then the sessions, newest first (the day, the start and end times and the length: "Today, 20:46 – 22:41, 1h 55m").

- **Before history** is play time Lattis has no sessions for: what a game had before Lattis started keeping history, or what you typed into its Properties.
- **Steam and Windows games** get a session each time Steam's play time grows while Lattis is open. Play from Steam while Lattis was closed can't be told apart, so it shows as one entry, "Played from Steam while Lattis was closed". Games Steam already had play time for start with it all under Before history.
- **To add a session by hand**, for a game you played without opening it through Lattis, press **Add session** at the bottom, next to Close. Set the start (a date and a time, typed in your system's format, or picked with the calendar button at the end of the date field; **Today** and **Yesterday** fill the date), then the length or the end: change one and the other is worked out. Changing the start keeps the length. A line under the fields shows the result ("Yesterday, 21:40, 1h 25m") or why it can't be added: the end must be after the start, nothing can be in the future, the length can't be zero, and no other session of the game can end at the same minute. A session that overlaps another one shows a warning but can still be added. It counts like any other session in the totals, last played and the statistics, is marked in the list with a small icon ("Added by hand"), and the message that appears has an **Undo**. Steam and Windows games don't have the button: Steam counts play outside Lattis itself. A Heroic game does, with a note that play through Heroic is counted already. When the history shows linked copies together, a **Copy** choice picks which one gets the session.
- **In the calendar**, the arrows beside the month step through the months (not past the current one), days of the neighbouring months are dimmed but can be picked, and days in the future can't. With the keyboard, the arrow keys move a day or a week, Page Up and Page Down a month, Home goes to today, Enter picks and Esc closes it. The week starts on your system's first day of the week. Its colors and sizes are in Customize, under **Calendar**.
- **To delete several sessions**, click them with Ctrl (or Shift for a range, as in the game list) and press **Delete selected** at the bottom left. One Undo brings them all back. Esc clears the selection.
- **To delete a session**, right-click it and choose **Delete session**. It goes at once, and the message that appears has an **Undo**. For an emulated or Linux game the minutes also come off its play time, so a game left running overnight is easy to fix. A Steam game's play time is Steam's, so it stays; only the session leaves the history and the statistics.

Sessions are in your data folder, one file per game (see [Your data folder](data-folder.md)).

## Library overview

The button above Play statistics on the rail (or F7) opens the Library overview: the state of the library now, where Play statistics is about play over time. A **Play statistics** button at the top right switches to that page. Hidden games are left out of every number, and a game linked to copies on other platforms counts once.

- **Games, Hours played, Backlog and Achievements** at the top: how many games the library has and how many are installed, the total play time (and as days, "N days of your life"), how many games have the status Not started, and the share of achievements you have unlocked with how many games are perfect. Only Steam has achievements Lattis can read, so the Achievements tile covers Steam games (installed or played) and says so; a share over the whole library would be wrong. It shows `…` for a moment while Lattis reads Steam's local files (no network), and a dash when no Steam game has achievements. A game you play is read again the next time the page opens.
- **Continue playing:** the installed games you played most recently, newest first, up to seven, sharing the card's width, each with when it was last played ("2 h ago", "Yesterday", "3 weeks ago", "5 months ago"). Hover a cover to zoom it in slightly, darken its lower part and show a round **Play** button in its bottom right corner; click the cover to find the game in the list.
- **Storage:** a bar for each drive that holds installed games, with its free space beside its name (its label, or else its folder, as Home for `/home`). The bar is split by platform, largest first, then Other, then free space; a platform taking less than 1% of the drive is counted in Other (the theme's `storage_fold_percent`). Each bar has its own legend under it, with what that platform takes on that drive. Hover a part of the bar or a legend entry to see its size and share of the drive; for Other, a tooltip names the platforms counted in it. A game whose size isn't known (a Proton shortcut Lattis can't measure, say) isn't added up, and the drive says how many were left out. Under the bars, **Largest games** lists the four installed games that take the most space, each with when it was last played and a thin bar showing its size against the largest. Click a row to find the game in the list; to uninstall it, use its page or right-click menu. **Show all games by size**, beside the heading, lists every installed game, largest first.
- **Games by platform:** beside Storage (below it in a narrow window), how many games each platform has, its share of the library and how many of them are installed, with the totals of installed and played games below. The bar colors are the ones in Storage. A game linked to copies on other platforms counts on each platform here.
- **Needs attention:** what could be fixed, one line each with an icon, a count and a button, and the total number of items at the top right of the card. The line is left out when there is nothing to report, and the card when no line is left. **Played games with saves never backed up** counts the installed games you have played that have no save backup yet (Back up makes them now, as Back up now in Save backups does); it counts a game even when it has no save files Lattis can find, and ROM games aren't covered. **Games without a cover** and **Games without game info** (no description, year or rating) are the ones Look up missing info could still fill in; games it found nothing for aren't counted, and Look up opens that window. **Steam updates waiting** shows the Steam games in the list. **ROMs not found** (a ROM that has gone from its folder) opens Settings → Platforms, and **Games whose emulator isn't set up** opens Settings → Emulators. Updates, ROMs and emulators count every copy of a linked game; the rest count it once.
- **From your backlog:** one installed game with the status Not started, picked at random: its cover, title, platform and year. **Play** starts it, **Shuffle** fades to another one, and clicking the cover finds the game in the list. It picks a new game each time the page opens.
- **Nearly done** (labeled "Games with 80%+ achievements"): Steam games at 80% or more of their achievements but not all, highest first, up to four, with how many are unlocked. Click one to find the game in the list. The card is left out when there are none.
- **Status, Top tags and Release years:** how many games have each status, each bar sized against the games that have a status (games with no status are a note under the bars; click it to list them) (under Games by platform, beside Storage), the tags on the most games, as chips in their own colors with their counts, three rows of them stretched to end evenly (click one to filter by it), and a column for each release year (hover one for its count in a tooltip; the card says which year has the most games).
- **Compatibility:** the ProtonDB tiers of your installed Steam games, as one bar and a row per tier showing their share, with a row for the games that have no reports, how many installed games run natively and how many through Proton (Linux programs and Proton shortcuts count too, not only Steam games), and the Proton version most games use. While the page is open Lattis asks ProtonDB about the games it has no recent answer for, one at a time, so the card fills in over a minute or two and stays filled for a week; it only asks for games that are installed. A Steam game counts as running through Proton when you chose a Proton version for it, or when Steam has a default Proton and ProtonDB doesn't list the game as native; a game with no ProtonDB answer yet isn't counted either way.
- **Save backups:** how many of the installed games you have played have their saves backed up (a bar), when the last backup was made, how many backups there are and the space they take, in the backup folder from Settings → Library. **Back up now**, beside the card's title, backs up the saves of every installed game you have played at once, as the one on a game's page does for that game (the saves export in Settings → Library is separate and isn't counted here). The card is left out without any installed played game or backup.
- **Fun facts:** your longest session (from the play history), the game that has waited longest in your Backlog (by the date it was added), and how many Steam games you have finished all achievements in; click one to go to it.

Every number leads somewhere: the top tiles brighten under the mouse and dip when pressed. Click a tile, a platform, a status or a tag and the game list opens with just those games (every other filter and the search are cleared, and games that aren't installed are included). The Games tile shows the whole library, Backlog the games with that status, Achievements the Steam games, and Hours played opens Play statistics. A release year column opens the games of that year (the year range set to it). Up, Down, Page Up, Page Down, Home and End scroll the page.

In a narrow window the cards go from three columns to two and then one, and Continue playing scrolls sideways when its covers don't fit. Esc, or F7 again, returns to the games. The colors, sizes and spacing are the theme's `[overview]` section, which follows `[stats]` unless you change it, and `platform_colors` there lists the colors given, in the order the platforms are listed, to platforms in the Storage bars and Games by platform that have no color of their own (built-in platforms have one; set `color` on a platform in `platforms.toml` to change it).

## Play statistics

The bar chart button on the rail (or F9) opens the statistics page over the whole window. It shows one month or one year at a time, from the sessions only, so time under Before history isn't in it. It opens on the current month.

- **Month | Year:** the toggle beside the title switches between the two views. Going from Year back to Month returns to the month you left, or to the year's last month you can pick if you'd moved to another year.
- **Changing the period:** the arrows beside the title (or Left and Right) step by a month, or by a year in the Year view. Click the title to pick from a popup: a grid of months (with the year's arrows, Page Up and Page Down to change the year, and **All of 2026** to see the whole year), or in the Year view a list of years with their hours. Arrow keys move through it, Enter picks and Esc closes it. Months with no history are dimmed; months before your first session or after the current one can't be picked.
- **The strip:** at the right, a small bar for each month of the year you're looking at; click one to go there. Stepping into another year redraws it. In the Year view it shows one bar per year instead.
- **Tiles:** hours played, sessions, games played, and the change from the previous month (or year). For the current month or year the change compares the same days of the previous one, so a part period doesn't look like a drop.
- **Month charts:** hours for each day, with the weeks marked under them (hover a day for the games played), hours by platform, and hours by week.
- **Year charts:** hours for each month (hover a month for its top games, click it to open that month), hours by weekday, and hours by platform.
- **Most played:** the five games played most in the period; click one to select it in the list.
- **Highlights:** the longest session, the longest streak of days played (in a year it can run across months), and the average session.

Hidden games are left out unless **Hidden games** is on in the filters. Click **Back to games** or press Esc to return to the list, with the same game selected.

## Statuses and favorites

To keep track of where you are with a game, set its **status** in Properties: Backlog, Playing, Finished (the story), Completed (100%) or Dropped. The list can be filtered and grouped by status (see [Finding games](finding-games.md)).

The star button on a game's page (Ctrl+D) adds it to your favorites.
