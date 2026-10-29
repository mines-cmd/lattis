# Steam games

When Steam is installed, its installed games show up in Lattis by themselves, launch through Steam, and show the play time Steam records. You can edit their launch options and Proton version in Lattis, and turn on presets like MangoHud with a check box.

## Finding Steam

Lattis looks for Steam in `~/.steam/steam` (a native install), then in the Flatpak's folder. The account is the one that logged in last. Settings → Steam shows what was found, with **Change** to pick another folder and a choice of account if there are several.

To turn Steam off in Lattis, switch off **Import Steam games** on Settings → Steam. Steam games then show as not installed.

## Your Steam games

- **Installed games are imported**, and games Steam installs or uninstalls later show up without a restart. The rest of your Steam library (games you own but haven't installed) isn't, unless you turn on **Whole library** (below).
- **An uninstalled game stays in the library**, dimmed, with **Install** in place of Launch, which hands off to Steam. Its play time, tags and art stay. Hide it if you don't want to see it.
- **Steam's tools aren't games:** Proton, the Steam Linux Runtimes and the like are left out.
- **Art:** Steam's own art for the game (your custom art in Steam first) is copied into Lattis once. Art you choose yourself wins. Remove Steam's art in Properties and it isn't copied back; the art browser's Steam tab (see [Game info and art](game-info.md)) brings it back.

## Whole library

**Settings → Steam → Whole library** also lists the games you own but haven't installed, and the games your Steam Family shares with you. They show as not installed, and **Install** starts the download in Steam. Turn the switch off and they leave the library again (the toast has an Undo).

- **Sign in with Steam** shows a QR code: scan it with the Steam app on your phone and approve. It adds a session to your account, like a browser's, and Lattis never sees your password. The sign-in is kept in your keyring (GNOME Keyring, KWallet); without one it lasts until Lattis quits. **Sign out** forgets it here; remove the session from Steam's authorized devices to end it there too.
- **A Web API key** instead fetches the games you own (not the family's) without signing in. Your Steam profile's Game details must be public.
- Games are fetched once a day and when you press **Sync now**; the line beside it says how many games and how long ago.
- **Family games** have a **Family** chip on their page ("Shared by Sam"). The filter panel's **Steam** section shows only **Owned** or only **Family** games. The chip can be removed or moved like the others (Settings → Game page).
- The setup guide's Steam step has the same sign-in, under **Import your whole library**.

## Updates

When Steam has an update for a game, Lattis shows it in three places:

- **The Launch button** becomes **Update**. Steam doesn't let other programs start an update, so a click opens the game's page in Steam, where Steam's own Update button is. While Steam downloads, the button says **Updating**, and **Paused** if you paused the download in Steam; a click opens Steam's downloads.
- **An "Update available" chip** next to the play time shows the state: **Waiting** with the download size once Steam knows it, **Downloading** with the percentage, or **Paused** with the percentage. The percentage is the one Steam shows and moves while it downloads.
- **An arrow and a colored title** in the list mark the game. Turn the arrow off with **Show an icon on games with a Steam update** in Settings → Style (`list.show_update_icon`).

**A download bar** shows along the bottom of the window while Steam downloads a game, on every game's page (not only the downloading game's). It has the game's icon, **Updating**, **Installing** (a first install) or **Paused** with its name, the percentage and a progress bar. Click it to open Steam's downloads. Steam downloads one game at a time, so the bar shows the one that runs, or a paused one when none runs. It looks like the other panels and follows the theme: the `[download_bar]` settings, where `show = false` hides it.

**Hiding the download bar.** The chevron at the bar's right end slides the bar down out of view and leaves a small pill in the bottom-right corner with a download icon, the percentage and a chevron up. Click the pill to bring the bar back. It stays hidden, across downloads and restarts, until you show it again (`download_bar_collapsed` in `[general]` in `settings.toml` holds the choice). A theme can remove the chevron (`chevron_size = 0`), but a bar you already hid still shows as the pill.

**A game Steam is installing for the first time** stays in the list, even with **Installed only** on. Its button says **Installing**, or **Paused** if you paused the download in Steam, and a click opens Steam's downloads. It can't be launched until Steam finishes.

Lattis only knows what Steam has written down. It doesn't check online, so a patch Steam hasn't noticed yet shows nothing, and a game Steam hasn't started updating shows no size. While the game is updating you launch it from Steam; Lattis has no way to start the old version.

The labels and colors are theme settings: the `update_*`, `updating_*`, `installing_label` and `paused_label` settings of `[launch_button]`, and `update_icon_color` in `[list]`.

## Clips and screenshots

A Steam game's page (and a Proton game's, once it's added to Steam) shows your own Steam clips (Steam's Game Recording) and screenshots in a **Media** row under the description, above the tags, mixed together, newest first. A clip shows its length. Click one to open it in Lattis, or right-click it for **Show in folder**. Lattis only reads Steam's files; it never changes them.

