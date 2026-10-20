# Steam games

When Steam is installed, its installed games show up in Lattis by themselves, launch through Steam, and show the play time Steam records. You can edit their launch options and Proton version in Lattis, and turn on presets like MangoHud with a check box.

## Finding Steam

Lattis looks for Steam in `~/.steam/steam` (a native install), then in the Flatpak's folder. The account is the one that logged in last. Settings → Steam shows what was found, with **Change** to pick another folder and a choice of account if there are several.

To turn Steam off in Lattis, switch off **Import Steam games** on Settings → Steam. Steam games then show as not installed.

## Your Steam games

- **Installed games are imported**, and games Steam installs or uninstalls later show up without a restart. The rest of your Steam library (games you own but haven't installed) isn't.
- **An uninstalled game stays in the library**, dimmed, with **Install** in place of Launch, which hands off to Steam. Its play time, tags and art stay. Hide it if you don't want to see it.
- **Steam's tools aren't games:** Proton, the Steam Linux Runtimes and the like are left out.
- **Art:** Steam's own art for the game (your custom art in Steam first) is copied into Lattis once. Art you choose yourself wins. Remove Steam's art in Properties and it isn't copied back.

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

## Launching and play time

**Launch** starts the game through Steam, without opening Steam's window. **Open in Steam** shows the game's page in Steam.

**Play time and last played are Steam's**, including sessions started from Steam itself and games borrowed through Family Sharing. They update a moment after the game closes. They can't be edited in Lattis.

A game started from Steam rather than from Lattis doesn't show as running.

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
