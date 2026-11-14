import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN_COLL = "key Ctrl+Shift+C\nwait 600\nexpect COLLECTIONS\n"
OPEN_FILTERS = "key Ctrl+Shift+F\nwait 600\nexpect SORT\n"
# Mario Tennis Aces has no ROM in the demo, so Installed hides it.
MIX = "expect 3 games\nexpect Hades\nexpect Mario Tennis Aces\nexpect Pikmin 4\nexpect-not Mario Kart 8 Deluxe\n"
LEAVE = "click Leave the collection\nwait 500\nexpect-not 3 games\n"

COLLECTIONS = """[[collection]]
id = "mix"
name = "Mix"
added = ["steam-1145360", "switch-mario-tennis-aces", "switch-pikmin-4"]

[[collection]]
id = "nothing"
name = "Nothing yet"
"""
# Down from nothing lands on the second game of the list; Shift+Down twice makes three.
THREE = "key Down\nkey Shift+Down\nkey Shift+Down\nexpect 3 selected\n"
MENU = "key Menu\nwait 500\nclick Collections\nwait 400\n"

# Rules: favorites, installed or not. Hades is a favorite but left out; Pikmin 4 isn't one but was added.
RULED = """[[collection]]
id = "faves"
name = "Faves"
rules = true
favorites_only = true
added = ["switch-pikmin-4"]
removed = ["steam-1145360"]
"""
EDIT = "click Faves\nwait 500\nkey Menu\nwait 400\nclick Edit\nwait 600\nexpect Edit collection\n"


def with_collections(dirs):
    dirs.use_demo()
    dirs.write("library/collections.toml", COLLECTIONS)


