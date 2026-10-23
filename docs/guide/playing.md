# Playing

## Launching

Select a game and click **Launch**, press Enter, or double-click it in the list. The Launch button's tooltip says what it will use ("Launch with Ryubing (AppImage)").

- **Emulated games** start in the game's emulator: the one pinned in its Properties if it's installed, else the first installed one in its platform's list. If none is installed, a message says which to install, and the game's page shows **Install** and the emulator's name in place of Launch (the first emulator in the platform's list, if it can be installed from the app). The click installs it the way [Settings → Emulators](emulators.md#installing-an-emulator) does, taking its recommended route; the button shows the progress and becomes Launch once the emulator is found. A failure shows its reason in a message, with a link to the Emulators page. Pressing Enter or double-clicking in the list never installs anything: it shows the message.
- **Steam and Windows games** start through Steam.
- **Linux games** start directly.

**Open emulator** starts the emulator without a game, and **Open folder** opens the folder the game is in. **Open save folder**, in the game's menu, opens where its saves are (see [Properties](adding-games.md#properties) for how it's found); it's also a button you can add to the game's page in `[game_page] buttons`, shown only when the game has a save folder.

When a game won't start, its emulator's or program's output is in `~/.cache/lattis/launch/<game id>.log`.

## While a game runs

The Launch button says "Close" ("X STOP" in the Steam style), and launching again does nothing. Click it and it asks first, then ends the game at once. Prefer closing the game itself, since stopping an emulator this way loses unsaved progress.

On Settings → General, **While a game runs** chooses whether the launcher stays open or minimizes and comes back when the game ends. Some desktops (Hyprland, sway) can't minimize, so it stays open there.

## Play time

When an emulated or Linux game closes, the session is added to its play time and its last played is set.

- Sessions shorter than the **Minimum session length** on Settings → General (30 seconds at first) don't count, so a game that crashes on start changes nothing.
- A game that exits with an error within a few seconds didn't really start: a message says so.
- The game keeps running if you quit Lattis, but that session isn't recorded.
- Play time and last played can be corrected by hand in the game's Properties.

Steam and Windows games show the play time Steam records instead, including sessions started from Steam.

## Play history

Lattis keeps each session, so you can see when you played and for how long. Click the play time on a game's page, or choose **Play history** in its right-click menu, to open the game's history: its total, this week and this month, then the sessions, newest first (the day, the start and end times and the length: "Today, 20:46 – 22:41, 1h 55m").

- **Before history** is play time Lattis has no sessions for: what a game had before Lattis started keeping history, or what you typed into its Properties.
- **Steam and Windows games** get a session each time Steam's play time grows while Lattis is open. Play from Steam while Lattis was closed can't be told apart, so it shows as one entry, "Played from Steam while Lattis was closed". Games Steam already had play time for start with it all under Before history.
- **To delete several sessions**, click them with Ctrl (or Shift for a range, as in the game list) and press **Delete selected** at the bottom left. One Undo brings them all back. Esc clears the selection.
- **To delete a session**, right-click it and choose **Delete session**. It goes at once, and the message that appears has an **Undo**. For an emulated or Linux game the minutes also come off its play time, so a game left running overnight is easy to fix. A Steam game's play time is Steam's, so it stays; only the session leaves the history and the statistics.

Sessions are in your data folder, one file per game (see [Your data folder](data-folder.md)).

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
