import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import ROOT, Scenario, run_scenarios

FIXTURES = os.path.join(ROOT, "tests", "data", "metadata")
FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
ACES_TEXT = "A new Mario Tennis game is bringing a new level of skill"
BARE_ACES = 'title = "Mario Tennis Aces"\nplatform = "switch"\n'
ACES_FILE = "library/games/switch-mario-tennis-aces.toml"
PIKMIN_ANSWER = "igdb-search-pikmin-4.json"


def fixtures(dirs, answers=None, files=None):
    dirs.use_demo()
    # The demo has the game's file but not its ROM, so the scan would drop it.
    dirs.write("roms/switch/Mario Tennis Aces [0100BDE00862A000][v0].nsp", b"")
    folder = os.path.join(dirs.root, "fixtures")
    shutil.copytree(FIXTURES, folder)
    for name, content in (answers or {}).items():
        with open(os.path.join(folder, name), "w") as f:
            f.write(content)
    for path, content in (files or {}).items():
        dirs.write(path, content)
    dirs.env["LATTIS_METADATA_FIXTURES"] = folder


def setup(answers=None, files=None):
    return lambda dirs: fixtures(dirs, answers, files)


def properties(game):
    return f"click {game}\nkey Alt+Return\nexpect Properties\nclick Look up in IGDB\n"


def art_browser():
    return ("click Mario Tennis Aces\nkey Alt+Return\nexpect Properties\nclick Images\nclick Browse SteamGridDB\n"
            "wait 1500\n")


def bulk(search):
    return ("key Ctrl+Shift+L\nexpect Look up missing info\nclick Search titles\ntype " + search
            + "\nwait 500\nclick Look up 1\nwait 1500\n")


def malformed(body):
    return Scenario(f"a malformed IGDB answer ({body[:24]!r}) ends the lookup without a crash",
                    [(FIRST + properties("Pikmin 4") + "wait 1500\nexpect-not Cancel lookup\nexpect-has IGDB \nexpect Look up in IGDB\n"
                      + "click Cancel\nexpect-not Properties\n" + FIRST, None)],
                    setup({PIKMIN_ANSWER: body}))


