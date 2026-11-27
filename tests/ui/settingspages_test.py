import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The Settings pages that settings_test only opens: each scenario uses one and checks the file it writes,
# and where it changes the library, what the main window does with it.

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN = FIRST + "key Ctrl+Comma\nwait 800\nexpect General\n"
CLOSE = "key Ctrl+W\nwait 400\nexpect-not General\n"
KEYBOARD = OPEN + "click Keyboard\nwait 600\nclick Search shortcuts\ntype favorite\nwait 600\n"
LIBRARY = OPEN + "click-first Library\nwait 600\n"
MARIO_KART = "switch-mario-kart-8-deluxe.toml"
# The page is taller than the window: a search brings a tag's row, and its tooltip-only buttons, to the top.
SEARCH_PARTY = "click Search tags, hidden games and problems\ntype Party\nwait 600\n"


def demo(dirs):
    dirs.use_demo()


def read(dirs, *parts):
    path = os.path.join(dirs.data, *parts)
    if not os.path.exists(path):
        return ""
    with open(path) as f:
        return f.read()


def has(where, *present, absent=()):
    """where: the path under the data dir."""
    def check(result, dirs):
        text = read(dirs, *where.split("/"))
        return ([f"{where} lacks: {line}" for line in present if line not in text]
                + [f"{where} still has: {line}" for line in absent if line in text])
    return check


def both(*checks):
    def check(result, dirs):
        return [problem for c in checks for problem in (c(result, dirs) or [])]
    return check


def exists(where, wanted=True):
    def check(result, dirs):
        if os.path.exists(os.path.join(dirs.data, *where.split("/"))) != wanted:
            return [f"{where} " + ("isn't there" if wanted else "is still there")]
    return check


def trashed(name):
    """The scenario's own trash (XDG_DATA_HOME is a scratch folder), never the user's."""
    def check(result, dirs):
        if not os.path.exists(os.path.join(dirs.root, "share", "Trash", "files", name)):
            return [f"{name} isn't in the trash"]
    return check


def own_theme(dirs):
    dirs.use_demo()
    dirs.write("themes/Mine.toml", 'colors = "Ember"\nstyle = "Soft"\n')


MOD_MANAGERS = OPEN + "click Mod managers\nwait 600\n"


def one_manager(dirs):
    dirs.use_demo()
    dirs.write("library/mod-managers/mo2.toml",
               'name = "Mod Organizer 2"\nexe = "/usr/bin/true"\ngames = ["linux-supertuxkart"]\n')


def two_own_themes(dirs):
    own_theme(dirs)
    dirs.write("themes/Yours.toml", 'colors = "Nord"\nstyle = "Soft"\n')


APPEARANCE = OPEN + "click Appearance\nwait 600\n"


def hidden_kirby(dirs):
    dirs.use_demo()
    path = os.path.join(dirs.data, "library", "games", "switch-kirby-and-the-forgotten-land.toml")
    with open(path, "a") as f:
        f.write("\nhidden = true\n")


PLATFORMS = OPEN + "click Platforms\nwait 600\nexpect Search platforms\n"
# A new emulator's row opens on its own.
ADD_EMULATOR = (OPEN + "click Emulators\nwait 600\nclick-first Add emulator\nwait 500\ntype My Emu\nwait 300\n"
                "click Add\nwait 800\n")


def ids_in(dirs):
    return re.findall(r'^id = "([^"]+)"', read(dirs, "library", "platforms.toml"), re.M)


def platform_ids(*ids):
    """platforms.toml has exactly these platforms, in this order."""
    def check(result, dirs):
        found = ids_in(dirs)
        return [] if found == list(ids) else [f"platforms.toml has {found}, not {list(ids)}"]
    return check


def linux_moved_up(result, dirs):
    found = ids_in(dirs)
    return [] if sorted(found) == sorted(DEMO_PLATFORMS) and found[-1] != "linux" else [
        f"platforms.toml has {found}: Linux didn't move up"]


DEMO_PLATFORMS = ["switch", "3ds", "steam", "proton", "linux"]


