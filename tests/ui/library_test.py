import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
MARIO_ONLY = "expect Mario Kart 8 Deluxe\nexpect Super Mario Bros. Wonder\nexpect-not Pikmin 4\nexpect-not Hades\n"


def demo(dirs):
    dirs.use_demo()


def with_managers(dirs):
    dirs.use_demo()
    dirs.write("library/mod-managers/mo2.toml",
               'name = "Mod Organizer 2"\nexe = "/usr/bin/true"\ngames = ["linux-supertuxkart"]\n')
    dirs.write("library/mod-managers/vortex.toml",
               'name = "Vortex"\nexe = "/usr/bin/true"\ngames = ["linux-supertuxkart"]\n')


def search(text):
    return "type " + text + "\nwait 400\n"


SCENARIOS = [
    Scenario("the demo library lists every platform group",
             [(FIRST + "expect SWITCH\nexpect 3DS\nexpect STEAM\nexpect PROTON\nexpect LINUX\n", None)], demo),
    Scenario("typing searches, two Escapes clear it",
             [(FIRST + search("mario") + MARIO_ONLY + "key Escape\nkey Escape\n" + FIRST, None)], demo),
    Scenario("a search is case-insensitive and matches inside a title",
             [(FIRST + search("KART") + "expect Mario Kart 8 Deluxe\nexpect-not Super Mario Bros. Wonder\n", None)],
             demo),
    Scenario("a search with no match shows no games, and clearing it brings them back",
             [(FIRST + search("zzzzqq") + "expect-not Mario Kart 8 Deluxe\nexpect-not Hades\n"
               + "key Escape\nkey Escape\n" + FIRST, None)], demo),
    Scenario("Backspace edits the search",
             [(FIRST + search("mariox") + "expect-not Mario Kart 8 Deluxe\nkey Backspace\nwait 400\n" + MARIO_ONLY,
               None)], demo),
    Scenario("a platform in the rail filters the list, All games brings the rest back",
             [(FIRST + "key Ctrl+2\nexpect Mario Kart 8 Deluxe\nexpect-not Hades\nexpect-not Fire Emblem Awakening\n"
               + "key Ctrl+3\nexpect Fire Emblem Awakening\nexpect-not Mario Kart 8 Deluxe\n"
               + "key Ctrl+1\n" + FIRST, None)], demo),
    Scenario("a platform filter and a search combine",
             [(FIRST + "key Ctrl+2\n" + search("mario") + MARIO_ONLY + "key Ctrl+3\nexpect-not Mario Kart 8 Deluxe\n"
               + "key Ctrl+1\nwait 400\nexpect Mario Kart 8 Deluxe\nexpect-not Pikmin 4\n", None)], demo),
    Scenario("Ctrl+Shift+S steps through the sort orders and back",
             [(FIRST + "key Ctrl+Shift+S\nexpect Sorted by last played\nwait 3500\n"
               + "key Ctrl+Shift+S\nexpect Sorted by play time\nwait 3500\n"
               + "key Ctrl+Shift+S\nexpect Sorted by title\n" + FIRST, None)], demo),
    Scenario("Down and Up move through the games, Shift+Down and Shift+Up extend the selection",
             [(FIRST + "key Down\nkey Shift+Down\nexpect 2 selected\nkey Shift+Down\nexpect 3 selected\n"
               + "key Shift+Up\nexpect 2 selected\nkey Shift+Up\nexpect-not 1 selected\nexpect-not 2 selected\n"
               + "key Shift+Down\nkey Shift+Down\nexpect 3 selected\nkey Escape\nexpect-not 3 selected\n", None)],
             demo),
    Scenario("Ctrl+A selects every listed game and Escape clears it",
             [(FIRST + "key Ctrl+A\nexpect-has selected\nkey Escape\nexpect-not-has selected\n", None)], demo),
    Scenario("Ctrl+A selects only what the search lists",
             [(FIRST + search("mario") + "key Escape\nkey Ctrl+A\nexpect 2 selected\n", None)], demo),
    Scenario("Home and End jump to the first and last game, a click selects one",
             [(FIRST + "key Ctrl+A\nkey Home\nexpect-not-has selected\nkey End\nclick Pikmin 4\n"
               + "expect-not-has selected\nexpect Pikmin 4\n", None)], demo),
    Scenario("a game's page shows its details when it is selected",
             [(FIRST + "click Mario Kart 8 Deluxe\nexpect-has Kart racing across 48 tracks\nexpect Install Ryubing\n"
               + "click Hades\nexpect-not-has Kart racing across 48 tracks\n", None)], demo),
    Scenario("Ctrl+D favorites a game, Ctrl+Shift+D shows only favorites",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Ctrl+D\nkey Ctrl+Shift+D\nwait 400\n"
               + "expect Mario Kart 8 Deluxe\nexpect-not Pikmin 4\nkey Ctrl+Shift+D\nwait 400\n" + FIRST, None)],
             demo),
    Scenario("a favorite persists across a restart",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Ctrl+D\nwait 600\n", None),
              (FIRST + "key Ctrl+Shift+D\nwait 400\nexpect Mario Kart 8 Deluxe\nexpect-not Pikmin 4\n", None)], demo),
    Scenario("Properties opens, Cancel closes it and keeps the game as it was",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\nexpect Save\nexpect Cancel\n"
               + "click Title\nclick Mario Kart 8 Deluxe\nclick Cancel\nexpect-not Properties\n" + FIRST, None)],
             demo),
    Scenario("a title edited in Properties shows in the list and persists",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\n"
               + "click Mario Kart 8 Deluxe\nkey Ctrl+A\ntype Mario Kart 9 Deluxe\nclick Save\nexpect-not Properties\n"
               + "expect Mario Kart 9 Deluxe\nexpect-not Mario Kart 8 Deluxe\n", None),
              ("expect Mario Kart 9 Deluxe\nexpect-not Mario Kart 8 Deluxe\n", None)], demo),
    Scenario("the description edited in Properties shows on the game's page",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\n"
               + "click Description\nclick Kart racing across 48 tracks, with items, gliders and underwater "
               + "sections. Up to four players on one screen.\nkey Ctrl+A\ntype Only turtle shells\n"
               + "click Save\nexpect-not Properties\nexpect-has Only turtle shells\n", None)], demo),
    Scenario("Properties tabs show one job each, and Save persists edits made on two of them",
             [(FIRST + search("Diablo") + "click Show not installed\nwait 400\nclick Diablo IV\nkey Alt+Return\nexpect Properties\n"
               + "expect General\nexpect Images\nexpect Launch\nexpect Look up in IGDB\nexpect-not Proton version\n"
               + "click Images\nexpect Browse SteamGridDB\nexpect Media folders\nexpect-not Look up in IGDB\n"
               + "click Launch\nexpect Proton version\nexpect-not Media folders\nexpect-not Look up in IGDB\n"
               + "click mangohud %command%\nkey Ctrl+A\ntype mangohud %command% --fullscreen\n"
               + "click General\nexpect Look up in IGDB\nexpect-not Proton version\n"
               + "click Diablo IV\nkey Ctrl+A\ntype Diablo IV Remastered\nclick Save\nexpect-not Properties\n"
               + "expect Diablo IV Remastered\n", None),
              (search("Diablo") + "expect Diablo IV Remastered\nclick Diablo IV Remastered\nkey Alt+Return\n"
               + "expect Properties\nclick Launch\nexpect mangohud %command% --fullscreen\n", None)], demo),
    Scenario("Properties opens on General each time, and Save jumps to the tab of a field in error",
             [(FIRST + search("Diablo") + "click Show not installed\nwait 400\nclick Diablo IV\nkey Alt+Return\nexpect Properties\nclick Launch\n"
               + "expect Proton version\nclick General\nclick Diablo IV\nkey Ctrl+A\nkey Delete\nclick Launch\n"
               + "expect Proton version\nclick Save\nexpect A game needs a title\nexpect Look up in IGDB\nexpect-not Proton version\n"
               + "key Escape\nexpect-not Properties\nkey Alt+Return\nexpect Properties\nexpect Look up in IGDB\n"
               + "expect-not Proton version\n", None)], demo),
    Scenario("the Saves tab shows the save folder for an emulator game, and the Launch tab no longer does",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\nclick Launch\n"
               + "expect-not Prefix folder\nexpect-not Save folder\nclick Saves\nexpect Save folder\n"
               + "expect-not Backed up\nexpect-not Mod managers\nexpect-not Prefix folder\n", None)], demo),
    Scenario("a Linux game's Saves & mods tab has the save folder and the mod managers",
             [(FIRST + "click SuperTuxKart\nkey Alt+Return\nexpect Properties\nclick Saves & mods\n"
               + "expect Save folder\nexpect Mod managers\nexpect Mod Organizer 2\n", None)], with_managers),
    Scenario("Hide game in Properties removes the game from the list",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\nclick Hide game\n"
               + "wait 400\nclick Hide\nwait 600\nexpect-not Mario Kart 8 Deluxe\nexpect Pikmin 4\n", None)], demo),
    Scenario("Escape closes Properties without saving",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\nkey Escape\n"
               + "expect-not Properties\nexpect Mario Kart 8 Deluxe\n", None)], demo),
    Scenario("a linked game's menu opens its mod managers, another game's only offers to add one",
             [(FIRST + "click SuperTuxKart\nkey Menu\nwait 400\nexpect Open Mod Organizer 2\nexpect Open Vortex\n"
               + "expect Add a mod manager\nkey Escape\nwait 400\nclick Hades\nkey Menu\nwait 400\n"
               + "expect-not Open Vortex\nexpect Add a mod manager\n", None)], with_managers),
]

sys.exit(run_scenarios(SCENARIOS))