- **Stepping through:** the ‹ and › buttons at the viewer's sides, or Page Up and Page Down, go to the previous or next item in the row. On a screenshot, Left and Right do the same.
- **Zoom:** on a screenshot, scroll the wheel to zoom toward the pointer (up to 8×) or use the slider in the picture's bottom right corner (it shows while the pointer moves over the picture), and drag to move the zoomed picture. `+` and `-` zoom from the middle and `0` or the magnifier beside the slider goes back to the whole picture; stepping to another item resets it.
- **Your own folders:** a Steam, Windows or Linux game can have media folders (Properties → **Media folders**, see [Adding games](adding-games.md)) for OBS recordings, say, or another screenshot tool. Every picture (`png`, `jpg`, `jpeg`, `webp`, `bmp`) and video (`mp4`, `mkv`, `webm`, `mov`) in them and their subfolders shows in the Media row and the Gallery, whatever it is called, mixed with Steam's and newest first by the file's date. Hidden files and folders are skipped, links to folders aren't followed, and something Steam already shows is listed once. A video plays as it is, with no ffmpeg; its thumbnail is made the first time it is listed (a film icon shows until then). A folder with more than 2000 files shows the newest 2000, and the row says so. Lattis only reads these folders; it never moves, renames or deletes their files. A game outside Steam, with no folder, has no Media row.
- **Screenshots:** from Steam, only its own copies, the ones Steam's screenshot manager shows. To leave them out, turn off **Show screenshots** on Settings → Game page; **Show video** does the same for the clips.
- **In the viewer:** the controls lie over the bottom of the video, and the date and position (with the game and Go to game in the Gallery) over its top. While a clip plays they fade out, with the pointer, 2 seconds after the mouse stops, or as soon as it leaves the video (`[media] viewer_hide_delay`; 0 keeps them), and come back when it moves or a key is pressed. Hovering the seek bar shows the time under the pointer; the volume slider slides out beside the speaker. Space plays and pauses, Left and Right skip 5 seconds, Up and Down change the volume (remembered), M mutes (a sign in the middle shows play, pause, the skip or the volume), F goes fullscreen, Esc closes (or leaves the fullscreen the viewer turned on), and a click outside the video closes it.
- **ffmpeg:** Steam saves clips in pieces that need joining before they play, and Lattis uses `ffmpeg` for that. If it's on your system, Lattis uses it. If not, the Media row offers **Install ffmpeg**, which downloads it (the size is shown; from github.com/BtbN/FFmpeg-Builds) only when you click. The **ffmpeg** section on Settings → Steam appears once Lattis installed it, with **Remove** to delete that copy.
- **Clips that moved:** if you changed where Steam saves recordings, clips in both the old and the new folder show.
- **Hiding it:** click the **Media** heading to fold the row. To remove it from every game's page, or move it, use Settings → Game page.
- **Looks:** the Media heading shows how many items there are, the row sits on one card the items scroll inside, each item has a drop shadow, and the row and viewer follow the theme's `[media]` settings.

## Achievements

A Steam game's page shows its achievements in a panel below the info panel: how many are unlocked, a progress bar and a grid of icons (colored when unlocked, gray when locked). Hover an icon to see its name, description and when you unlocked it. A hidden achievement you haven't unlocked shows a **?** and no details, as in Steam.

Click the panel or **Show all** to open the full list: your progress, a search box, an All / Unlocked / Locked filter, and the achievements in **Unlocked**, **Locked** and **Hidden** sections with their icons, descriptions and unlock dates. Esc, the close button or a click outside closes it.