SCENARIOS = [
    Scenario("Keyboard: a recorded shortcut is saved and works in the main window, the old one doesn't",
             [(KEYBOARD + "expect Add to / remove from favorites\nclick Ctrl+D\nwait 300\nexpect Press keys…\n"
               + "expect Press the new keys; Escape cancels\nkey Ctrl+Alt+F\nwait 600\nexpect Ctrl+Alt+F\n"
               + "expect-not Ctrl+D\n" + CLOSE,
               has("settings.toml", 'toggle_favorite = "Ctrl+Alt+F"')),
              (FIRST + "click Pikmin 4\nkey Ctrl+D\nwait 400\nclick Mario Kart 8 Deluxe\nkey Ctrl+Alt+F\nwait 600\n",
               both(has(f"library/games/{MARIO_KART}", "favorite = true"),
                    has("library/games/switch-pikmin-4.toml", absent=("favorite = true",)))),
              (KEYBOARD + "expect Ctrl+Alt+F\n", None)], demo),
    Scenario("Keyboard: the key after a recorded one isn't recorded too",
             [(KEYBOARD + "click Ctrl+D\nwait 300\nkey Ctrl+Alt+F\nwait 600\nexpect Ctrl+Alt+F\nkey Ctrl+W\nwait 600\n",
               has("settings.toml", 'toggle_favorite = "Ctrl+Alt+F"'))], demo),
    Scenario("Keyboard: Escape while recording keeps the shortcut",
             [(KEYBOARD + "click Ctrl+D\nwait 300\nexpect Press keys…\nkey Escape\nwait 400\nexpect-not Press keys…\n"
               + "expect Ctrl+D\nkey Ctrl+Alt+F\nwait 400\nexpect Ctrl+D\n",
               has("settings.toml", absent=("toggle_favorite",)))], demo),
    Scenario("Keyboard: keys another action uses are marked as a conflict",
             [(KEYBOARD + "click Ctrl+D\nwait 300\nkey Ctrl+G\nwait 600\nexpect Another action uses the same keys\n"
               + "click Reset to Ctrl+D\nwait 600\nexpect-not Another action uses the same keys\nexpect Ctrl+D\n",
               has("settings.toml", absent=("toggle_favorite",)))], demo),
    Scenario("Keyboard: Reset puts the default back and takes the line out of settings.toml",
             [(KEYBOARD + "click Ctrl+D\nwait 300\nkey Ctrl+Alt+F\nwait 600\nexpect Ctrl+Alt+F\n",
               has("settings.toml", 'toggle_favorite = "Ctrl+Alt+F"')),
              (KEYBOARD + "expect Ctrl+Alt+F\nclick Reset to Ctrl+D\nwait 600\nexpect Ctrl+D\nexpect-not Ctrl+Alt+F\n",
               has("settings.toml", absent=("toggle_favorite",)))], demo),
    Scenario("Library: a new tag is added to tags.toml and offered in a game's Properties",
             [(LIBRARY + "click New tag\ntype Speedrun\nclick Add tag\nwait 600\nexpect Speedrun\nexpect 0 games\n",
               has("library/tags.toml", 'name = "Speedrun"')),
              (FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\nclick Add tag\nwait 400\n"
               + "type Speed\nwait 300\nexpect Speedrun\n", None)], demo),
    Scenario("Library: renaming a tag renames it in tags.toml and on its games",
             [(LIBRARY + SEARCH_PARTY + "click Rename Party\nwait 300\ntype Couch\nkey Return\nwait 600\n"
               + "expect Nothing matches \"Party\".\nclick Party\nkey Ctrl+A\ntype Couch\nwait 600\nexpect 3 games\n",
               both(has("library/tags.toml", 'name = "Couch"', absent=('name = "Party"',)),
                    has(f"library/games/{MARIO_KART}", '"Couch"', absent=('"Party"',))))], demo),
    Scenario("Library: deleting a tag asks first, Cancel keeps it, Delete takes it off its games",
             [(LIBRARY + SEARCH_PARTY + "click Delete Party\nwait 400\nexpect Delete the tag Party?\n"
               + "expect 3 games lose it. The games themselves stay.\nclick Cancel\nwait 400\nexpect 3 games\n"
               + "click Delete Party\nwait 400\nclick Delete\nwait 600\nexpect-not Delete the tag Party?\n"
               + "expect Nothing matches \"Party\".\n",
               both(has("library/tags.toml", absent=('name = "Party"',)),
                    has(f"library/games/{MARIO_KART}", absent=('"Party"',))))], demo),
    Scenario("Library: Undo after deleting a tag puts it back on its games",
             [(LIBRARY + SEARCH_PARTY + "click Delete Party\nwait 400\nclick Delete\nwait 600\nclick Undo\nwait 800\n"
               + "expect 3 games\n",
               both(has("library/tags.toml", 'name = "Party"'), has(f"library/games/{MARIO_KART}", '"Party"')))],
             demo),
    Scenario("Library: a hidden game is listed under Hidden games, and Unhide brings it back to the list",
             [(LIBRARY + "click Search tags, hidden games and problems\ntype Kirby\nwait 600\n"
               + "expect Kirby and the Forgotten Land\nclick Unhide\nwait 600\nexpect Nothing matches \"Kirby\".\n"
               + CLOSE + "expect Kirby and the Forgotten Land\n",
               has("library/games/switch-kirby-and-the-forgotten-land.toml", absent=("hidden = true",))),
              ("expect Kirby and the Forgotten Land\n", None)], hidden_kirby),
    Scenario("Platforms: Add platform names it, shows its id, and adds it to platforms.toml",
             [(PLATFORMS + "click Add platform\nwait 500\ntype GameCube\nwait 300\n"
               + "expect-has Id: gamecube.\nclick Add\nwait 600\nexpect GameCube\n",
               both(has("library/platforms.toml", 'id = "gamecube"', 'name = "GameCube"'),
                    platform_ids(*DEMO_PLATFORMS, "gamecube")))], demo),
    Scenario("Platforms: a name whose id is taken is refused",
             [(PLATFORMS + "click Add platform\nwait 500\ntype Linux\nwait 300\n"
               + "expect There's already a platform with the id linux.\nclick Cancel\nwait 400\n",
               platform_ids("switch", "3ds", "steam", "proton", "linux"))], demo),
    # The first key after Settings closes doesn't reach the main window offscreen, so the rail is checked
    # after a restart.
    Scenario("Platforms: typing a place moves a platform there, in platforms.toml and in the rail",
             [(PLATFORMS + "click 5\nkey Ctrl+A\ntype 1\nkey Return\nwait 600\n",
               platform_ids("linux", "switch", "3ds", "steam", "proton")),
              (FIRST + "key Ctrl+2\nwait 400\nexpect SuperTuxKart\nexpect-not Mario Kart 8 Deluxe\n", None)], demo),
    Scenario("Platforms: dragging the last platform by its grip moves it up",
             [(PLATFORMS + "drag Drag to change its place in the rail by 0 -150\nwait 800\n", linux_moved_up)], demo),
    # Linux, Steam and Proton come back when removed (the library adds them while it has their games).
    Scenario("Platforms: Remove platform asks first and keeps the games",
             [(PLATFORMS + "click 3DS\nwait 500\nclick Remove platform\nwait 400\nexpect Remove 3DS?\n"
               + "expect-has Its ROM folders won't be scanned.\nclick Remove\nwait 600\nexpect-not Remove 3DS?\n",
               both(platform_ids("switch", "steam", "proton", "linux"),
                    has("library/games/3ds-fire-emblem-awakening.toml", 'platform = "3ds"'))),
              (FIRST + "type Fire Emblem\nwait 400\nclick Show not installed\nwait 400\nexpect Fire Emblem Awakening\n", None)], demo),
    Scenario("Emulators: Add emulator writes a blank one to emulators.toml, a taken id is refused",
             [(ADD_EMULATOR + "expect My Emu\nexpect Not found; choose the program\nclick-first Add emulator\nwait 500\n"
               + "type My Emu\nwait 300\nexpect There's already an emulator with the id my-emu.\nclick Cancel\nwait 400\n",
               has("library/emulators.toml", 'id = "my-emu"', 'name = "My Emu"'))], demo),
    Scenario("Emulators: its extensions, Flatpak app id and arguments are saved",
             [(ADD_EMULATOR + "click e.g. 3ds cci cxi\ntype iso cso\nkey Return\nwait 400\n"
               + "click e.g. org.citra_emu.citra\ntype org.example.MyEmu\nkey Return\nwait 400\n"
               + "click None\ntype --fullscreen\nkey Return\nwait 600\nexpect --fullscreen\n",
               has("library/emulators.toml", 'extensions = ["iso", "cso"]', 'flatpak = "org.example.MyEmu"',
                   '"--fullscreen"'))], demo),
    Scenario("Emulators: Remove emulator asks first and takes it out of emulators.toml",
             [(ADD_EMULATOR + "click Remove emulator\nwait 400\nexpect Remove My Emu?\nclick Remove\nwait 600\n"
               + "expect-not My Emu\n", has("library/emulators.toml", absent=('id = "my-emu"',)))], demo),
    Scenario("Mod managers: Add mod manager writes its file, and the linked game's menu opens it",
             [(MOD_MANAGERS + "click-first Add mod manager\nwait 600\nexpect Add mod manager\n"
               + "click e.g. Mod Organizer 2\ntype Tool\nclick A .exe, a script, an AppImage or a Flatpak app id\n"
               + "type /usr/bin/true\nwait 400\nexpect It runs directly, like a Linux game.\nclick Search games\ntype tux\nwait 300\n"
               + "click SuperTuxKart\n"
               + "wait 300\nclick Add\nwait 800\nexpect Tool\n",
               lambda r, d: [] if any('name = "Tool"' in read(d, "library", "mod-managers", f)
                                      and "linux-supertuxkart" in read(d, "library", "mod-managers", f)
                                      for f in os.listdir(os.path.join(d.data, "library", "mod-managers")))
               else ["no mod manager file names Tool with SuperTuxKart"]),
              (FIRST + "click SuperTuxKart\nkey Menu\nwait 400\nexpect Open Tool\n", None)], demo),
    Scenario("Mod managers: Edit renames one, Remove asks and takes its file out",
             [(MOD_MANAGERS + "click Edit\nwait 600\nexpect Edit mod manager\nclick Mod Organizer 2\nkey Ctrl+A\n"
               + "type MO2\nclick Save\nwait 800\nexpect MO2\n",
               has("library/mod-managers/mo2.toml", 'name = "MO2"')),
              (MOD_MANAGERS + "click Remove\nwait 600\nexpect Remove MO2?\n"
               + "expect It's taken out of the list. Every game's files and prefix stay.\nclick Remove\nwait 800\n"
               + "expect-not MO2\n", exists("library/mod-managers/mo2.toml", False))], one_manager),
    Scenario("Themes: New colors makes a copy of the colors in use and switches to it",
             [(OPEN + "key Ctrl+Shift+N\nwait 800\nexpect New colors\nexpect Based on\nclick Create\nwait 1000\n"
               + "expect-not Based on\n",
               both(exists("themes/colors/My colors.toml"), has("settings.toml", 'colors = "My colors"')))], demo),
    Scenario("Themes: a theme of your own is renamed from its card's menu",
             [(APPEARANCE + "rightclick Mine\nwait 400\nclick Rename\nwait 500\nexpect Rename Mine\nclick Mine\nkey Ctrl+A\n"
               + "type Ours\nwait 300\nclick Rename\nwait 800\nexpect Ours\nexpect-not Mine\n",
               both(exists("themes/Ours.toml"), exists("themes/Mine.toml", False)))], own_theme),
    Scenario("Themes: renaming to a theme's name that's taken is refused",
             [(APPEARANCE + "rightclick Mine\nwait 400\nclick Rename\nwait 500\nclick Mine\nkey Ctrl+A\ntype Nord\nwait 300\n"
               + "expect Nord is built in\nkey Ctrl+A\ntype Yours\nwait 300\nexpect There is already a theme called Yours\n"
               + "click Cancel\nwait 400\n", both(exists("themes/Mine.toml"), exists("themes/Yours.toml")))],
             two_own_themes),
    Scenario("Themes: the Rename dialog opens with no error on the theme's own name",
             [(APPEARANCE + "rightclick Mine\nwait 400\nclick Rename\nwait 500\nexpect Rename Mine\n"
               + "expect-not-has There is already a theme called Mine\n", exists("themes/Mine.toml"))], own_theme,
             known_bugs=[("expect-not-has There is already a theme called Mine didn't happen",
                          "RenameThemeDialog counts the theme's own name as taken")]),
    Scenario("Themes: a theme of your own is deleted from its card's menu, to the trash",
             [(APPEARANCE + "rightclick Mine\nwait 400\nclick Delete\nwait 500\nexpect Delete Mine?\n"
               + "expect-has This moves\nclick Delete\nwait 800\nexpect-not Mine\nexpect-has Moved Mine to the trash\n",
               both(exists("themes/Mine.toml", False), trashed("Mine.toml")))], own_theme),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