SCENARIOS = [
    Scenario("a collection in the filter panel opens with its games, uninstalled ones too, and a banner",
             [(FIRST + OPEN_COLL + "expect Mix\nexpect Nothing yet\nclick Mix\nwait 500\n" + MIX
               + "expect Leave the collection\n", None)], with_collections),
    Scenario("filters narrow within a collection and Clear filters returns to its full list",
             [(FIRST + OPEN_COLL + "click Mix\nwait 500\n" + MIX
               + OPEN_FILTERS
               + "click Installed\nwait 400\nexpect Hades\nexpect-not Mario Tennis Aces\nexpect Installed only\n"
               + "click Clear filters\nwait 400\n" + MIX + "expect-not Installed only\n"
               + "expect Leave the collection\n", None)], with_collections),
    Scenario("leaving brings back the filters and the platform from before",
             [(FIRST + "key Ctrl+Shift+D\nwait 400\nexpect Hades\nexpect-not Pikmin 4\n"
               + OPEN_COLL + "click Mix\nwait 500\n" + MIX + LEAVE
               + "expect Hades\nexpect-not Pikmin 4\nexpect-not Mario Tennis Aces\n"
               + "key Ctrl+Shift+D\nwait 400\n" + FIRST, None)], with_collections),
    Scenario("a collection with no games says so and stays in the panel",
             [(FIRST + OPEN_COLL + "click Nothing yet\nwait 500\nexpect This collection has no games\n"
               + "expect 0 games\nexpect-not Hades\n"
               + "click Leave the collection\nwait 500\nexpect-not This collection has no games\n" + FIRST
               + OPEN_COLL + "expect Nothing yet\n", None)], with_collections),
    Scenario("an open collection is still open after a restart",
             [(FIRST + OPEN_COLL + "click Mix\nwait 500\n" + MIX, None),
              ("expect 3 games\nexpect Hades\nexpect-not Mario Kart 8 Deluxe\n" + LEAVE + FIRST, None)],
             with_collections),
    Scenario("a number on the overview leaves the open collection",
             [(FIRST + OPEN_COLL + "click Mix\nwait 500\n" + MIX
               + "key F7\nexpect Library overview\nexpect Needs attention\nwait 1200\n"
               + "key PageDown\nwait 300\nclick Proton\nwait 500\nexpect-not Library overview\n"
               + "expect Final Fantasy XIV Online\nexpect-not 3 games\n", None)], with_collections),
    Scenario("the Tags section starts open, folds on a click and stays as left; Collections isn't in the panel",
             [(FIRST + OPEN_FILTERS + "expect TAGS\nexpect Racing\nexpect-not COLLECTIONS\nclick TAGS\nwait 400\n"
               + "expect-not Racing\n", None),
              (FIRST + OPEN_FILTERS + "expect-not Racing\nclick TAGS\nwait 400\nexpect Racing\n", None)],
             with_collections),
    Scenario("the tag search narrows the tags, ignores case and accents, and keeps selected tags; Escape clears it",
             [(FIRST + OPEN_FILTERS + "click Racing\nwait 400\nclick Search tags (10)\ntype pLATFÖRMER\nwait 400\n"
               + "expect Platformer\nexpect Racing\nexpect-not Roguelike\nexpect-not Metroidvania\n"
               + "type zzz\nwait 300\nexpect-not Platformer\nexpect Racing\n"
               + "key Escape\nwait 400\nexpect Roguelike\nexpect Metroidvania\nexpect TAGS (1)\n"
               + "key Escape\nwait 600\nexpect-not TAGS (1)\n", None),
              (OPEN_FILTERS + "expect Roguelike\nexpect Metroidvania\n", None)],
             with_collections),
    Scenario("a selection goes into a collection from the game menu, and Undo takes it out",
             [(FIRST + THREE + MENU + "click Nothing yet\nwait 600\nexpect-has 3 games added to Nothing yet\n"
               + "key Escape\nkey Escape\nwait 400\n" + OPEN_COLL + "click Nothing yet\nwait 500\n"
               + "expect 3 games\nexpect Metroid Dread\nkey Ctrl+Z\nwait 600\nexpect This collection has no games\n",
               None)], with_collections),
    Scenario("New collection in the game menu names a collection for the selection",
             [(FIRST + THREE + MENU + "click New collection\nwait 400\nexpect New collection for 3 games\n"
               + "type Switch picks\nkey Return\nwait 600\nexpect-has Collection Switch picks made with 3 games\n"
               + OPEN_COLL + "expect Switch picks\nclick Switch picks\nwait 500\n"
               + "expect 3 games\nexpect Metroid Dread\nexpect-not Hades\n", None),
              ("expect 3 games\n" + LEAVE + FIRST + OPEN_COLL + "expect Switch picks\n", None)], with_collections),
    Scenario("unchecking the open collection in the game menu takes the game out of it only",
             [(FIRST + OPEN_COLL + "click Mix\nwait 500\n" + MIX + "key Escape\nclick Hades\n" + MENU
               + "click Mix\nwait 600\nexpect-has 1 game removed from Mix\nkey Escape\nkey Escape\nwait 400\n"
               + "expect 2 games\n"
               + "expect-not Hades\nexpect Pikmin 4\nkey Escape\nkey Ctrl+Z\nwait 600\nexpect 3 games\n"
               + "expect Hades\n", None)], with_collections),
    Scenario("New collection in the filter panel makes an empty one that stays",
             [(FIRST + OPEN_COLL + "click New collection\nwait 400\nexpect Name of the collection\n"
               + "type Fresh\nkey Return\nwait 600\n" + OPEN_COLL + "expect Fresh\n", None),
              (FIRST + OPEN_COLL + "expect Fresh\n", None)], with_collections),
    Scenario("a name already used is refused",
             [(FIRST + OPEN_COLL + "click New collection\nwait 400\ntype mix\nkey Return\nwait 600\n"
               + "expect-has There is already a collection called Mix\n", None)], with_collections),
    Scenario("a collection's menu renames or deletes it, and Delete leaves the games",
             [(FIRST + OPEN_COLL + "click Mix\nwait 500\nkey Menu\nwait 400\nclick Rename\nwait 500\n"
               + "expect Rename collection\nkey Ctrl+A\ntype Weekend\nkey Return\nwait 600\n"
               + "expect-has Collection renamed to Weekend\n" + OPEN_COLL + "expect Weekend\nexpect-not Mix\n"
               + "click Nothing yet\nwait 500\nkey Menu\nwait 400\n"
               + "click Delete\nwait 600\nexpect-has Collection Nothing yet deleted\n"
               + "expect Weekend\nexpect-not Nothing yet\n"
               + "click Weekend\nwait 500\nexpect 3 games\nexpect Hades\n", None)], with_collections),
]

def with_ruled(dirs):
    dirs.use_demo()
    dirs.write("library/collections.toml", COLLECTIONS + RULED)


