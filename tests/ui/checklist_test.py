import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN_COLL = "key Ctrl+Shift+C\nwait 600\nexpect COLLECTIONS\n"
# An empty collection with the rules off: its one list is the Games tab, the library searched from scratch.
EDIT = OPEN_COLL + "click Nothing yet\nwait 500\nkey Menu\nwait 400\nclick Edit\nwait 600\nexpect Edit collection\n"
SEARCH = "click Search your library\ntype mario\nwait 300\n"
MARIO = "expect Mario Kart 8 Deluxe\nexpect Mario Tennis Aces\nexpect Super Mario Bros. Wonder\n"

COLLECTIONS = """[[collection]]
id = "nothing"
name = "Nothing yet"
"""


def with_empty(dirs):
    dirs.use_demo()
    dirs.write("library/collections.toml", COLLECTIONS)


def with_managers(dirs):
    dirs.use_demo()
    dirs.write("library/mod-managers/mo2.toml",
               'name = "Mod Organizer 2"\nexe = "/usr/bin/true"\ngames = ["linux-supertuxkart"]\n')


# The Edit mod manager dialog lists the Steam, Proton and Linux games; "Final" matches two of them.
MOD_MANAGER = (FIRST + "key Ctrl+Comma\nwait 800\nexpect General\nclick Mod managers\nwait 500\nclick Edit\nwait 600\n"
               "expect Games it mods\nexpect-has 1 selected\n")
FINAL = "click Search games\ntype final\nwait 300\n"

SCENARIOS = [
    Scenario("Edit collection's search counts what it shows and what is ticked; Select all ticks the results only",
             [(FIRST + EDIT + "expect 0 games · 0 selected\nexpect-not Select all\n" + SEARCH + MARIO
               + "expect 3 shown · 0 selected\nexpect Select all\nexpect 0 of 3 selected\n"
               + "click Select all\nwait 300\nexpect 3 shown · 3 selected\nexpect 3 of 3 selected\n"
               + "click mario\nkey Ctrl+A\nkey Delete\nwait 300\n"
               + "expect Mario Kart 8 Deluxe\nexpect 3 games · 3 selected\nexpect-not Select all\n"
               + "type tennis\nwait 300\nexpect 1 shown · 3 selected\nexpect 1 of 1 selected\nclick Select all\nwait 300\n"
               + "expect 1 shown · 2 selected\nexpect 0 of 1 selected\nclick Save\nwait 600\n", None),
              ("expect Leave the collection\n" + OPEN_COLL + "click Nothing yet\nwait 500\nexpect 2 games\n"
               + "expect Mario Kart 8 Deluxe\nexpect Super Mario Bros. Wonder\nexpect-not Mario Tennis Aces\n", None)],
             with_empty),
    Scenario("Shift-click ticks the range from the last clicked row to the state the clicked row takes",
             [(FIRST + EDIT + SEARCH + MARIO
               + "click Mario Kart 8 Deluxe\nshift-click Super Mario Bros. Wonder\nwait 300\nexpect 3 shown · 3 selected\n"
               + "click Mario Kart 8 Deluxe\nexpect 3 shown · 2 selected\n"
               + "shift-click Super Mario Bros. Wonder\nwait 300\nexpect 3 shown · 0 selected\n"
               + "shift-click Mario Tennis Aces\nwait 300\nexpect 3 shown · 2 selected\nclick Cancel\nwait 400\n", None)],
             with_empty),
    Scenario("The Selected button shows only the ticked rows, and a row stays until the filter changes",
             [(FIRST + EDIT + SEARCH + "click Mario Tennis Aces\nclick Selected\nwait 300\n"
               + "expect 1 of 3 shown · 1 selected\nexpect Mario Tennis Aces\nexpect-not Mario Kart 8 Deluxe\n"
               + "click Mario Tennis Aces\nwait 300\nexpect Mario Tennis Aces\nexpect 1 of 3 shown · 0 selected\n"
               + "click Selected\nwait 300\n" + MARIO + "expect 3 shown · 0 selected\n"
               + "click mario\ntype zzz\nwait 300\nexpect-has No games match\nexpect 0 shown · 0 selected\nclick Cancel\nwait 400\n", None)],
             with_empty),
    Scenario("Keys in the list: Down and Space tick a row, Shift+Down extends, Ctrl+A is Select all, Ctrl+F reaches the search",
             [(FIRST + EDIT + SEARCH + "key Tab\nkey Tab\nwait 200\n"
               + "key Down\nkey Space\nwait 300\nexpect 3 shown · 1 selected\n"
               + "key Shift+Down\nwait 300\nexpect 3 shown · 2 selected\n"
               + "key Ctrl+A\nwait 300\nexpect 3 shown · 3 selected\nkey Ctrl+A\nwait 300\nexpect 3 shown · 0 selected\n"
               + "key Ctrl+F\nwait 200\nkey Ctrl+A\ntype tennis\nwait 300\nexpect 1 shown · 0 selected\n"
               + "click Cancel\nwait 400\n", None)],
             with_empty),
    Scenario("Edit mod manager: the count line, shift-click, Select all over a search and the Selected filter",
             [(MOD_MANAGER + FINAL
               + "expect 2 of 12 shown · 1 selected\nexpect Final Fantasy XIV Online\n"
               + "click Final Fantasy VII Remake Intergrade\nshift-click Final Fantasy XIV Online\nwait 300\nexpect 2 of 12 shown · 3 selected\n"
               + "click Select all\nwait 300\nexpect 2 of 12 shown · 1 selected\nclick Select all\nwait 300\nexpect 2 of 12 shown · 3 selected\n"
               + "click Selected\nwait 300\nexpect 2 of 12 shown · 3 selected\n"
               + "click final\nkey Ctrl+A\nkey Delete\nwait 300\nexpect 3 of 12 shown · 3 selected\nexpect SuperTuxKart\n"
               + "click Cancel\nwait 400\n", None)],
             with_managers),
]

sys.exit(run_scenarios(SCENARIOS))
