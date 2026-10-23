# Emulators and ROM folders

Emulated games come from **ROM folders**: folders you point Lattis at, one or more per platform. Lattis finds the games in them by itself and launches each with the platform's emulator.

## Platforms

A platform (Switch, 3DS, GameCube…) is a button in the rail and a group in the game list. Settings → Platforms lists them in rail order.

- **Add platform**, give it a name, then add its emulators and its ROM folders.
- **Drag a platform by the dots at its left end**, or type a place in the box (marked #), to change its place in the rail; holding it near the top or bottom of the list scrolls it. The move can be undone from the message that follows. Ctrl+2, Ctrl+3… jump to the platforms in that order.
- A platform shows in the rail once it has games. Open a platform and turn off **Show in the rail**, or right-click its button in the rail and choose **Hide from the rail**, to hide its button (`in_rail = false` in `platforms.toml`); its games still show in All games, and Ctrl+2, Ctrl+3… skip it.
- Behind each platform's name is an id made from it (`switch`, `3ds`). Games refer to their platform by this id, so it can't change once games use it; the name can.
- **Remove platform** keeps its games in the library, with their favorites and play time. Add a platform with the same id to show them again.

Steam, Proton and Linux are platforms too. Lattis adds them by itself when they're needed, at the top of the rail in that order, and you can rename or move them like any other.

## Emulators

Lattis knows twelve emulators out of the box, among them Ryubing (Switch), Azahar (3DS), Dolphin (GameCube and Wii), Cemu (Wii U), PCSX2 (PS2), RPCS3 (PS3), shadPS4 (PS4), PPSSPP (PSP), mGBA and melonDS. Settings → Emulators lists them all.

**Installed emulators are found by themselves.** Lattis looks for each one as a Flatpak, as a program on your `PATH`, or as an AppImage in `~/Applications` or `~/AppImages`. If yours is somewhere else, choose its program with **Browse**; **Detect automatically** goes back to finding it. "(portable)" after an install means the emulator keeps its data beside its program.

**Arguments** added on an emulator's row go to every game it runs, like `--fullscreen`. A single game can have its own arguments too, in its Properties.

### Installing an emulator

A built-in emulator that isn't found says "Not found" on its row, which is normal on a fresh system. Open the row to install it: the card offers one tile for each way to install it, with **Install AppImage** and **Install Flatpak** buttons. The AppImage is the project's build from its release page, put in `~/Applications` (or `~/AppImages` if that's the one you have).  It also gets an entry in your applications menu, like Gear Lever makes, and Uninstall removes it. Dolphin publishes no AppImage of its own, so its AppImage is a community build (pkgforge-dev's), tagged **Unofficial**, and the Flatpak is the recommended one. Ryubing has no download source left for its AppImage, so only its Flatpak is offered. Without Flatpak on your system only the AppImage is offered. If you already have the emulator somewhere else, choose its program under "Already have it somewhere else?".

- **AppImage** (recommended, except where it's unofficial): the project's newest build, checked against the checksum the release page gives. It can read your ROM folders wherever they are.
- **Flatpak**: updated by your system (its software centre, or `flatpak update`), installed for your user only. A Flatpak may need access to ROM folders outside your home; Lattis warns you when it can't read one.

While it installs, the row shows a small progress bar and the step, and the open card shows the step in full with a bigger bar and **Cancel**, which stops it and deletes what was downloaded. You can close Settings meanwhile: the install carries on. If it fails, the row says "Install failed" and the card shows the reason with **Try again** and **Dismiss**; the install tiles stay below it, so you can try the other route.

An emulator Lattis installed shows its route and version in the row, and its card says where it came from and when. When the Emulators page opens, Lattis checks whether a newer release is out (at most once an hour per emulator); the row then reads "Update" with the version, and **Update to** that version, in the card, downloads it and moves the old AppImage to the trash once the new one is in place. **Check for updates**, also in the card, looks again right away. Flatpaks aren't checked: your system updates them. **Install the Flatpak instead** (or the AppImage) at the bottom of the card switches route.

**Uninstall**, at the bottom of the card, moves an AppImage to the trash or removes a Flatpak. Your saves and keys stay. Close a game running in that emulator before updating or uninstalling it. Emulators you installed yourself have no Uninstall; remove them the way you installed them.

If the `flatpak` command isn't on your system, only the AppImage is offered.

The setup guide's Emulators step offers the same install. A platform whose emulator is missing says "Needs <emulator>"; click it to open the same tiles, progress and error boxes as here. You can go on to the next step while an install runs.

### A platform's emulators

On Settings → Platforms, **Add emulator** gives a platform an emulator. A platform can have several, in order of preference: a game launches in the first one that's installed. So you can list an AppImage first and a Flatpak second, and the Flatpak is used only when the AppImage is gone. One emulator can serve several platforms (Dolphin runs both GameCube and Wii).

A single game can be pinned to another emulator in its Properties. Saves stay in each emulator's own folders, so moving a game you've played to another emulator doesn't bring its saves along; Lattis warns you when you do this.

### Adding an emulator Lattis doesn't know

On Settings → Emulators, **Add emulator** asks for a name and what to start from:

- **Blank**, for an emulator unlike the built-in ones. You fill in the rest on its row.
- **A built-in emulator**, for a fork of it, like Citra from Azahar. The copy takes its platforms, ROM extensions, command and arguments, but not where it's installed (it would find the original) or where it keeps saves and keys (a fork uses its own folders).

The new emulator is added last to each of its platforms' emulators, so it's used only when the ones before it aren't installed. Change that order on Settings → Platforms.

Its row, under **Yours**, sets:

- **Name.** The emulator's id is made from the name when you add it and doesn't change after; the name can.
- **Platforms** it runs games for. Adding a platform here doesn't add the emulator to that platform's list; Settings → Platforms does.
- **ROM extensions**: the files in ROM folders that are its games, like `3ds cci cxi`.
- **Program**, with **Browse**, or its **Flatpak app id**, like `org.citra_emu.citra`.
- **Command**: how it's started. `{exe}` is the program, `{args}` the arguments, `{rom}` the game's file. Most emulators take `{exe} {args} {rom}`, the default.
- **Arguments**, as for the built-in ones.

**Remove emulator** takes it off every platform. Games pinned to it use their platform's emulators instead, and get their pin back if you add the emulator again under the same name.

**Saves and keys** can't be set in the app. Until they are, a backup skips the emulator's saves (saves kept next to the ROM excepted) and says so. Set them in `library/emulators.toml` (**Open emulators.toml** on its row): `emulators.reference.toml`, next to it, shows every key, with the built-in emulators as examples.

The file is also another way to add an emulator. A minimal one:

```toml
[[emulator]]
id = "citra"
name = "Citra"
platforms = ["3ds"]
path = "~/Applications/citra-qt.AppImage"
command = ["{exe}", "{args}", "{rom}"]
extensions = ["3ds", "cci", "cxi", "3dsx"]
```

`{exe}` is the program, `{args}` the arguments, `{rom}` the game's file. Save the file and the emulator shows up on the Emulators page.

## ROM folders

On Settings → Platforms, **Add folder** under a platform adds a ROM folder. Its games, subfolders included, are found right away and at every start. **Scan now** (or "Scan ROM folders now" in the app menu) looks again whenever you've added files.

- Only file names and sizes are read, never the games themselves, so scanning is quick even for large folders and network drives.
- **Titles come from the file names**, cleaned up: `Mario Kart 8 Deluxe [0100152000022000][v0].nsp` becomes "Mario Kart 8 Deluxe", and `MONSTER HUNTER RISE` becomes "Monster Hunter Rise". Fix a title in the game's Properties, or look the game up (see [Game info and art](game-info.md)).
- **Switch updates and DLC aren't games**, and neither are PS4 update folders; they're skipped. Install them in the emulator.
- **Renamed or moved files keep their game**, with its favorites, tags and play time, when Lattis can tell it's the same file. When it can't, the file shows up as a new game; open the old game's Properties and use **Locate** to point it at the file, which merges the two.
- **A missing file** leaves its game in the library, dimmed, with "ROM not found" in place of Launch. Put the file back and the game is as it was. An unplugged drive works the same way.
- **To stop scanning a folder**, remove it from the platform. Its games stay in the library.

### Finding all your folders at once

If your ROMs are sorted into one folder per platform, the setup guide's **Find from one folder** matches the subfolders to platforms by name (`switch`, `3ds`, `gamecube`, `ps2`…) and adds them all. It lists the folders it couldn't match, to add by hand.

## Removing a game

Deleting a ROM's file doesn't remove its game: it stays as "ROM not found", so a drive that isn't plugged in doesn't empty your library. To remove a game, hide it (see [Adding and editing games](adding-games.md#hiding-and-removing-games)).