SCENARIOS += [
    Scenario("Save as collection copies the filters, and the collection opens as a fixed scope",
             [(FIRST + OPEN_FILTERS + "expect-not Save as collection\nclick Favorites\nwait 400\n"
               + "expect Save as collection\nclick Save as collection\nwait 500\n"
               + "expect Save the filters as a collection\ntype Faves now\nkey Return\nwait 600\n"
               + "expect-has Collection Faves now saved from the filters\n"
               + OPEN_COLL + "expect Faves now\nclick Faves now\nwait 500\n"
               + "expect 1 game\nexpect Hades\nexpect-not Pikmin 4\nexpect-not Installed only\n"
               + "expect-not Save as collection\n", None),
              ("expect 1 game\nexpect Hades\n" + LEAVE + "expect Hades\nexpect-not Pikmin 4\n"
               + OPEN_COLL + "expect Faves now\n", None)], with_collections),
    Scenario("Edit collection lists the exceptions, drops one, and Undo puts it back",
             [(FIRST + OPEN_COLL + EDIT + "expect Add games by rules\nexpect Added by hand\n"
               + "expect Left out\nexpect-not Hades\nclick Left out\nwait 300\nexpect Hades\nclick Hades\nclick Save\nwait 600\n"
               + "expect-has Collection Faves changed\n" + OPEN_COLL + "click Faves\nwait 500\n"
               + "expect Hades\nexpect Mario Tennis Aces\nexpect Pikmin 4\nexpect 3 games\n"
               + "key Ctrl+Z\nwait 600\nexpect-not Hades\nexpect 2 games\n", None)], with_ruled),
    Scenario("Edit collection can turn the rules off, leaving the games added by hand",
             [(FIRST + OPEN_COLL + EDIT + "toggle Add games by rules\nwait 300\nexpect-not Left out\n"
               + "click Save\nwait 600\n" + OPEN_COLL + "click Faves\nwait 500\n"
               + "expect 1 game\nexpect Pikmin 4\nexpect-not Mario Tennis Aces\n", None)], with_ruled),
    Scenario("Cancel in Edit collection changes nothing",
             [(FIRST + OPEN_COLL + EDIT + "click Left out\nwait 300\nclick Hades\nclick Cancel\nwait 500\n" + OPEN_COLL
               + "click Faves\nwait 500\nexpect 2 games\nexpect-not Hades\n", None)], with_ruled),
]



def with_rated(dirs):
    dirs.use_demo()
    dirs.write("library/collections.toml", COLLECTIONS + RULED.replace("favorites_only = true", "min_rating = 2.0"))


SCENARIOS += [
    Scenario("Edit collection: chips toggle, the count follows them and Save keeps them",
             [(FIRST + OPEN_COLL + EDIT + "expect Add games by rules\nexpect Library\nexpect Progress\nexpect Details\n"
               + "expect-has <b>2</b></font> games match\nclick Installed\nwait 400\nexpect-has <b>1</b></font> game matches\n"
               + "click Installed\nwait 400\nexpect-has <b>2</b></font> games match\nclick Installed\nwait 400\nexpect-has <b>1</b></font> game matches\n"
               + "click Save\nwait 600\nexpect-has Collection Faves changed\n" + OPEN_COLL + "click Faves\nwait 500\n"
               + "expect 1 game\nexpect Pikmin 4\nexpect-not Mario Tennis Aces\n", None),
              ("expect Leave the collection\n" + OPEN_COLL + "expect Faves\nkey Escape\nwait 400\n" + EDIT
               + "expect-has <b>1</b></font> game matches\n", None)], with_ruled),
    Scenario("Clear rules resets them without saving, and is hidden while none is set",
             [(FIRST + OPEN_COLL + EDIT + "expect Clear rules\nclick Clear rules\nwait 400\nexpect-not Clear rules\n"
               + "expect-not-has <b>2</b></font> games match\nclick Installed\nwait 300\nexpect Clear rules\nclick Cancel\nwait 500\n"
               + OPEN_COLL + "click Faves\nwait 500\nexpect 2 games\nexpect Mario Tennis Aces\n", None)], with_ruled),
    Scenario("Rating chips hide half steps no game has, unless the collection saved one",
             [(FIRST + OPEN_COLL + EDIT + "expect Any\nexpect 3.5+\nexpect 4.5+\nexpect-not 1+\nexpect-not 2+\n"
               + "click 4.5+\nwait 400\nexpect 4.5+\n", None)], with_ruled),
    Scenario("a saved rating keeps its chip even when no game has it",
             [(FIRST + OPEN_COLL + EDIT + "expect 2+\nexpect 3.5+\nexpect-not 1+\nclick Any\nwait 400\n"
               + "expect 2+\nclick Cancel\nwait 500\n", None)], with_rated),
]

