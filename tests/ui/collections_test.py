import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN_PANEL = "key Ctrl+Shift+F\nwait 600\nexpect COLLECTIONS\n"
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
             [(FIRST + OPEN_PANEL + "expect Mix\nexpect Nothing yet\nclick Mix\nwait 500\n" + MIX
               + "expect Leave the collection\n", None)], with_collections),
    Scenario("filters narrow within a collection and Clear filters returns to its full list",
             [(FIRST + OPEN_PANEL + "click Mix\nwait 500\n" + MIX
               + "click Installed\nwait 400\nexpect Hades\nexpect-not Mario Tennis Aces\nexpect Installed only\n"
               + "click Clear filters\nwait 400\n" + MIX + "expect-not Installed only\n"
               + "expect Leave the collection\n", None)], with_collections),
    Scenario("leaving brings back the filters and the platform from before",
             [(FIRST + "key Ctrl+Shift+D\nwait 400\nexpect Hades\nexpect-not Pikmin 4\n"
               + OPEN_PANEL + "click Mix\nwait 500\n" + MIX + LEAVE
               + "expect Hades\nexpect-not Pikmin 4\nexpect-not Mario Tennis Aces\n"
               + "key Ctrl+Shift+D\nwait 400\n" + FIRST, None)], with_collections),
    Scenario("a collection with no games says so and stays in the panel",
             [(FIRST + OPEN_PANEL + "click Nothing yet\nwait 500\nexpect This collection has no games\n"
               + "expect 0 games\nexpect-not Hades\n"
               + "click Leave the collection\nwait 500\nexpect-not This collection has no games\n" + FIRST
               + OPEN_PANEL + "expect Nothing yet\n", None)], with_collections),
    Scenario("an open collection is still open after a restart",
             [(FIRST + OPEN_PANEL + "click Mix\nwait 500\n" + MIX, None),
              ("expect 3 games\nexpect Hades\nexpect-not Mario Kart 8 Deluxe\n" + LEAVE + FIRST, None)],
             with_collections),
    Scenario("a number on the overview leaves the open collection",
             [(FIRST + OPEN_PANEL + "click Mix\nwait 500\n" + MIX
               + "key F7\nexpect Library overview\nexpect Needs attention\nwait 1200\n"
               + "key PageDown\nwait 300\nclick Proton\nwait 500\nexpect-not Library overview\n"
               + "expect Final Fantasy XIV Online\nexpect-not 3 games\n", None)], with_collections),
    Scenario("the Tags section starts collapsed, opens on a click and stays as left",
             [(FIRST + OPEN_PANEL + "expect TAGS\nexpect-not Racing\nclick TAGS\nwait 400\nexpect Racing\n", None),
              (FIRST + OPEN_PANEL + "expect Racing\nclick TAGS\nwait 400\nexpect-not Racing\n", None)],
             with_collections),
    Scenario("a selection goes into a collection from the game menu, and Undo takes it out",
             [(FIRST + THREE + MENU + "click Nothing yet\nwait 600\nexpect-has 3 games added to Nothing yet\n"
               + "key Escape\nkey Escape\nwait 400\n" + OPEN_PANEL + "click Nothing yet\nwait 500\n"
               + "expect 3 games\nexpect Metroid Dread\nkey Ctrl+Z\nwait 600\nexpect This collection has no games\n",
               None)], with_collections),
    Scenario("New collection in the game menu names a collection for the selection",
             [(FIRST + THREE + MENU + "click New collection\nwait 400\nexpect New collection for 3 games\n"
               + "type Switch picks\nkey Return\nwait 600\nexpect-has Collection Switch picks made with 3 games\n"
               + OPEN_PANEL + "expect Switch picks\nclick Switch picks\nwait 500\n"
               + "expect 3 games\nexpect Metroid Dread\nexpect-not Hades\n", None),
              ("expect 3 games\n" + LEAVE + FIRST + OPEN_PANEL + "expect Switch picks\n", None)], with_collections),
    Scenario("Remove from collection takes a game out of the open collection only",
             [(FIRST + OPEN_PANEL + "click Mix\nwait 500\n" + MIX + "key Escape\nclick Hades\nkey Menu\nwait 500\n"
               + "click Remove from collection\nwait 600\nexpect-has 1 game removed from Mix\nexpect 2 games\n"
               + "expect-not Hades\nexpect Pikmin 4\nkey Escape\nkey Ctrl+Z\nwait 600\nexpect 3 games\n"
               + "expect Hades\n", None)], with_collections),
    Scenario("New collection in the filter panel makes an empty one that stays",
             [(FIRST + OPEN_PANEL + "click New collection\nwait 400\nexpect Name of the collection\n"
               + "type Fresh\nkey Return\nwait 600\n" + OPEN_PANEL + "expect Fresh\n", None),
              (FIRST + OPEN_PANEL + "expect Fresh\n", None)], with_collections),
    Scenario("a name already used is refused",
             [(FIRST + OPEN_PANEL + "click New collection\nwait 400\ntype mix\nkey Return\nwait 600\n"
               + "expect-has There is already a collection called Mix\n", None)], with_collections),
    Scenario("a collection's menu renames or deletes it, and Delete leaves the games",
             [(FIRST + OPEN_PANEL + "click Mix\nwait 500\nkey Menu\nwait 400\nclick Rename\nwait 500\n"
               + "expect Rename collection\nkey Ctrl+A\ntype Weekend\nkey Return\nwait 600\n"
               + "expect-has Collection renamed to Weekend\n" + OPEN_PANEL + "expect Weekend\nexpect-not Mix\n"
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
             [(FIRST + OPEN_PANEL + "expect-not Save as collection\nclick Favorites\nwait 400\n"
               + "expect Save as collection\nclick Save as collection\nwait 500\n"
               + "expect Save the filters as a collection\ntype Faves now\nkey Return\nwait 600\n"
               + "expect-has Collection Faves now saved from the filters\n"
               + OPEN_PANEL + "expect Faves now\nclick Faves now\nwait 500\n"
               + "expect 1 game\nexpect Hades\nexpect-not Pikmin 4\nexpect-not Installed only\n"
               + "expect-not Save as collection\n", None),
              ("expect 1 game\nexpect Hades\n" + LEAVE + "expect Hades\nexpect-not Pikmin 4\n"
               + OPEN_PANEL + "expect Faves now\n", None)], with_collections),
    Scenario("Edit collection lists the exceptions, drops one, and Undo puts it back",
             [(FIRST + OPEN_PANEL + EDIT + "expect Keep up to date with these rules\nexpect Added by hand\n"
               + "expect Left out\nexpect Hades\nclick Hades\nclick Save\nwait 600\n"
               + "expect-has Collection Faves changed\n" + OPEN_PANEL + "click Faves\nwait 500\n"
               + "expect Hades\nexpect Mario Tennis Aces\nexpect Pikmin 4\nexpect 3 games\n"
               + "key Ctrl+Z\nwait 600\nexpect-not Hades\nexpect 2 games\n", None)], with_ruled),
    Scenario("Edit collection can turn the rules off, leaving the games added by hand",
             [(FIRST + OPEN_PANEL + EDIT + "click Keep up to date with these rules\nwait 300\nexpect-not Left out\n"
               + "click Save\nwait 600\n" + OPEN_PANEL + "click Faves\nwait 500\n"
               + "expect 1 game\nexpect Pikmin 4\nexpect-not Mario Tennis Aces\n", None)], with_ruled),
    Scenario("Cancel in Edit collection changes nothing",
             [(FIRST + OPEN_PANEL + EDIT + "click Hades\nclick Cancel\nwait 500\n" + OPEN_PANEL
               + "click Faves\nwait 500\nexpect 2 games\nexpect-not Hades\n", None)], with_ruled),
]

IMPORT = OPEN_PANEL + "click Import from Steam\nwait 700\nexpect Import collections from Steam\n"


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
               + "expect Nothing is changed in Steam.\nexpect-not-has imported before\n"
               + "click Import\nwait 800\nexpect Imported from Steam\nexpect-has Added: Weekend Games, Couch Co-op, Roguelike\n"
               + "expect-has 1 game in them aren't in your library\nexpect 999001\n"
               + "click Close\nwait 500\n" + OPEN_PANEL + "expect Weekend Games\nexpect Couch Co-op\nexpect Roguelike\n"
               + "click Weekend Games\nwait 500\nexpect 2 games\nexpect Hades\nexpect-not Pikmin 4\n", None)],
             lambda dirs: dirs.use_demo()),
    Scenario("Importing again says what is lost, keeps a rename and Undo takes the import out",
             [(FIRST + IMPORT + "click Import\nwait 800\nexpect-has Added:\nclick Close\nwait 500\n"
               + "expect-has 3 collections imported from Steam\nkey Ctrl+Z\nwait 600\n" + OPEN_PANEL
               + "expect-not Weekend Games\n", None),
              (FIRST + IMPORT + "click Import\nwait 800\nexpect-has Added:\nclick Close\nwait 500\n"
               + OPEN_PANEL + "click Roguelike\nwait 500\nkey Menu\nwait 400\nclick Rename\nwait 500\n"
               + "key Ctrl+A\ntype Dungeons\nkey Return\nwait 600\n", None),
              ("expect Leave the collection\n" + LEAVE + FIRST + IMPORT + "expect-has Roguelike (1 game), imported before\n"
               + "expect-has Games you added to them in Lattis are removed\nclick Import\nwait 800\n"
               + "expect Nothing needed changing.\nexpect-not-has Added:\nclick Close\nwait 500\n"
               + OPEN_PANEL + "expect Dungeons\nexpect Weekend Games\n", None)],
             lambda dirs: dirs.use_demo()),
    Scenario("Import from Steam with nothing to import says so",
             [(FIRST + IMPORT + "expect This account has no collections of its own to import.\n"
               + "click Cancel\nwait 400\n" + FIRST, None)], with_nothing_in_steam),
]

sys.exit(run_scenarios(SCENARIOS))
