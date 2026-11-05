import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import ROOT, Scenario, run_scenarios

FIXTURES = os.path.join(ROOT, "tests", "data", "metadata")

# The demo's Steam games: Hades 1145360, Celeste 504230, Hollow Knight 367520.
# An answer takes 300 ms to be asked for and 300 ms to arrive.
ANSWER = "wait 1500\n"
BAD = {"protondb-367520.json": "not json"}


def setup(chips=True, answers=None):
    def run(dirs):
        dirs.use_demo()
        folder = os.path.join(dirs.root, "fixtures")
        shutil.copytree(FIXTURES, folder)
        for name, content in (answers or {}).items():
            with open(os.path.join(folder, name), "w") as f:
                f.write(content)
        dirs.env["LATTIS_METADATA_FIXTURES"] = folder
        if chips:
            with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
                f.write('\n[game_page]\nchips = ["protondb"]\n')
    return run


# Labels show in capitals. Clicking the chip opens a browser, which the offscreen platform can't do: that
# scenario only checks that the click goes through without a warning.
SCENARIOS = [
    Scenario("off by default: no chip",
             [("click Hades\n" + ANSWER + "expect Hades\nexpect-not PROTONDB\nexpect-not Platinum\n", None)],
             setup(chips=False)),
    Scenario("a game with reports shows its tier",
             [("click Hades\n" + ANSWER + "expect PROTONDB\nexpect Platinum\n", None)], setup()),
    Scenario("a game without reports says so",
             [("click Celeste\n" + ANSWER + "expect PROTONDB\nexpect No reports\n", None)], setup()),
    Scenario("an answer that can't be read leaves Open page",
             [("click Hollow Knight\n" + ANSWER + "expect PROTONDB\nexpect Open page\n", None)], setup(answers=BAD)),
    Scenario("a tier the launcher doesn't know leaves Open page",
             [("click Hollow Knight\n" + ANSWER + "expect Open page\nexpect-not Platinum\n", None)],
             setup(answers={"protondb-367520.json": '{"tier":"mythic","trendingTier":"mythic","total":5}'})),
    Scenario("no chip on a game that isn't a Steam game",
             [("click Mario Kart 8 Deluxe\n" + ANSWER + "expect Mario Kart 8 Deluxe\nexpect-not PROTONDB\n", None)],
             setup()),
    Scenario("the chip can be clicked in every state",
             [("click Hades\n" + ANSWER + "click Platinum\nwait 300\nclick Celeste\n" + ANSWER
               + "click No reports\nwait 300\nclick Hollow Knight\n" + ANSWER + "click Open page\nwait 300\n", None)],
             setup(answers=BAD)),
]

sys.exit(run_scenarios(SCENARIOS))