def with_tagged(dirs):
    dirs.use_demo()
    dirs.write("library/collections.toml", COLLECTIONS + RULED.replace("favorites_only = true", 'tags = ["Racing"]'))


SCENARIOS += [
    Scenario("Edit collection's tag picker adds a tag from the search, ignoring case and accents, and Match shows with two",
             [(FIRST + OPEN_COLL + EDIT + "expect Racing\nexpect-not Match\nclick Search tags to add\nwait 300\nexpect Platformer\ntype pLATFÖRMER\nwait 400\n"
               + "expect Platformer\nexpect-not Roguelike\nclick Platformer\nwait 400\nexpect Match\nexpect All\n"
               + "type zzz\nwait 300\nexpect-has No other tag called\nkey Escape\nwait 300\n"
               + "expect Edit collection\nclick All\nwait 300\nexpect-has <b>1</b></font> game matches\nclick Any\nwait 300\nexpect-not-has <b>1</b></font> game matches\nclick All\nwait 300\nclick Save\nwait 600\n", None),
              ("expect Leave the collection\n" + OPEN_COLL + "expect Faves\nkey Escape\nwait 400\n" + EDIT
               + "expect Racing\nexpect Platformer\nexpect Match\nexpect-has <b>1</b></font> game matches\n", None)], with_tagged),
    Scenario("Edit collection's tag picker removes a tag with its x, hides Match under two and Return adds the first match",
             [(FIRST + OPEN_COLL + EDIT + "click Search tags to add\ntype Plat\nkey Return\nwait 400\nexpect Match\n"
               + "click Racing\nwait 400\nexpect-not Match\nclick Save\nwait 600\n", None),
              ("expect Leave the collection\n" + OPEN_COLL + "key Escape\nwait 400\n" + EDIT
               + "expect-not Match\nexpect Search tags to add\n", None)], with_tagged),
]

TABS = "expect Rules\nexpect Added by hand\nexpect Left out\nexpect Library\n"
LEFT_HINT = "expect Uncheck a game to let the rules include it again.\n"
ADDED_HINT = "expect Uncheck a game to take it out of the collection.\n"

SCENARIOS += [
    Scenario("Edit collection's tabs switch between the rules and the two lists of exceptions",
             [(FIRST + OPEN_COLL + EDIT + TABS
               + "click Left out\nwait 300\nexpect Hades\nexpect-not Library\n" + LEFT_HINT
               + "click Added by hand\nwait 300\nexpect Pikmin 4\n" + ADDED_HINT
               + "click Rules\nwait 300\nexpect Library\nexpect-not Uncheck a game to take it out of the collection.\n", None)], with_ruled),
    Scenario("Edit collection's tab counts follow the ticks, and the footer count follows them too",
             [(FIRST + OPEN_COLL + EDIT + "expect-has <b>2</b></font> games match\nclick Left out\nwait 300\nclick Hades\nwait 400\n"
               + "expect-has <b>3</b></font> games match\nexpect 0\nclick Added by hand\nwait 300\nclick Pikmin 4\nwait 400\n"
               + "expect-has <b>2</b></font> games match\nclick Cancel\nwait 500\n", None)], with_ruled),
    Scenario("Edit collection's tabs follow the keyboard: Tab reaches the open one after the switch, arrows change the tab",
             [(FIRST + OPEN_COLL + EDIT + "key Tab\nkey Tab\nkey Tab\nkey Tab\nkey Right\nwait 300\nexpect Pikmin 4\n" + ADDED_HINT
               + "key Right\nwait 300\nexpect Hades\n" + LEFT_HINT
               + "key Left\nkey Left\nwait 300\nexpect Library\n", None)], with_ruled),
    Scenario("Edit collection with Add games by rules off has no tabs, just the Games, and turning it on again keeps the rules",
             [(FIRST + OPEN_COLL + EDIT + "toggle Add games by rules\nwait 300\nexpect Games\nexpect Pikmin 4\n" + ADDED_HINT
               + "expect-not Rules\nexpect-not Left out\nexpect-not Library\n"
               + "toggle Add games by rules\nwait 300\nexpect Rules\nexpect Left out\nexpect Library\nexpect-not Games\n"
               + "click Added by hand\nwait 300\nexpect Pikmin 4\nclick Cancel\nwait 400\n", None)], with_ruled),
    Scenario("Edit collection's tab with nothing in it says so",
             [(FIRST + OPEN_COLL + "expect Nothing yet\nclick Nothing yet\nwait 400\nkey Menu\nwait 400\nclick Edit\nwait 600\n"
               + "toggle Add games by rules\nwait 300\nclick Left out\nwait 300\n"
               + "expect No games are left out.\nclick Added by hand\nwait 300\nexpect No games were added by hand.\n", None)],
             with_collections),
]

