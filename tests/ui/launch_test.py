import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# Linux games whose program is a script the scenario writes: it sleeps, exits at once, or fails at once.
PROGRAMS = {
    "sleeper": ("Sleeper Game", "#!/bin/sh\nsleep 1234\n"),
    "instant": ("Instant Game", "#!/bin/sh\nexit 0\n"),
    "failing": ("Failing Game", "#!/bin/sh\necho boom >&2\nexit 3\n"),
}
LAUNCHER = ("Launcher Game", "#!/bin/sh\nsleep 1234 &\nexit 0\n")


def games(dirs, min_session_seconds=0):
    """The demo library plus one Linux game per program; a session of any length counts unless asked."""
    dirs.use_demo()
    dirs.write("settings.toml",
               open(os.path.join(dirs.data, "settings.toml")).read()
               + f"\n[launch]\nmin_session_seconds = {min_session_seconds}\n")
    programs = dict(PROGRAMS, launcher=LAUNCHER)
    for name, (title, body) in programs.items():
        program = os.path.join(dirs.root, "games", name + ".sh")
        dirs.write(f"library/games/linux-{name}.toml",
                   f'title = "{title}"\nplatform = "linux"\nexe = "{program}"\ninstall_dir = "{dirs.root}/games"\n'
                   "added = 2026-01-01\n")
        os.makedirs(os.path.dirname(program), exist_ok=True)
        with open(program, "w") as f:
            f.write(body)
        os.chmod(program, 0o755)
    dirs.write("library/games/linux-missing.toml",
               f'title = "Missing Game"\nplatform = "linux"\nexe = "{dirs.root}/games/nothing.sh"\n'
               f'install_dir = "{dirs.root}/games"\nadded = 2026-01-01\n')
    dirs.write("library/games/linux-plain.toml",
               f'title = "Plain Game"\nplatform = "linux"\nexe = "{dirs.root}/games/plain.sh"\n'
               f'install_dir = "{dirs.root}/games"\nadded = 2026-01-01\n')
    with open(os.path.join(dirs.root, "games", "plain.sh"), "w") as f:
        f.write("#!/bin/sh\nsleep 1234\n")


def game_file(dirs, name):
    with open(os.path.join(dirs.data, "library", "games", f"linux-{name}.toml")) as f:
        return f.read()


def history_exists(dirs, name):
    return os.path.exists(os.path.join(dirs.data, "library", "history", f"linux-{name}.toml"))


def has_last_played(name, expected=True):
    def check(result, dirs):
        has = "last_played" in game_file(dirs, name)
        if has != expected:
            return [f"{name}: last_played {'missing' if expected else 'written'} in its game file"]
    return check


def plays(name, minutes):
    def check(result, dirs):
        found = []
        if f"play_minutes = {minutes}" not in game_file(dirs, name) and not (minutes == 0 and "play_minutes" not in game_file(dirs, name)):
            found.append(f"{name}: play_minutes isn't {minutes}")
        return found
    return check


def select(title):
    return f"type {title}\nwait 400\nclick {title}\nwait 300\n"


def select_hidden(title):
    return f"type {title}\nwait 400\nclick Show not installed\nwait 400\nclick {title}\nwait 300\n"


def launch_and_close(title):
    return (select(title) + "click Launch\nexpect Close\nwait 500\nclick Close\nexpect Force close\n"
            "click Force close\nwait 800\nexpect Launch\nexpect-not Close\nexpect-not-has closed right away\nwait 800\n")


def leave_no_processes(result, dirs):
    stray = os.popen("pgrep -f 'sleep 1234'").read().split()
    for pid in stray:
        os.kill(int(pid), 9)
    return [f"a game's process was left running: {' '.join(stray)}"] if stray else []


SCENARIOS = [
    Scenario("Launch starts a program, the button reads Close, force close stops it",
             [(launch_and_close("Sleeper Game") + "expect Sleeper Game\n", leave_no_processes)], games),
    Scenario("a game that ran under min_session_seconds sets last played, but adds no minutes or history",
             [(launch_and_close("Sleeper Game"),
               lambda r, d: (has_last_played("sleeper")(r, d) or []) + (plays("sleeper", 0)(r, d) or [])
               + ([] if not history_exists(d, "sleeper") else ["a session of 0 minutes was put in the history"]))],
             games),
    Scenario("a session shorter than the minimum is not counted at all",
             [(launch_and_close("Sleeper Game"),
               lambda r, d: (has_last_played("sleeper", False)(r, d) or []) + ([] if not history_exists(d, "sleeper")
                                                                              else ["history written"]))],
             lambda dirs: games(dirs, min_session_seconds=600)),
    Scenario("a program that exits at once with success ends its session and the button is Launch again",
             [(select("Instant Game") + "click Launch\nwait 1500\nexpect Launch\nexpect-not Close\n"
               "expect-not-has closed right away\n", None)], games),
    Scenario("a program that fails at once says it closed right away and is launchable again",
             [(select("Failing Game") + "click Launch\nexpect-has closed right away\nwait 800\nexpect Launch\n"
               "expect-not Close\n", None)], games),
    Scenario("a launcher script that exits while its game runs keeps the button on Close",
             [(select("Launcher Game") + "click Launch\nwait 2500\nexpect Close\nclick Close\n"
               "expect Force close\nclick Force close\nwait 1500\nexpect Launch\n", leave_no_processes)], games),
    Scenario("a game whose program is gone is listed as not installed, with a Game not found button",
             [(select_hidden("Missing Game") + "expect Game not found\nexpect-not Close\nexpect-not Launch\n", None)],
             games),
    Scenario("a program that isn't executable counts as not installed too",
             [(select_hidden("Plain Game") + "expect Game not found\nexpect-not Close\n", None)],
             lambda dirs: (games(dirs), os.chmod(os.path.join(dirs.root, "games", "plain.sh"), 0o644))),
    Scenario("Return launches the selected game, Escape on the force close prompt keeps it running",
             [(select("Sleeper Game") + "key Return\nexpect Close\nwait 500\nclick Close\nexpect Force close\n"
               "key Escape\nwait 400\nexpect-not Force close\nexpect Close\nclick Close\nclick Force close\n"
               "wait 800\nexpect Launch\n", leave_no_processes)], games),
    Scenario("a session over the minimum adds a minute and a history entry that survive a restart",
             [(select("Sleeper Game") + "click Launch\nexpect Close\nwait 9000\nwait 9000\nwait 9000\nwait 6000\n"
               "click Close\nclick Force close\nwait 1000\nexpect Launch\n",
               lambda r, d: [] if history_exists(d, "sleeper") and "play_minutes = 1" in game_file(d, "sleeper")
               else ["no history file or play_minutes = 1 after a 33 s session"]),
              (select("Sleeper Game") + "expect Launch\n", leave_no_processes)],
             games),
]

sys.exit(run_scenarios(SCENARIOS))
