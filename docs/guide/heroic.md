# Epic games through Heroic

When [Heroic Games Launcher](https://heroicgameslauncher.com) is installed and signed in to Epic, your Epic games show up in Lattis by themselves, on their own **Epic** platform, and launch through Heroic. Heroic stays in charge of installing, updating, Wine and cloud saves. Lattis only reads Heroic's files and changes nothing in them.

GOG and Amazon games in Heroic, and games you added to Heroic yourself, aren't shown yet.

## Finding Heroic

Lattis looks for Heroic in `~/.config/heroic` (a native install or AppImage), then in the Flatpak's folder. Settings → Heroic shows what was found, with **Change** to pick another folder (the one with `store_cache` in it) and **Find automatically** to go back.

To turn Heroic off in Lattis, switch off **Import Epic games** on Settings → Heroic. Epic games then show as not installed.

## Your Epic games

- **Your whole library is listed,** from what Heroic last read from your account, so you don't sign in to anything in Lattis. Games that aren't installed are dimmed, with **Install** in place of Launch. The **Installed only** filter, on at first, keeps them out of the list until you turn it off (see [Finding games](finding-games.md)). Switch off **Import the whole library** on Settings → Heroic to leave them out for good; the message that appears has an **Undo**.
- **Games Heroic installs or uninstalls** change in Lattis without a restart.
- **A game that leaves your Epic library** drops out of the list, unless it's installed. If it comes back, so do its play time, tags and art.
- **DLC isn't listed.**
- **Art:** the images Heroic has already shown are copied in when a game first arrives, and the rest comes from [looking the game up](game-info.md). Nothing is downloaded from Epic. New games are looked up as they arrive, so without API keys a game Heroic hasn't shown has no art.
- **"Add to Steam":** a game you added to Steam from Heroic isn't offered by **Add existing shortcuts**, since it's already in your library.

## Launching and installing

**Launch** hands the game to Heroic, which runs it without showing its window. If you have Heroic's window open, it hides.

The game shows as running until Heroic says it closed. If a launch stops early, for example because the install folder is gone, Heroic opens its own dialog and nothing ends the session in Lattis: use **Close** on the Launch button.

**Install** opens Heroic, copies the game's title and tells you to paste it in Heroic's search, because Heroic has no way to open a game's page from outside. Uninstall a game in Heroic too.

**Open Heroic** is on the game's page and in its menu (Ctrl+O), and on Settings → Heroic.

## Play time

An Epic game's play time and last played are kept by Lattis, and you can correct them in Properties as for a Linux game.

- **The first time** Lattis sees a game, its play time starts from Heroic's total.
- **After that,** whenever Heroic's total grows, the growth is added to the game's play time and to its [play history](playing.md#play-history). Only the growth is added, so your corrections stay. This counts play started from Heroic too.
- **A session under a minute** adds nothing, because Heroic's total rounds down.
- **If Heroic's total drops,** for example after a reinstall, nothing changes in Lattis and counting goes on from the new total.

## Properties

The Launch tab shows the folder Heroic installed the game in, and **Open Heroic**. Wine, launch options and updates for the game are in Heroic. Tags, art, status and the rest work as for any game.

Automatic save backups don't cover Epic games.
