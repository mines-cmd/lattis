# Backups

Lattis makes two kinds of backup, each a single `.tar.zst` file: one of your **library**, and one of your **saves**. One **Import** restores either. All three are on Settings → Library and in the app menu.

## Back up library

Your whole [data folder](data-folder.md): your games and everything you've set on them (tags, favorites, play time, launch options), their art, your themes, your settings, your platforms and emulators, and your API keys (uncheck "API keys" to leave them out).

ROMs are never included, even ones you keep inside the data folder.

## Back up saves

Your games' saves, which live outside the data folder:

- **Every configured emulator's saves**, from wherever that emulator keeps them (save states included), and saves kept next to the ROM, as mGBA and melonDS do.
- **Keys and firmware** for the emulators that need them (uncheck to leave them out).
- **Windows games' saves**: each game's Wine prefix, without the files Proton itself put there, about 8 MB a game.

An emulator that isn't installed, or one Lattis doesn't know where it keeps saves, is skipped and named in the dialog.

Each dialog shows how big the backup will be, what the other backup covers, and what neither does.

## What isn't backed up

- ROMs.
- Steam itself and Steam games' saves (Steam Cloud keeps those).
- Linux games' saves: each game keeps them in its own place.
- Steam games' own Proton prefixes.

A backup can't be made while a game you launched from Lattis is running, since it may be writing its saves.

## Restoring

**Import** opens a backup and shows what's in it, when it was made, and what it will replace.

- **A library backup** replaces your whole data folder, then restarts Lattis.
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
