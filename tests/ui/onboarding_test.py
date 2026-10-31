import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

WELCOME = "expect Set up a new library\nexpect Restore from a backup\nexpect Skip setup\n"
STEAM = "expect-has Two ways to bring in Steam games\n"
EMULATORS = "expect Emulators and ROM folders\n"
GAME_INFO = "expect-has free API keys\n"
LOOK = "expect-has Pick colors and a style\n"
DONE = "expect You're all set\nexpect Start\nexpect-not Skip setup\n"
EMPTY_LIBRARY = "expect-not Skip setup\nexpect-not You're all set\nexpect No games yet\n"
NO_GUIDE_AFTER = "expect No games yet\nexpect-not Skip setup\nexpect-not Set up a new library\n"


def guide_on_f7(dirs):
    dirs.write("settings.toml", "[shortcuts]\nsetup_guide = \"F7\"\n")


def empty(dirs):
    pass


def step(name, *screens):
    return name + "\n" + "".join(screens)


FORWARD = (
    WELCOME
    + "click Next\n" + STEAM
    + "click Next\n" + EMULATORS
    + "click Next\n" + GAME_INFO
    + "click Next\n" + LOOK
    + "click Finish\n" + DONE
    + "click Start\n" + EMPTY_LIBRARY
)

SCENARIOS = [
    Scenario("Next through every step, then Start", [(FORWARD, None)], empty),
    Scenario("finishing persists: no guide on the next start",
             [(FORWARD, None), (NO_GUIDE_AFTER, None)], empty),
    Scenario("Return advances like Next",
             [(WELCOME + "key Return\n" + STEAM + "key Return\n" + EMULATORS + "key Return\n" + GAME_INFO
               + "key Return\n" + LOOK + "key Return\n" + DONE + "key Return\n" + EMPTY_LIBRARY, None)],
             empty),
    Scenario("Back steps back",
             [(WELCOME + "click Next\n" + STEAM + "click Next\n" + EMULATORS + "click Back\n" + STEAM
               + "click Back\n" + WELCOME, None)],
             empty),
    Scenario("Back is hidden on Welcome and on the last step",
             [(WELCOME + "expect-not Back\nclick Next\nexpect Back\n"
               + "click Next\nclick Next\nclick Next\nclick Finish\n" + DONE + "expect-not Back\n", None)],
             empty),
    Scenario("clicking a step name jumps to it",
             [(WELCOME + "click Game info\n" + GAME_INFO + "click Emulators\n" + EMULATORS + "click Steam\n"
               + STEAM + "click Welcome\n" + WELCOME, None)],
             empty),
    Scenario("steps already visited can be left and revisited",
             [(WELCOME + "click Next\nclick Next\nclick Next\nclick Look\n" + LOOK + "click Back\n" + GAME_INFO
               + "click Back\n" + EMULATORS + "click Next\n" + GAME_INFO, None)],
             empty),
    Scenario("Skip setup closes the guide",
             [(WELCOME + "click Skip setup\n" + EMPTY_LIBRARY, None)], empty),
    Scenario("Skip setup persists: no guide on the next start",
             [(WELCOME + "click Skip setup\n" + EMPTY_LIBRARY, None), (NO_GUIDE_AFTER, None)], empty),
    Scenario("Skip setup from the middle persists",
             [(WELCOME + "click Next\nclick Next\n" + EMULATORS + "click Skip setup\n" + EMPTY_LIBRARY, None),
              (NO_GUIDE_AFTER, None)],
             empty),
    Scenario("a guide left open without finishing comes back on the next start",
             [(WELCOME + "click Next\n" + STEAM, None), (WELCOME, None)], empty),
    Scenario("a shortcut reopens the guide as Close, not Skip setup",
             [(WELCOME + "click Skip setup\n" + EMPTY_LIBRARY + "key F7\n"
               + "expect Close\nexpect-not Skip setup\nexpect Set up a new library\nclick Close\n"
               + "expect-not Set up a new library\n", None)],
             guide_on_f7),
    Scenario("Escape closes a reopened guide but not the first-start guide",
             [(WELCOME + "key Escape\n" + WELCOME + "click Skip setup\n" + EMPTY_LIBRARY
               + "key F7\nexpect Set up a new library\nwait 600\nkey Escape\n"
               + "expect-not Set up a new library\n", None)],
             guide_on_f7),
    Scenario("a reopened guide can be walked through to the end",
             [(WELCOME + "click Skip setup\n" + EMPTY_LIBRARY + "key F7\n"
               + "click Next\n" + STEAM + "click Next\n" + EMULATORS + "click Next\n" + GAME_INFO
               + "click Next\n" + LOOK + "click Finish\n" + DONE + "click Start\n" + EMPTY_LIBRARY, None)],
             guide_on_f7),
]

sys.exit(run_scenarios(SCENARIOS))
