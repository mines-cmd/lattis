import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import ROOT, Scenario, run_scenarios

LINKED = os.path.join(ROOT, "tools", "demo-linked")


def with_linked(dirs):
    dirs.use_demo()
    shutil.copytree(LINKED, dirs.data, dirs_exist_ok=True)


def game_file(dirs, name):
    return open(os.path.join(dirs.data, "library", "games", f"{name}.toml")).read()


def select(title):
    return f"type {title}\nwait 400\nclick {title}\nwait 300\n"


COPIES = "key Alt+Return\nexpect Properties\nclick Copies\nwait 300\nexpect Linked copies\n"


def unlinked(result, dirs):
    if "same_as" in game_file(dirs, "proton-hades"):
        return ["Unlink all wasn't saved: " + game_file(dirs, "proton-hades")]
    return None


SCENARIOS = [
    Scenario("Unlink all waits for Save, shows the unsaved dot's note and writes once",
             [(select("Hades") + COPIES + "expect-has Together\nclick Unlink all\nwait 300\n"
               "expect-has The combined numbers update\nexpect-not-has Together\n"
               "click Save\nwait 800\nexpect-not Properties\n", unlinked),
              (select("Hades") + COPIES + "expect-has Copies of one game from different\n"
               "click Cancel\nwait 300\n", None)],
             with_linked),
    Scenario("Cancel drops a pending change",
             [(select("Hades") + COPIES + "click Unlink all\nwait 300\nclick Cancel\nwait 400\n", None),
              (select("Hades") + COPIES + "expect-has Together\nclick Cancel\nwait 300\n", None)],
             with_linked),
    Scenario("Choose games links a copy with another title, and Open asks before dropping it",
             [(select("Hades") + COPIES + "click Choose games…\nwait 400\nexpect-has Choose games to link\n"
               "type celeste\nwait 300\nclick Celeste\nwait 200\nclick Link\nwait 500\nexpect New\n"
               "click Open\nwait 400\nexpect-has Discard your changes\nclick Keep editing\nwait 300\nexpect New\n"
               "click Open\nwait 400\nclick Discard and open\nwait 600\nexpect Properties\nexpect-not New\n"
               "click Cancel\nwait 300\n", None)],
             with_linked),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
