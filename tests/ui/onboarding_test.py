import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import ROOT, Scenario, run_scenarios

FAKE_HEROIC = os.path.join(ROOT, "tests", "data", "heroic")
EPIC_ALL = ("FezApp", "HikeApp", "RimeApp")

WELCOME = "expect Set up a new library\nexpect Restore from a backup\nexpect Skip setup\n"
STEAM = "expect-has Two ways to bring in Steam games\n"
HEROIC = "expect-has Your Epic games, installed or not\n"
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


def fake_heroic_program(dirs):
    program = os.path.join(dirs.bin, "heroic")
    with open(program, "w") as f:
        f.write(f'#!/bin/sh\necho opened >> "{opened_log(dirs)}"\n')
    os.chmod(program, 0o755)


def fake_heroic(dirs):
    root = os.path.join(dirs.root, "heroic")
    shutil.copytree(FAKE_HEROIC, root)
    dirs.write("settings.toml", f'[shortcuts]\nsetup_guide = "F7"\n\n[heroic]\npath = "{root}"\n')
    fake_heroic_program(dirs)
    return root


# A Heroic on the machine running the tests would otherwise count as installed.
def hide_heroic(dirs):
    dirs.env["PATH"] = dirs.bin


def empty_heroic(dirs):
    root = fake_heroic(dirs)
    with open(os.path.join(root, "store_cache", "legendary_library.json"), "w") as f:
        f.write('{"library": []}')
    with open(os.path.join(root, "legendaryConfig", "legendary", "installed.json"), "w") as f:
        f.write("{}")


def append_setting(dirs, line):
    with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
        f.write(line)


# Stands in for Heroic refreshing its cache after the sign-in, a few seconds into the guide.
def sign_in_later(dirs):
    empty_heroic(dirs)
    library = os.path.join(dirs.root, "heroic", "store_cache", "legendary_library.json")
    command = f'sleep 2; cp "{os.path.join(FAKE_HEROIC, "store_cache", "legendary_library.json")}" "{library}.new"; ' \
              f'mv "{library}.new" "{library}"'
    subprocess.Popen(["sh", "-c", f"sleep 1; {command}"])


def opened_log(dirs):
    return os.path.join(dirs.root, "opened.txt")


def opened_heroic(result, dirs):
    if not os.path.exists(opened_log(dirs)):
        return ["Open Heroic didn't start Heroic"]


def epic_games(dirs):
    index = os.path.join(dirs.data, "library", "heroic.toml")
    return sorted(re.findall(r'app_name = "(\w+)"', open(index).read())) if os.path.exists(index) else []


def imports(*expected):
    def check(result, dirs):
        if epic_games(dirs) != list(expected):
            return [f"the library has {epic_games(dirs)}"]
    return check


TO_HEROIC = WELCOME + "click Next\n" + STEAM + "click Next\n" + HEROIC + "wait 300\n"

