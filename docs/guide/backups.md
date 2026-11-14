# Backups

Lattis makes two kinds of backup by hand, each a single `.tar.zst` file: one of your **library**, and one of your **saves**. One **Import** restores either. All three are on Settings → Library and in the app menu. Lattis can also back up a game's saves by itself when the game closes: see [Automatic save backups](#automatic-save-backups).

## Back up library

Your whole [data folder](data-folder.md): your games and everything you've set on them (tags, favorites, play time, launch options), their art, your themes, your settings, your platforms and emulators, and your API keys, a Steam Web API key included (uncheck "API keys" to leave them out). Your Steam sign-in is never in a backup: on a new machine, sign in again in Settings → Steam.

Your mod managers come along too. A Windows mod manager's Steam shortcut isn't in the backup: after an Import, Lattis adds it back to Steam the next time it syncs, as soon as Steam is closed. The manager's own Wine prefix and its program files aren't backed up.

ROMs are never included, even ones you keep inside the data folder.

## Back up saves

Your games' saves, which live outside the data folder:

- **Every configured emulator's saves**, from wherever that emulator keeps them (save states included), and saves kept next to the ROM, as mGBA and melonDS do.
- **Keys and firmware** for the emulators that need them.
- **Games' saves**: for each Linux, Steam, Epic and Proton game that the [saves list](#automatic-save-backups) knows (or that you gave a save location in Properties), just that game's saves, wherever it keeps them. This is the same search automatic save backups use, whether or not automatic backup is on for the game.
- **Windows games' saves the saves list doesn't know**: for a launcher Proton game, its Wine prefix without the files Proton itself put there, about 8 MB a game.

Each emulator and each game has its own checkbox: uncheck a row to leave it out. The checkbox on a section's heading checks or unchecks all its rows, and shows a dash when only some are checked. What you uncheck is remembered for the next backup; emulators and games added later start checked.

Each game row shows its size and where its saves were found. A game whose saves come to more than the automatic backups' size limit (1 GB unless you changed it) is listed as "Too big, skipped" and left out, so a wrong match never fills your disk.

The games are found with the saves list, which is only downloaded once you turn on automatic backup for something. Until then the dialog says "Update the saves list to include Linux and Steam games" with a button that downloads it; nothing is downloaded without that click. Without the list, a game with a save location of its own is still found, an Epic game whose save folder Epic gives is too, and launcher Proton games still get the whole-prefix copy.

An emulator that isn't installed, or one Lattis doesn't know where it keeps saves, is skipped and named in the dialog.

Each dialog shows how big the backup will be, what the other backup covers, and what neither does.

## What isn't backed up