IMPORT = OPEN_COLL + "click Import from Steam\nwait 700\nexpect Import collections from Steam\n"


def with_nothing_in_steam(dirs):
    dirs.use_demo()
    dirs.write("steam/userdata/39734273/config/cloudstorage/cloud-storage-namespace-1.json", "[]")


# The demo's fake Steam has a cloud file: two hand-picked collections, one from a tag, a dynamic one,
# Steam's favorite and hidden, and a deleted one. 999001 isn't a game in the demo.
SCENARIOS += [
    Scenario("Import from Steam lists the hand-picked collections, imports them and sums up",
             [(FIRST + IMPORT + "expect-has Weekend Games\nexpect-has Couch Co-op\nexpect-has Roguelike\n"
               + "expect-not-has Rule Based\nexpect-not-has Favorites\nexpect-not-has Hidden\n"
               + "expect-has 1 collection of Steam's own rules are left out\n"
               + "expect-has Nothing is changed in Steam.\nexpect-not-has Imported before\nexpect Select all\nexpect 3 of 3 selected\n"
               + "click Import 3 collections\nwait 800\nexpect Imported from Steam\nexpect Added\nexpect-has Weekend Games, Couch Co-op, Roguelike\n"
               + "expect Not in your library\nexpect-has 999001\n"
               + "click Close\nwait 500\n" + OPEN_COLL + "expect Weekend Games\nexpect Couch Co-op\nexpect Roguelike\n"
               + "click Weekend Games\nwait 500\nexpect 2 games\nexpect Hades\nexpect-not Pikmin 4\n", None)],
             lambda dirs: dirs.use_demo()),
    Scenario("Importing again says what is lost, keeps a rename and Undo takes the import out",
             [(FIRST + IMPORT + "click Import 3 collections\nwait 800\nexpect Added\nclick Close\nwait 500\n"
               + "expect-has 3 collections imported from Steam\nkey Ctrl+Z\nwait 600\n" + OPEN_COLL
               + "expect-not Weekend Games\n", None),
              (FIRST + IMPORT + "click Import 3 collections\nwait 800\nexpect Added\nclick Close\nwait 500\n"
               + OPEN_COLL + "click Roguelike\nwait 500\nkey Menu\nwait 400\nclick Rename\nwait 500\n"
               + "key Ctrl+A\ntype Dungeons\nkey Return\nwait 600\n", None),
              ("expect Leave the collection\n" + LEAVE + FIRST + IMPORT + "expect Roguelike\nexpect-has Imported before  ·  1 game\n"
               + "expect-has Games you added to them in Lattis are removed\nclick Import 3 collections\nwait 800\n"
               + "expect Nothing needed changing.\nexpect-not Added\nclick Close\nwait 500\n"
               + OPEN_COLL + "expect Dungeons\nexpect Weekend Games\n", None)],
             lambda dirs: dirs.use_demo()),
    Scenario("Import from Steam with nothing to import says so",
             [(FIRST + IMPORT + "expect This account has no collections of its own to import.\n"
               + "click Cancel\nwait 400\n" + FIRST, None)], with_nothing_in_steam),
]

sys.exit(run_scenarios(SCENARIOS))
