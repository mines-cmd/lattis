import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
# Down from nothing lands on the second game of the list; Shift+Down twice makes three.
THREE = "key Down\nkey Shift+Down\nkey Shift+Down\nexpect 3 selected\n"
# The three games the keys above select.
THREE_GAMES = ["Kirby and the Forgotten Land", "Mario Kart 8 Deluxe", "Metroid Dread"]
ONLY_THREE = "".join(f"expect {g}\n" for g in THREE_GAMES) + "expect-not Pikmin 4\nexpect Hades\n"
FAVORITES = "key Ctrl+Shift+D\nwait 400\n"


def demo(dirs):
    dirs.use_demo()


def favorites_of(*games):
    return "".join(f"expect {g}\n" for g in games) + "expect-not Pikmin 4\nexpect Hades\n"


SCENARIOS = [
    Scenario("favoriting a selection marks every game and keeps the selection",
             [(FIRST + THREE + "key Ctrl+D\nexpect 3 games added to favorites\nexpect 3 selected\n"
               + FAVORITES + ONLY_THREE, None)], demo),
    Scenario("Ctrl+Z undoes a bulk favorite",
             [(FIRST + THREE + "key Ctrl+D\nexpect 3 games added to favorites\nkey Ctrl+Z\n"
               + "expect-not 3 games added to favorites\nexpect 3 selected\n" + FAVORITES
               + "expect-not Mario Kart 8 Deluxe\nexpect-not Metroid Dread\n", None)], demo),
    Scenario("the Undo link in the message undoes a bulk favorite",
             [(FIRST + THREE + "key Ctrl+D\nclick Undo\n" + FAVORITES
               + "expect-not Mario Kart 8 Deluxe\nexpect-not Metroid Dread\n", None)], demo),
    Scenario("a bulk favorite persists across a restart, an undone one does not",
             [(FIRST + THREE + "key Ctrl+D\nwait 600\n", None),
              (FIRST + FAVORITES + ONLY_THREE + "key Ctrl+Shift+D\nwait 400\nkey Escape\nkey Down\n"
               + "key Shift+Down\nkey Ctrl+D\nkey Ctrl+Z\nwait 600\n", None),
              (FIRST + FAVORITES + ONLY_THREE, None)], demo),
    Scenario("a second Ctrl+D on the same selection removes the favorites again",
             [(FIRST + THREE + "key Ctrl+D\nexpect 3 games added to favorites\nwait 400\nkey Ctrl+D\n"
               + "expect-has removed from favorites\nexpect 3 selected\n", None)], demo),
    Scenario("Ctrl+A then favorite then undo covers the whole library",
             [(FIRST + "key Ctrl+A\nkey Ctrl+D\nexpect-has added to favorites\nkey Ctrl+Z\n"
               + "expect-not-has added to favorites\nexpect-has selected\n" + FAVORITES
               + "expect-not Mario Kart 8 Deluxe\nexpect Hades\n", None)], demo),
    Scenario("a selection made inside a search only touches what the search lists",
             [(FIRST + "type mario\nwait 400\nkey Escape\nkey Ctrl+A\nkey Ctrl+D\nexpect 2 games added to favorites\n"
               + "key Escape\nkey Escape\nwait 400\n" + FAVORITES
               + favorites_of("Mario Kart 8 Deluxe", "Super Mario Bros. Wonder"), None)], demo),
    Scenario("hiding a selection from the game menu removes it from the list, undo brings it back in place",
             [(FIRST + THREE + "key Menu\nwait 500\nclick Hide\nwait 600\n"
               + "expect-not Kirby and the Forgotten Land\nexpect-not Mario Kart 8 Deluxe\nexpect-not Metroid Dread\n"
               + "expect Pikmin 4\nexpect Hades\nkey Ctrl+Z\nwait 600\n" + FIRST
               + "expect Kirby and the Forgotten Land\nexpect Metroid Dread\n", None)], demo),
    Scenario("a hidden selection stays hidden after a restart",
             [(FIRST + THREE + "key Menu\nwait 500\nclick Hide\nwait 600\n", None),
              ("expect Pikmin 4\nexpect Hades\nexpect-not Mario Kart 8 Deluxe\nexpect-not Metroid Dread\n", None)],
             demo),
    Scenario("tagging a selection from the game menu, then undoing it",
             [(FIRST + THREE + "key Menu\nwait 500\nclick Tags\nwait 400\nclick Platformer\nwait 600\n"
               + "expect-has Tag Platformer added to 3 game\nkey Escape\nkey Escape\nwait 400\nkey Ctrl+Z\nwait 600\nexpect-not-has Tag Platformer added to 3 game\nexpect 3 selected\n", None)], demo),
    Scenario("Properties Save can be undone",
             [(FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\n"
               + "click Mario Kart 8 Deluxe\nkey Ctrl+A\ntype Mario Kart 9 Deluxe\nclick Save\n"
               + "expect Mario Kart 9 Deluxe\nkey Ctrl+Z\nwait 600\nexpect Mario Kart 8 Deluxe\n"
               + "expect-not Mario Kart 9 Deluxe\n", None)], demo),
    Scenario("undoing a selection's change leaves the list sorted and every game listed once",
             [(FIRST + "key Ctrl+Shift+S\nwait 3500\nkey Ctrl+A\nkey Ctrl+D\nkey Ctrl+Z\nwait 600\n"
               + "key Ctrl+Shift+S\nwait 3500\nkey Ctrl+Shift+S\nwait 3500\n" + FIRST, None)], demo),
    Scenario("Escape clears the selection, then a single game can be favorited alone",
             [(FIRST + THREE + "key Escape\nexpect-not 3 selected\nkey Ctrl+D\nexpect-not 3 games\n"
               + FAVORITES + "expect Metroid Dread\nexpect-not Kirby and the Forgotten Land\nexpect-not Mario Kart 8 Deluxe\n", None)], demo),
]

sys.exit(run_scenarios(SCENARIOS))