- **Where it comes from:** the files Steam keeps on this computer, for the Steam user Lattis found. No API key or public profile is needed, and nothing is changed. If nothing shows for a game, open the game in Steam once so it fetches the list, then select it again here.
- **The only download** is the icons, from Steam's image servers, kept on disk afterwards. Offline, a plain square stands in for an icon not saved yet.
- **Language:** the names and descriptions are in the language Steam runs in.
- **Hiding it:** turn off **Show achievements** on Settings → Game page, or in the info panel's right-click menu. The panel follows the theme's `[achievements]` settings.

## Launching and play time

**Launch** starts the game through Steam, without opening Steam's window. **Open in Steam** shows the game's page in Steam.

**Play time and last played are Steam's**, including sessions started from Steam itself and games borrowed through Family Sharing. They update a moment after the game closes. They can't be edited in Lattis.

A game started from Steam rather than from Lattis doesn't show as running.

After **Launch**, the game shows as running while Steam starts it, for up to a minute and a half. If Steam doesn't start it, for example because a family member is playing a game you borrowed, click **Close** to stop waiting.

**Uninstall**, in an installed game's Properties or its right-click menu, hands off to Steam, which asks and removes the files. It is also there while a first install is downloading or paused, where it cancels the download, as in Steam. Lattis never deletes a Steam game's files itself.

## Launch options and Proton version

A Steam game's Properties have a Launch card:

- **Proton version:** "Steam's default" or any installed compatibility tool: Valve's Protons and ones like GE-Proton in `compatibilitytools.d`.
- **Presets:** a check box per preset. Checking one adds it to the launch options; unchecking takes it back out.
- **Launch options:** the same text as in Steam's Properties, like `mangohud %command%`. Typing a preset's part by hand checks its box.

### Changes wait for Steam to close

Steam rewrites its settings when it exits, so it would undo any change made while it runs. Lattis only writes to Steam while Steam is closed:

- **Steam closed:** your change goes into Steam as soon as you save.
- **Steam running:** the change waits. The game's settings button gets a badge, the Launch card says "Waiting for Steam to close to apply this", and a Steam chip beside Launch says what's waiting. It's applied when Steam next closes.
- **Close Steam and apply** (on the Launch card, Settings → Steam, the chip, or "Apply N Steam changes" in the app menu) closes Steam, writes the changes and starts Steam again.
- **Undo** on the chip, or per game on Settings → Steam, drops the waiting changes and keeps what Steam has.
- **Launching a game with a change waiting** while Steam runs asks: "Restart Steam and launch", "Launch with the old options", or Cancel.

**Changes made in Steam** come into Lattis within a moment. If the same game's value was changed in both before they met, Steam's wins.

Lattis changes only the values you edited in Steam's files, and keeps a copy of each file the first time it writes it, in `~/.local/state/lattis/steam-backup/`.

## Presets

Three presets are built in:

| Preset | Adds |
|---|---|
| MangoHud | `mangohud` before `%command%` (the performance overlay) |
| GameMode | `gamemoderun` before `%command%` |
| Wayland | `PROTON_ENABLE_WAYLAND=1` (GE-Proton only) |

**Default presets**, on Settings → Steam (or the setup guide), are turned on for each new Steam game as it's imported. Games already in the library keep theirs. **Apply to all** and **Remove from all** turn a preset on or off in every Steam game at once, waiting for Steam like any change.

### Your own presets

Add presets in `settings.toml` (**Open settings.toml** on Settings → Steam). A preset has a name and any of `env` (variables), `wrapper` (a command before `%command%`) and `args` (after it):

```toml
[steam.presets.gamescope]
name = "Gamescope"
wrapper = ["gamescope", "-f", "--"]

[steam.presets.hdr]
name = "HDR"
env = { PROTON_ENABLE_HDR = "1", DXVK_HDR = "1" }
```

Save the file and they show up as check boxes in every Steam, Windows and Linux game's Properties.

## Restoring on another computer

Steam itself isn't part of a Lattis backup, but your Steam games are: their tags, art and launch options. After restoring, games Steam hasn't installed yet show as not installed, with Install. Their launch options and Proton versions are written into Steam again the next time it's closed.
