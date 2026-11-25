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
    dirs.env.update({"LD_PRELOAD": library, "TZ": "UTC", "CLOCKSHIM_START": str(NOVEMBER_30),
                     "CLOCKSHIM_AFTER": str(JUMP_AFTER), "CLOCKSHIM_JUMP": str(DAY)})





SCENARIOS = [
    Scenario("Play statistics opened again after midnight is on the new day's month",
             [("wait 1500\nkey F9\nwait 1500\nexpect November 2026\nkey Escape\nwait 300\n"
               + f"wait {JUMP_AFTER * 1000}\nkey F9\nwait 1500\nexpect December 2026\nexpect-not November 2026\n", None)],
             setup),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