FORWARD = (
    WELCOME
    + "click Next\n" + STEAM
    + "click Next\n" + HEROIC
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
             [(WELCOME + "key Return\n" + STEAM + "key Return\n" + HEROIC + "key Return\n" + EMULATORS
               + "key Return\n" + GAME_INFO
               + "key Return\n" + LOOK + "key Return\n" + DONE + "key Return\n" + EMPTY_LIBRARY, None)],
             empty),
    Scenario("Back steps back",
             [(WELCOME + "click Next\n" + STEAM + "click Next\n" + HEROIC + "click Next\n" + EMULATORS
               + "click Back\n" + HEROIC + "click Back\n" + STEAM
               + "click Back\n" + WELCOME, None)],
             empty),
    Scenario("Back is hidden on Welcome and on the last step",
             [(WELCOME + "expect-not Back\nclick Next\nexpect Back\n"
               + "click Next\nclick Next\nclick Next\nclick Next\nclick Finish\n" + DONE + "expect-not Back\n", None)],
             empty),
    Scenario("clicking a step name jumps to it",
             [(WELCOME + "click Game info\n" + GAME_INFO + "click Emulators\n" + EMULATORS + "click Heroic\n" + HEROIC
               + "click Steam\n"
               + STEAM + "click Welcome\n" + WELCOME, None)],
             empty),
    Scenario("steps already visited can be left and revisited",
             [(WELCOME + "click Next\nclick Next\nclick Next\nclick Next\nclick Look\n" + LOOK + "click Back\n" + GAME_INFO
               + "click Back\n" + EMULATORS + "click Next\n" + GAME_INFO, None)],
             empty),
    Scenario("Skip setup closes the guide",
             [(WELCOME + "click Skip setup\n" + EMPTY_LIBRARY, None)], empty),
    Scenario("Skip setup persists: no guide on the next start",
             [(WELCOME + "click Skip setup\n" + EMPTY_LIBRARY, None), (NO_GUIDE_AFTER, None)], empty),
    Scenario("Skip setup from the middle persists",
             [(WELCOME + "click Next\nclick Next\nclick Next\n" + EMULATORS + "click Skip setup\n" + EMPTY_LIBRARY, None),
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
               + "click Next\n" + STEAM + "click Next\n" + HEROIC + "click Next\n" + EMULATORS + "click Next\n" + GAME_INFO
               + "click Next\n" + LOOK + "click Finish\n" + DONE + "click Start\n" + EMPTY_LIBRARY, None)],
             guide_on_f7),
    Scenario("Heroic's step without Heroic is one short card",
             [(TO_HEROIC + "expect Heroic wasn't found\nexpect Not in ~/.config/heroic or the Flatpak's folder.\n"
               "expect Choose folder\nexpect-has Get Heroic\nexpect-not Find automatically\nexpect-not Epic Games\n"
               "expect-not GOG\nexpect-not-has Nothing is imported\n", None)],
             lambda dirs: (empty(dirs), hide_heroic(dirs))),
    Scenario("a Heroic with no library yet is found, and offers Open Heroic",
             [(TO_HEROIC + "expect Heroic found\nexpect It has no Epic library yet.\nexpect Choose folder\n"
               "expect-not-has Get Heroic\nexpect Epic Games\n"
               "expect No Epic games yet. Sign in to Epic in Heroic to see them here.\n"
               "expect-not Import the whole library\nclick Open Heroic\nwait 500\n", opened_heroic)],
             lambda dirs: (empty(dirs), hide_heroic(dirs), fake_heroic_program(dirs))),
    Scenario("a Heroic folder that isn't one offers Find automatically",
             [(TO_HEROIC + "expect There's no Heroic in nothing.\nexpect Find automatically\nexpect-not Epic Games\n"
               "click Find automatically\nwait 800\nexpect Not in ~/.config/heroic or the Flatpak's folder.\n"
               "expect-not Find automatically\n", None)],
             lambda dirs: (dirs.write("settings.toml", '[heroic]\npath = "nothing"\n'), hide_heroic(dirs))),
    Scenario("Heroic's step counts what Finish will add, and the other stores are listed",
             [(TO_HEROIC + "expect Heroic found\nexpect-has · native\n"
               "expect 1 installed game will be added when you finish.\n"
               "expect 2 more you own will be added as not installed.\nexpect-on Import Epic games\n"
               "expect-on Import the whole library\nexpect GOG\nexpect Amazon\nexpect-has Not implemented yet\n"
               "expect-has Nothing is imported\nclick Look\nclick Finish\n" + DONE, imports(*EPIC_ALL))],
             fake_heroic),
    Scenario("the whole library can be left out in the guide",
             [(TO_HEROIC + "toggle Import the whole library\nwait 800\nexpect-off Import the whole library\n"
               "expect Off: the 2 more you own won't be added.\nclick Look\nclick Finish\n" + DONE,
               imports("HikeApp"))],
             fake_heroic),
    Scenario("turning the import off keeps Heroic's row, and back on brings the counts back",
             [(TO_HEROIC + "toggle Import Epic games\nwait 800\nexpect Off: Epic games won't be imported.\n"
               "expect Heroic found\nexpect-not Import the whole library\nexpect GOG\n"
               "toggle Import Epic games\nwait 800\nexpect Heroic found\n"
               "expect 1 installed game will be added when you finish.\n", None)],
             fake_heroic),
    Scenario("the import off in Finish imports no Epic game",
             [(TO_HEROIC + "toggle Import Epic games\nwait 800\nclick Look\nclick Finish\n" + DONE, imports())],
             fake_heroic),
    Scenario("a guide started with the import off can turn it on",
             [(TO_HEROIC + "expect Heroic wasn't looked for\nexpect Off: Epic games won't be imported.\n"
               "expect-not Choose folder\ntoggle Import Epic games\nwait 800\nexpect Heroic found\n"
               "expect 1 installed game will be added when you finish.\n", None)],
             lambda dirs: (fake_heroic(dirs), append_setting(dirs, "enabled = false\n"))),
    Scenario("an empty Epic library offers Open Heroic",
             [(TO_HEROIC + "expect Heroic found\n"
               "expect No Epic games yet. Sign in to Epic in Heroic to see them here.\n"
               "expect-not Import the whole library\nclick Open Heroic\nwait 500\n", opened_heroic)],
             empty_heroic),
    Scenario("signing in during the guide fills the count in",
             [(TO_HEROIC + "expect No Epic games yet. Sign in to Epic in Heroic to see them here.\n"
               "wait 3500\nexpect 3 more you own will be added as not installed.\n"
               "expect-not No Epic games yet. Sign in to Epic in Heroic to see them here.\n", None)],
             sign_in_later),
    Scenario("a reopened guide says what the whole library switch takes out",
             [(TO_HEROIC + "click Look\nclick Finish\n" + DONE + "click Start\nwait 800\nkey F7\nwait 600\n"
               "click Heroic\nwait 500\nexpect 1 installed game.\n"
               "expect Turning this off takes the games that aren't installed out of the library.\n"
               "toggle Import the whole library\nwait 1000\n"
               "expect Turning this on adds the games you own that aren't installed.\n"
               "expect-not No Epic games yet. Sign in to Epic in Heroic to see them here.\n",
               None)],
             fake_heroic),
]

sys.exit(run_scenarios(SCENARIOS))
