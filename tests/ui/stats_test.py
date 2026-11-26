import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The launcher stays open over midnight: the page must read the date again each time it opens. The
# clock is moved with an LD_PRELOAD shim (clockshim.c, built here): 2026-11-30 12:00 UTC at the start,
# one day later after 4 s of real time. TZ=UTC keeps the day the same everywhere.
SHIM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "clockshim.c")
NOVEMBER_30 = 1796040000
DAY = 86400
JUMP_AFTER = 4


def setup(dirs):
    dirs.use_demo()
    program = os.path.join(dirs.root, "games", "played.sh")
    os.makedirs(os.path.dirname(program), exist_ok=True)
    with open(program, "w") as f:
        f.write("#!/bin/sh\nsleep 1234\n")
    os.chmod(program, 0o755)
    dirs.write("library/games/linux-played.toml",
               f'title = "Played Game"\nplatform = "linux"\nexe = "{program}"\ninstall_dir = "{dirs.root}/games"\n'
               "added = 2026-01-01\n")
    dirs.write("library/history/linux-played.toml",
               '[[session]]\nend = 2026-11-20T21:00:00+00:00\nminutes = 90\n')
    library = os.path.join(dirs.root, "libclockshim.so")
    subprocess.run(["cc", "-shared", "-fPIC", "-O1", "-o", library, SHIM, "-ldl"], check=True)
    dirs.env.update({"LD_PRELOAD": library, "ASAN_OPTIONS": "verify_asan_link_order=0:detect_leaks=0", "TZ": "UTC", "CLOCKSHIM_START": str(NOVEMBER_30),
                     "CLOCKSHIM_AFTER": str(JUMP_AFTER), "CLOCKSHIM_JUMP": str(DAY)})


def history(dirs):
    """The clock held at 30 November 2026; Played Game has sessions in November and October 2026 and in 2025,
    Old Game one in March 2024 only."""
    setup(dirs)
    dirs.env["CLOCKSHIM_JUMP"] = "0"
    dirs.write("library/history/linux-played.toml",
               "[[session]]\nend = 2025-06-10T21:00:00+00:00\nminutes = 120\n"
               "[[session]]\nend = 2026-10-05T21:00:00+00:00\nminutes = 60\n"
               "[[session]]\nend = 2026-11-20T21:00:00+00:00\nminutes = 90\n")
    dirs.write("library/games/linux-old.toml",
               f'title = "Old Game"\nplatform = "linux"\nexe = "/bin/true"\ninstall_dir = "{dirs.root}/games"\n'
               "added = 2024-01-01\n")
    dirs.write("library/history/linux-old.toml", "[[session]]\nend = 2024-03-03T21:00:00+00:00\nminutes = 45\n")


def game_file(dirs, name):
    with open(os.path.join(dirs.data, "library", "games", name)) as f:
        return f.read()


def favorite_only(name, others=()):
    def check(result, dirs):
        found = []
        if "favorite = true" not in game_file(dirs, name):
            found.append(f"{name} isn't a favorite: the tile didn't select its game")
        found += [f"{other} became a favorite" for other in others if "favorite = true" in game_file(dirs, other)]
        return found
    return check


OPEN = "wait 1500\nkey F9\nwait 1500\nexpect November 2026\n"
YEAR = "click Year\nwait 600\nexpect 2026\n"


SCENARIOS = [
    Scenario("Play statistics: the arrows and the keys step through the months, up to this one",
             [(OPEN + "expect Played Game\nexpect-not Old Game\nkey Left\nexpect October 2026\nexpect Played Game\n"
               + "click Previous month\nexpect September 2026\nexpect-not Played Game\nclick Next month\n"
               + "expect October 2026\nkey Right\nexpect November 2026\nkey Right\nwait 400\nexpect November 2026\n"
               + "expect-not December 2026\n", None)], history),
    Scenario("Play statistics: Year shows a year, the arrows step through the years with history",
             [(OPEN + YEAR + "expect-not November 2026\nexpect Played Game\nexpect-not Old Game\nkey Left\n"
               + "expect 2025\nexpect Played Game\nclick Previous year\nexpect 2024\nexpect Old Game\n"
               + "expect-not Played Game\nkey Left\nwait 400\nexpect 2024\nexpect-not 2023\nclick Next year\n"
               + "expect 2025\nclick Month\nwait 600\nexpect December 2025\n", None)], history),
    Scenario("Play statistics: the year picker lists the years with history and opens the one picked",
             [(OPEN + YEAR + "click 2026\nwait 500\nexpect 2025\nexpect 2024\nexpect-not 2023\nclick 2024\nwait 600\n"
               + "expect In 2024\nexpect Old Game\nexpect-not Played Game\n", None)], history),
    Scenario("Play statistics: the month picker opens a month, and All of a year opens the year",
             [(OPEN + "click November 2026\nwait 500\nexpect Oct\nclick Oct\nwait 600\nexpect October 2026\n"
               + "click October 2026\nwait 500\nclick All of 2026\nwait 600\nexpect 2026\nexpect-not October 2026\n"
               + "expect Played Game\n", None)], history,
             known_bugs=[("Member rotation of the object PageButton overrides",
                          "StatsPeriodPicker's PageButton declares its own rotation")]),
    Scenario("Play statistics: a Most played tile opens its game in the library",
             [(OPEN + "click Played Game\nwait 1000\nexpect-not November 2026\nexpect Played Game\nkey Ctrl+D\nwait 600\n",
               favorite_only("linux-played.toml", ("linux-old.toml", "switch-mario-kart-8-deluxe.toml")))], history),
]

SCENARIOS += [
    Scenario("Play statistics opened again after midnight is on the new day's month",
             [("wait 1500\nkey F9\nwait 1500\nexpect November 2026\nkey Escape\nwait 300\n"
               + f"wait {JUMP_AFTER * 1000}\nkey F9\nwait 1500\nexpect December 2026\nexpect-not November 2026\n", None)],
             setup),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
