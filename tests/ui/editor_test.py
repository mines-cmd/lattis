import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# Properties asks "Discard your changes?" before Escape, Cancel or the ✕ drop what is unsaved, and only then.

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN = FIRST + "click Mario Kart 8 Deluxe\nkey Alt+Return\nexpect Properties\n"
EDIT = "click Mario Kart 8 Deluxe\nkey Ctrl+A\ntype Mario Kart 9 Deluxe\nexpect Unsaved changes in General\n"
ASKED = "wait 300\nexpect Discard your changes?\nexpect Keep editing\nexpect Discard and close\n"


def demo(dirs):
    dirs.use_demo()


SCENARIOS = [
    Scenario("Escape with an edited title asks first, Keep editing leaves it, Discard and close drops it",
             [(OPEN + EDIT + "key Escape\n" + ASKED + "click Keep editing\nwait 300\nexpect Properties\n"
               + "expect Unsaved changes in General\nkey Escape\n" + ASKED + "click Discard and close\nwait 400\n"
               + "expect-not Properties\nexpect-not Discard your changes?\nkey Alt+Return\nexpect Properties\n"
               + "expect-not Unsaved changes in General\nexpect-has Mario Kart 8 Deluxe\n"
               + "expect-not-has Mario Kart 9\n", None)], demo),
    Scenario("Cancel asks the same question",
             [(OPEN + EDIT + "click Cancel\n" + ASKED + "click Keep editing\nwait 300\nexpect Properties\n"
               + "click Cancel\n" + ASKED + "click Discard and close\nwait 400\nexpect-not Properties\n", None)], demo),
    Scenario("Properties with nothing changed closes at once, and Save closes without asking",
             [(OPEN + "key Escape\nwait 400\nexpect-not Properties\nexpect-not Discard your changes?\n"
               + "key Alt+Return\nexpect Properties\nclick Cancel\nwait 400\nexpect-not Properties\n"
               + "expect-not Discard your changes?\n"
               + "key Alt+Return\nexpect Properties\n" + EDIT + "click Save\nwait 400\nexpect-not Properties\n"
               + "expect-not Discard your changes?\n", None)], demo),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
