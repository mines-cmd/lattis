import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The shared dialog base (AppDialog): a small dialog (New collection) and a medium one (Play history) each show
# their title, and Escape closes each.

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN_COLL = "key Ctrl+Shift+C\nwait 600\nexpect COLLECTIONS\n"
HISTORY = "type Mario Tennis Aces\nwait 400\nclick Show not installed\nwait 400\nclick Mario Tennis Aces\nwait 300\nkey Ctrl+Alt+4\nwait 500\n"


def setup(dirs):
    dirs.use_demo()
    settings = os.path.join(dirs.data, "settings.toml")
    with open(settings) as f:
        text = f.read()
    dirs.write("settings.toml", text + '\n[shortcuts]\nplay_history = "Ctrl+Alt+4"\n')


SCENARIOS = [
    Scenario("a small dialog shows its title and its footer, and Escape closes it",
             [(FIRST + OPEN_COLL + "click New collection\nwait 400\nexpect Name of the collection\nexpect-has New collection\n"
               + "expect Cancel\nkey Escape\nwait 500\nexpect-not Name of the collection\n", None)], setup),
    Scenario("a medium dialog shows its title and its footer, and Escape closes it",
             [(HISTORY + "expect Total\nexpect Play history\nexpect Close\nkey Escape\nwait 500\nexpect-not Total\n", None)],
             setup),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