- ROMs.
- Steam itself. Steam Cloud keeps many Steam games' saves; the saves backup takes only what is on your disk.
- Steam games' own Proton prefixes, whole.
- Saves of games the saves list doesn't know, unless you gave them a save location (a launcher Proton game still gets the whole-prefix copy).
- Registry saves of Windows games, cloud saves (Steam Cloud, GOG, Epic; an Epic game's own save folder on disk is backed up, but Epic's cloud copy isn't), and an emulator's keys and firmware in automatic backups (use Back up saves for those).

A backup can't be made while a game you launched from Lattis is running, since it may be writing its saves.

## Automatic save backups

Games keep their saves in their own places, so Lattis finds them from the [Ludusavi manifest](https://github.com/mtkennerly/ludusavi-manifest), a list of where thousands of games keep their saves (its data comes from PCGamingWiki). With a game's backup on, closing the game makes a backup of its saves: a small `.tar.zst` in a folder you pick, the last few kept.

It covers Steam, Epic, Proton, Linux and emulated games. It's off until you turn it on, and nothing is downloaded before that.

**Emulated games** work differently: an emulator keeps every game's saves together, so closing a game backs up all of that emulator's save folders (and the saves it keeps next to ROMs), not keys or firmware. There is one backup per emulator, shared by all its games, in `emulator-<name>` inside the backup folder; it's skipped when nothing changed since the last one, so playing several games in a row makes few. If you add save locations to an emulated game (inside your home folder), they replace the emulator's folders: closing that game backs up only those, in a backup of its own named after the game. The saves list isn't needed. Turn it on with the **Back up saves** switch of an emulated platform, or per game as below; the game's Properties → Saves shows the emulator's backups and restores them.

**Turning it on**

- **For a platform:** the **Back up saves** switch on its row in Settings → Platforms (every platform with an emulator has it, and Steam, Epic, Proton and Linux).
- **For one game:** Properties → Saves tab → **Backup options** card → **Backup when the game closes**: Platform default, On or Off. On or Off wins over the platform.
- **A game with no saves:** switch on **This game has no saves** in the same card. The menu under it folds away, and the Saves card says there are no saves to back up. Backups skip the game, Back up now in the overview leaves it out, and the overview doesn't count it as never backed up.

The first time you turn it on, Lattis downloads the manifest. Settings → Library → **Game saves** shows its state and has **Update now**; while backup is on, Lattis checks for a newer list about once a day. That section also holds the backup folder (default `~/.local/share/lattis-saves/`, outside the data folder so a library backup stays small; point it at a synced folder to share saves between computers) and how many backups to keep (default 5; older ones go to the trash).

**What is backed up.** Lattis matches a game to the list by its Steam ID, or else by its title. Linux games' save paths are expanded from your home and the game's folder; Steam and Proton games' Windows save paths are looked for inside the game's Proton prefix. What isn't on disk is skipped. The save locations you add in Properties are backed up too. A launcher Proton game with no match gets the slim whole-prefix copy instead. Nothing is written for a game with no match and no save location.

**Epic games** (run through [Heroic](heroic.md#automatic-save-backups)) are found from Epic's own save folder when Heroic knows it for the game, which is exact; the saves list is used only for a game Epic gives no folder for, by its title. The folder is looked for in the game's Wine prefix, as Heroic set it for that game. Properties → Saves then says "Epic's save folder" where other games show the matched game, and Change match is hidden, as the list isn't used. An Epic game with neither gets nothing, not its whole prefix, since a Heroic prefix can be shared by many games. Restoring one of these backups while Heroic's cloud sync is on for the game shows a warning: Heroic may download Epic's copy over the restored saves, so turn sync off in Heroic first.

A game played for less than the minimum session length makes no backup, and neither does one whose saves haven't changed since the last. A game whose saves come to over 1 GB is skipped, with a notice naming it. A failure shows a notice and never stops the next launch.

**In a game's Properties,** the **Saves** card (on the Saves, or Saves & mods, tab) has two sections. **Backups** comes first: **Back up now** makes one at once, the folder button opens the backups folder, and the list shows every backup. The ? beside **Backups** says how many are kept and that backing up, renaming, deleting and restoring happen at once, and Cancel doesn't undo them (mod manager links wait for Save). In the list: a backup shows its date, or its name once it has one; the pencil names one to keep it (a named backup, marked with a disk icon, is never deleted to make room, and doesn't count toward the number kept), the bin moves one to the trash, and **Restore** asks first, then puts it back. **Save locations** comes second: every folder or file a backup takes, each with a copy button. The ones you added come first, with × to remove them; **Add** adds another. The ones Lattis found follow under the matched game, the first three shown and the rest behind a "more" button. **Change match** there picks the right game from the list when the match is missing or wrong (Lattis remembers it). To see the newest one on the game's page, tick the **Last backup** chip in Settings → Game page; it is off by default. Under the Saves card, the **Backup options** card holds **This game has no saves** (click the switch or its name) and **Backup when the game closes**.

**Restore** puts the files back where they came from, after moving the ones there now aside to `<name>.before-import-<date>` (a notice names them). It won't run while the game is running. Import opens these backups too, so one copied to another computer restores there.

## Restoring

**Import** opens a backup and shows what's in it, when it was made, and what it will replace.

- **A library backup** replaces your whole data folder, then restarts Lattis.
- **A game's saves backup** (from automatic backup) restores that game's files, as above.
- **A saves backup** lets you pick what to restore, part by part (an emulator's saves, its keys, a game's saves), each with where it'll go. Saves go into each emulator as it's installed now, even if it's installed differently than when you backed up (a Flatpak instead of an AppImage). Saves kept next to a ROM follow the ROM to wherever it is now.

**Nothing is overwritten.** What a restore replaces is moved aside, to a folder named `<folder>.before-import-<date>`, and a notice names each one. When you're happy with the result, delete them from Settings → Library's Backup section.

A part that can't be restored yet (an emulator that isn't installed, a Windows game that hasn't been launched on this computer yet) says why and stays in the backup for a later Import.

### On a new computer

1. Install Lattis, and Steam and your emulators.
2. Restore the library: from the setup guide's Welcome step, or Import.
3. Point any ROM folders that moved at their new place, on Settings → Platforms.
4. Restore the saves with Import.
5. Launch each Windows game once, then Import the saves again for them, if they were skipped.

Steam games come back as not installed until Steam installs them; their launch options are written into Steam again.

## Backing up by hand

The data folder is all there is to back up: copying `~/.local/share/lattis/` somewhere safe and back works too. The backup files are ordinary archives, so `tar --zstd -xf <file>` opens them; `manifest.toml` inside says where everything came from.
