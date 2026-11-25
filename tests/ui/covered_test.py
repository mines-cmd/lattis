import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# A page that covers the library: its key and a text that is on screen only while it is open.
PAGES = {
    "the Library overview": ("F7", "Library overview"),
    "Play statistics": ("F9", "Sessions appear here once a game is played."),
    "the Gallery": ("F8", "Back to games"),
}
SELECT = "type Marker Game\nwait 400\nclick Marker Game\nwait 300\n"


def marker_path(dirs):
    return os.path.join(dirs.root, "marker")


def games(dirs, grid=False):
    """The demo library plus a Linux game whose program touches a marker file and exits."""
    dirs.use_demo()
    if grid:
        with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
            f.write('\n[general]\nview = "grid"\n')
    program = os.path.join(dirs.root, "games", "marker.sh")
    os.makedirs(os.path.dirname(program), exist_ok=True)
    with open(program, "w") as f:
        f.write(f"#!/bin/sh\ntouch {marker_path(dirs)}\n")
    os.chmod(program, 0o755)
    dirs.write("library/games/linux-marker.toml",
               f'title = "Marker Game"\nplatform = "linux"\nexe = "{program}"\ninstall_dir = "{dirs.root}/games"\n'
               "added = 2026-01-01\n")


def game_file(dirs):
    with open(os.path.join(dirs.data, "library", "games", "linux-marker.toml")) as f:
        return f.read()


def launched(expected):
    def check(result, dirs):
        has = os.path.exists(marker_path(dirs))
        if has != expected:
            return ["the game " + ("didn't start" if expected else "started under a page that covers the library")]
    return check


def favorite(expected):
    def check(result, dirs):
        has = "favorite = true" in game_file(dirs)
        if has != expected:
            return ["the game " + ("wasn't made a favorite" if expected else "was made a favorite under a page that covers the library")]
    return check


def scenarios():
    found = [
        Scenario("control: Return on the list launches the selected game",
                 [(SELECT + "key Return\nwait 1500\n", launched(True))], games),
        Scenario("control: Ctrl+D on the list makes the selected game a favorite",
                 [(SELECT + "key Ctrl+D\nwait 600\n", favorite(True))], games),
        Scenario("control: a page opened and closed with no key changes nothing",
                 [(SELECT + "key F7\nexpect Library overview\nwait 1200\nkey F7\nwait 600\n"
                   "expect Marker Game\n", lambda r, d: launched(False)(r, d) or favorite(False)(r, d))], games),
    ]
    for page, (key, shown) in PAGES.items():
        covered = f"key {key}\nexpect {shown}\nwait 1200\n"
        found += [
            Scenario(f"Return under {page} doesn't launch the game that was selected in the list",
                     [(SELECT + covered + "key Return\nwait 1500\n" + f"expect {shown}\n", launched(False))], games),
            Scenario(f"Ctrl+D under {page} doesn't flip the favorite of the hidden game",
                     [(SELECT + covered + "key Ctrl+D\nwait 600\n" + f"expect {shown}\n", favorite(False))], games),
        ]
    found += [
        Scenario("navigation keys under the overview in Grid view do nothing and warn about nothing",
                 [("expect Hades\nkey F7\nexpect Library overview\nwait 1200\nkey Down\nkey End\nkey Home\n"
                   "key Ctrl+A\nwait 300\nexpect Library overview\nkey F7\nwait 600\nexpect Hades\n", None)],
                 lambda dirs: games(dirs, grid=True)),
        Scenario("Return under the overview in Grid view doesn't launch the selected game",
                 [("expect Hades\ntype Marker Game\nwait 400\nclick Marker Game\nwait 300\n"
                   "key F7\nexpect Library overview\nwait 1200\nkey Return\nwait 1500\nexpect Library overview\n",
                   launched(False))],
                 lambda dirs: games(dirs, grid=True)),
    ]
    return found


sys.exit(run_scenarios(scenarios()))
