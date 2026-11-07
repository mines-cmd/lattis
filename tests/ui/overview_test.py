import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

OPEN = "key F7\nexpect Library overview\n"
CARDS = "expect Games\nexpect Hours played\nexpect Backlog\nexpect Continue playing\n"


def demo(dirs):
    dirs.use_demo()


SCENARIOS = [
    Scenario("F7 opens the overview with its tiles and Continue playing, Escape closes it",
             [(OPEN + CARDS + "key Escape\nexpect-not Library overview\nexpect Hades\n", None)], demo),
    Scenario("F7 again closes the overview",
             [(OPEN + "key F7\nexpect-not Library overview\n", None)], demo),
    Scenario("Play statistics opens from the overview and takes its place",
             [(OPEN + "click Play statistics\nexpect-not Library overview\n"
               + "expect Sessions appear here once a game is played.\n", None)], demo),
    Scenario("the overview replaces Play statistics and F9 swaps back",
             [("key F9\nexpect Sessions appear here once a game is played.\n"
               + "key F7\nexpect Library overview\nexpect-not Sessions appear here once a game is played.\n"
               + "key F9\nexpect Sessions appear here once a game is played.\nexpect-not Library overview\n", None)],
             demo),
    Scenario("the app menu's Library overview entry opens the overview",
             [("key F10\nwait 600\nclick Library overview\nwait 300\nexpect Back to games\nexpect Storage\n", None)],
             demo),
    Scenario("Storage lists the largest games, switches to Dormant and Uninstall asks first",
             [(OPEN + "expect Storage\nexpect Largest\nexpect Dormant\nexpect Baldur's Gate 3\n"
               + "click Uninstall\nexpect Uninstall Hades?\nexpect Cancel\nclick Cancel\n"
               + "expect-not Uninstall Hades?\nexpect Library overview\n"
               + "click Dormant\nexpect Not played yet\n", None)], demo),
]

sys.exit(run_scenarios(SCENARIOS))
