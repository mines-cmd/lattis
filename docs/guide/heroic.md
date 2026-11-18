# Epic games through Heroic

When [Heroic Games Launcher](https://heroicgameslauncher.com) is installed and signed in to Epic, your Epic games show up in Lattis by themselves, on their own **Epic** platform, and launch through Heroic. Heroic stays in charge of installing, updating and cloud saves. Lattis reads Heroic's files, and writes only a game's Wine version, launch options and presets when you save them in Properties.

GOG and Amazon games in Heroic, and games you added to Heroic yourself, aren't shown yet.

## Finding Heroic

Lattis looks for Heroic in `~/.config/heroic` (a native install or AppImage), then in the Flatpak's folder. Settings → Heroic shows what was found, with **Change** to pick another folder (the one with `store_cache` in it) and **Find automatically** to go back.

To turn Heroic off in Lattis, switch off **Import Epic games** on Settings → Heroic. Epic games then show as not installed.

## In the setup guide

The setup guide's Heroic step comes right after Steam, and lets you choose what comes in before anything does. A card at the top says what Lattis found. Below it, the **Epic Games** card has the same two switches as Settings → Heroic: **Import Epic games**, with how many installed games will be added, and **Import the whole library**, with how many more you own, which come in as not installed. Both are on at first, and nothing is imported until you finish.

GOG and Amazon have their own cards, marked **Not implemented yet**. They show whether or not you have games in those stores, and nothing reads them.

What the step shows depends on what Lattis finds:

- **Heroic with your library:** "Heroic found", with its folder and whether it's native or a Flatpak. If its program is missing, a note says the games can't be launched.
- **Heroic with no library yet,** for example if you've never signed in: "Heroic found" and "It has no Epic library yet.", with **Choose folder** in case your library is somewhere else. The Epic Games card asks you to sign in to Epic in Heroic, and has **Open Heroic**.
- **No Heroic:** "Heroic wasn't found", with **Choose folder**, **Find automatically** once you've set a folder, and a link to Heroic's site. The store cards stay hidden.
- **Import Epic games off when the guide starts:** "Heroic wasn't looked for", until you turn the switch on.

A Heroic AppImage that isn't on your `PATH` and has never been signed in leaves nothing for Lattis to find, so it also shows "Heroic wasn't found".

If you sign in while the guide is open, what happens depends on whether Heroic had library files before. If it did, the counts fill in when Heroic refreshes its library. If it didn't, Lattis has nothing to watch and the step keeps saying there are no games, but Finish looks again, so your games still arrive.

When you run the guide again, the step counts what's already in your library and the switches take effect right away, as they do in Settings. Turning off **Import the whole library** there takes the games that aren't installed out of the library. The guide has no **Undo** for that, but turning the switch back on brings them back.

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

**Install** opens Heroic, copies the game's title and tells you to paste it in Heroic's search, because Heroic has no way to open a game's page from outside.

**Uninstall** (the game's menu and Properties) runs Heroic's own copy of Legendary, so Heroic doesn't need to be open. It deletes the game's files for good, not to the trash. If the game has a Wine prefix of its own (one no other game and not Heroic's default prefix uses), you can delete that too; it goes to the trash and holds the game's saves. The game stays in the list as not installed, with its play time, tags and art. With a Flatpak Heroic the button asks you to uninstall in Heroic instead.

**Open Heroic** is on the game's page and in its menu (Ctrl+O), and on Settings → Heroic.

## Play time

An Epic game's play time and last played are kept by Lattis, and you can correct them in Properties as for a Linux game.

- **The first time** Lattis sees a game, its play time starts from Heroic's total.
- **After that,** whenever Heroic's total grows, the growth is added to the game's play time and to its [play history](playing.md#play-history). Only the growth is added, so your corrections stay. This counts play started from Heroic too.
- **A session under a minute** adds nothing, because Heroic's total rounds down.
- **If Heroic's total drops,** for example after a reinstall, nothing changes in Lattis and counting goes on from the new total.

## Properties

The Launch tab shows the folder Heroic installed the game in, and **Open Heroic**. Below it, a Run card has the game's settings, read from Heroic's files for the game (or Heroic's defaults, for a game you never configured in Heroic) and written back when you save:

- **Wine version:** the Proton and Wine builds Heroic has installed, the compatibility tools in Steam's `compatibilitytools.d` (GE-Proton and the like) and the Wine on your system. If the game's build is gone, it's listed as "(not installed)".
- **Presets:** a check box per [preset](steam.md#presets). MangoHud and GameMode are Heroic's own switches for the game; your own presets add to Heroic's environment variables, wrapper commands and launch options. There are no default presets for Epic games: a new game keeps Heroic's settings until you change them.
- **Launch options:** Heroic's launch arguments for the game, written like Steam's: variables, a wrapper, then `%command%` and arguments after it. Typing a preset's part by hand checks its box.

An installed Epic game whose build is older than the newest one Epic lists (Heroic keeps that list up to date) shows the same "Update available" chip and list arrow as a Steam game with an update (see [Steam](steam.md#updates)). **Update** on the Launch button, the chip or the game's right-click menu runs Heroic's own Legendary in the background; the button reads **Updating** with the percentage, or **Queued** while another game's update runs (one at a time), and the download bar along the bottom of the game's page shows the running one. Click the bar to open the game's page, × to cancel it: Legendary keeps what it downloaded, so Update resumes. Quitting Lattis during an update asks first, then stops it. A Flatpak Heroic has no Legendary for Lattis to run, so Update tells you to update in Heroic. Heroic's other settings (Esync, Fsync, DXVK, the Wine prefix) are in Heroic. Tags, art, status and the rest work as for any game. Epic games can show the [ProtonDB chip](steam.md) too once a lookup has matched them to a game with a Steam app ID, and count in the overview's Compatibility card while installed.

Once a day, shortly after Lattis starts, it asks Epic for the newest builds through Heroic's own Legendary (`legendary list-installed --check-updates`, about 2.4 seconds, in the background), so the chip and arrow are right even if you haven't opened Heroic. This contacts Epic and changes nothing in your games. Switch **Check for updates daily** off on Settings → Heroic to stop it. It needs Legendary, so a Flatpak Heroic is left out, and a failed check (you are offline) is only written to the log.

### Changes wait for Heroic to close

Heroic keeps a game's settings in memory and writes them all back, so it would undo a change made while it runs. Lattis only writes to Heroic's files while Heroic is closed:

- **Heroic closed:** your change is written when you save.
- **Heroic running** (one minimized to the tray counts): the change waits. The game's settings button gets a badge, a yellow banner right of the toolbar names the game and what changed (Wine version, launch options, presets), and the Launch card says "Waiting for Heroic to close to apply this" (while you're still in Properties: "Heroic is running: saving applies this when it closes"). It's applied within a couple of seconds of Heroic closing, even if Lattis was restarted in between. The banner is the one [Steam's waiting changes](steam.md#changes-wait-for-steam-to-close) use: with games waiting on both launchers it lists each with its launcher, for example "Waiting to close: Hades (Steam), A Short Hike (Heroic)". **Show** goes to the game, or lists them when there are several. **Close Heroic** on the banner (and **Close Heroic and apply** under the Launch card, which saves first) closes Heroic and writes the changes once it has gone. It doesn't start Heroic again. It refuses while an Epic game runs, since closing Heroic could end the game, and gives up after 15 seconds if Heroic is still open. **Undo** asks first, then drops the waiting changes, and Heroic keeps the settings it has. With more than one game waiting (on Heroic, Steam or both) it lists them all, ticked, so you can untick the ones to keep. **Close Steam** appears only while a Steam game waits.

## Automatic save backups

Epic games can have their saves backed up when they close, like Steam and Linux games: switch on **Back up saves** on the Epic row of Settings → Platforms, or choose On in a game's Properties → **Backup options** → **Backup when the game closes**. It's off until you do. The [backups page](backups.md#automatic-save-backups) explains the rest.

Lattis finds an Epic game's saves from what Heroic and Epic's own files say, and changes none of them:

- **The game's Wine prefix** is the one Heroic uses for that game, else Heroic's default prefix. Which Windows user folder inside it is looked in depends on the Wine version the game runs with (`steamuser` for Proton, your own user name for Wine).
- **Epic's save folder** (the one Epic's cloud saves use) is the first choice when Heroic has it for the game. Otherwise the game is looked up in the saves list by its title, as a Proton game is.
- **A game with neither** gets no backup, and Properties → Saves says so. Add a save location there to back one up anyway.

If Heroic's cloud saves are on for the game, it may download Epic's copy over a save you restored, the next time you launch it. The restore question warns about it; turn the game's cloud sync off in Heroic first if you want the restored save to stay.

Backups of cloud saves themselves, and of GOG or Amazon games, aren't made.
