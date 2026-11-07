import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

OPEN = "key F7\nexpect Library overview\nexpect Needs attention\nwait 1200\n"
DOWN = "key End\nwait 300\n"
# The page is taller than the window: End shows the last cards, a page down the Status card.
STATUS = "key PageDown\nwait 300\n"
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
               + STATUS + "click Uninstall\nexpect Cancel\nclick Cancel\n"
               + "expect-not Cancel\nexpect Library overview\n" + "key Home\nwait 300\n"
               + "click Dormant\nexpect Not played yet\n", None)], demo),
    Scenario("Games by platform, Status, Top tags and Release years list the library",
             [(OPEN + "expect Games by platform\nexpect Status\nexpect Top tags\nexpect Release years\n"
               + "expect Dropped\nexpect Roguelike\n", None)], demo),
    Scenario("a tag in Top tags opens the game list with that tag on",
             [(OPEN + DOWN + "click Roguelike\nwait 500\nexpect-not Library overview\nexpect Hades\n"
               + "expect-not Metroid Dread\n", None)], demo),
    Scenario("a status in the overview opens the game list with that status on, uninstalled games included",
             [(OPEN + STATUS + "click Dropped\nwait 500\nexpect-not Library overview\nexpect Final Fantasy XIV Online\n"
               + "expect-not Metroid Dread\n", None)], demo),
    Scenario("a platform in the overview opens the game list on that platform, and the next click replaces the filters",
             [(OPEN + "click Proton\nwait 500\nexpect-not Library overview\nexpect Final Fantasy XIV Online\n"
               + "expect-not Metroid Dread\n"
               + "key F7\nexpect Library overview\n" + STATUS + "click Completed\nwait 500\nexpect Metroid Dread\n"
               + "expect-not Final Fantasy XIV Online\n", None)], demo),
    Scenario("Needs attention lists what to fix and Look up opens Look up missing info",
             [(OPEN + DOWN + "expect Needs attention\nexpect Games without a cover\nexpect Games without game info\n"
               + "click Look up\nwait 800\nexpect Missing info\nexpect Games not on IGDB\n", None)], demo),
    Scenario("Needs attention leaves out a row with nothing to report",
             [(OPEN + DOWN + "expect-not Steam updates waiting\n", None)], demo),
    Scenario("From your backlog offers a game with Play and Shuffle, and Shuffle keeps the card",
             [(OPEN + DOWN + "expect From your backlog\nexpect Play\nexpect Shuffle\nclick Shuffle\nwait 500\n"
               + "expect From your backlog\nexpect Play\n", None)], demo),
    Scenario("the Games tile opens the whole library, filters cleared",
             [(OPEN + STATUS + "click Dropped\nwait 500\nexpect-not Metroid Dread\n"
               + "key F7\nexpect Library overview\nkey Home\nwait 300\nclick Games\nwait 500\n"
               + "expect-not Library overview\nexpect Metroid Dread\nexpect Final Fantasy XIV Online\n", None)], demo),
]

sys.exit(run_scenarios(SCENARIOS))
