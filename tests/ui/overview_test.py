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
]

sys.exit(run_scenarios(SCENARIOS))