SCENARIOS = [
    Scenario("a clear match fills the empty fields, Undo lookup puts them back",
             [(FIRST + properties("Mario Tennis Aces")
               + "expect-has Filled in from IGDB: Mario Tennis Aces\nexpect-has " + ACES_TEXT + "\n"
               + "click Undo lookup\nexpect-not-has Filled in from IGDB\nexpect-not-has " + ACES_TEXT + "\n", None)],
             setup(files={ACES_FILE: BARE_ACES})),
    Scenario("a looked-up game keeps its info after Save and a restart",
             [(FIRST + properties("Mario Tennis Aces") + "expect-has Filled in from IGDB\nclick Save\n"
               + "expect-not Properties\nwait 600\n", None),
              (FIRST + "click Mario Tennis Aces\nexpect-has " + ACES_TEXT + "\n", None)],
             setup(files={ACES_FILE: BARE_ACES})),
    Scenario("fields that already have a value are listed, Replace these too takes IGDB's",
             [(FIRST + properties("Mario Tennis Aces") + "expect-has differ from what you have\n"
               + "expect-not-has " + ACES_TEXT + "\nclick Replace these too\nexpect-has " + ACES_TEXT + "\n", None)],
             setup()),
    Scenario("several games of one name show the pick list",
             [(FIRST + properties("Metroid Dread") + "expect Which game is Metroid Dread?\nexpect None of these\n"
               + "expect Not on IGDB\nclick None of these\nexpect-not Which game is Metroid Dread?\n", None)],
             setup()),
    Scenario("a title IGDB doesn't know says so and offers Detailed lookup",
             [(FIRST + properties("Pikmin 4") + "expect-has IGDB has nothing called Pikmin 4\nexpect Detailed lookup\n"
               + "click Cancel\nexpect-not Properties\n", None)], setup()),
    Scenario("bulk lookup: a matched game is applied and kept after a restart",
             [(FIRST + bulk("Aces") + "expect Review what was found\nexpect 1 of 1 looked up\nexpect Matched\n"
               + "expect-has Year 2018\nexpect Apply 1\nclick Apply 1\nexpect-has Changes to Mario Tennis Aces\nwait 600\n", None),
              (FIRST + "click Mario Tennis Aces\nexpect-has " + ACES_TEXT + "\n", None)],
             setup(files={ACES_FILE: BARE_ACES})),
    Scenario("bulk lookup: Back to the list returns to the games, nothing is written",
             [(FIRST + bulk("Aces") + "expect Matched\nclick Back to the list\nexpect Look up 1\n"
               + "key Escape\n", None),
              (FIRST + "click Mario Tennis Aces\nexpect-not-has " + ACES_TEXT + "\n", None)],
             setup(files={ACES_FILE: BARE_ACES})),
    Scenario("bulk lookup: a match can be changed with Detailed lookup",
             [(FIRST + bulk("Mario Tennis") + "expect Matched\nclick Detailed lookup\nwait 800\n"
               + "expect Look up Mario Tennis Aces in IGDB\nclick Cancel\n"
               + "expect-not Look up Mario Tennis Aces in IGDB\nexpect Matched\n", None)],
             setup(files={ACES_FILE: BARE_ACES})),
    Scenario("bulk lookup: None found opens the art browser on another game",
             [(FIRST + bulk("Mario Tennis") + "expect Matched\nexpect None found\nclick None found\nwait 800\n"
               + "expect-has Art for Mario Tennis Aces\nexpect Search SteamGridDB\nclick Done\nwait 500\n"
               + "expect-not-has Art for Mario Tennis Aces\n"
               + "expect Matched\n", None)],
             setup({"sgdb-search-mario-tennis-aces.json": '{"success": true, "data": []}'}, {ACES_FILE: BARE_ACES})),
    Scenario("bulk lookup: several games of one name need a pick",
             [(FIRST + bulk("Metroid") + "expect Which game?\nexpect Needs a pick\n", None)], setup()),
    Scenario("bulk lookup: an unknown title is Not found and can be marked Not on IGDB",
             [(FIRST + bulk("Pikmin") + "expect Not found\nexpect Detailed lookup\nclick Not on IGDB\n"
               + "wait 400\nexpect Undo\n", None)], setup()),
    Scenario("bulk lookup: a malformed answer is a failed result with Retry, not a crash",
             [(FIRST + bulk("Pikmin") + "expect Couldn't look up\nexpect Failed\nexpect Retry\nexpect Apply 0\n"
               + "click Back to the list\nexpect Look up 1\n", None)],
             setup({PIKMIN_ANSWER: "{{ nope"})),
    Scenario("the art browser has a tab per kind and Cancel closes it",
             [(FIRST + art_browser() + "expect Art for Mario Tennis Aces\nexpect Cover\nexpect Hero\nexpect Logo\n"
               + "expect Icon\nclick Hero\nexpect Style\nclick Logo\nclick Icon\nclick Cover\n"
               + "expect Use selected\nclick Cancel\nexpect-not Use selected\nexpect Properties\n", None)], setup()),
    Scenario("Other game shows another game's art and Back returns to the game's own",
             [(FIRST + art_browser() + "click Other game\ntype hades\nwait 800\nexpect Hades\nclick Hades\nwait 1500\n"
               + "expect Art from Hades\nexpect Back to Mario Tennis Aces\nclick Hero\nexpect Style\n"
               + "click Back to Mario Tennis Aces\nwait 1500\nexpect Art for Mario Tennis Aces\n"
               + "expect-not Back to Mario Tennis Aces\n", None)], setup()),
    Scenario("Escape in Other game's search leaves the art browser open on the game's own art",
             [(FIRST + art_browser() + "click Other game\nkey Escape\nwait 400\nexpect Art for Mario Tennis Aces\n"
               + "expect Other game\n", None)], setup()),
    malformed("{{ not json"),
    malformed("[{\"id\": 1, \"name\": "),
    malformed("{\"error\": \"nope\"}"),
    malformed("[1, 2, \"x\"]"),
    malformed("[{\"id\": \"abc\", \"name\": 5, \"platforms\": \"x\", \"cover\": [], \"release_dates\": 3}]"),
    malformed(""),
]

sys.exit(run_scenarios(SCENARIOS))
