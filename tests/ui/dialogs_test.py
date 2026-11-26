import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios
import steamfake

# The shared dialog base (AppDialog): a small dialog (New collection) and a medium one (Play history) each show
# their title, and Escape closes each.

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN_COLL = "key Ctrl+Shift+C\nwait 600\nexpect COLLECTIONS\n"
HISTORY = "type Mario Tennis Aces\nwait 400\nclick Show not installed\nwait 400\nclick Mario Tennis Aces\nwait 300\nkey Ctrl+Alt+4\nwait 500\n"


def setup(dirs):
    dirs.use_demo()
    settings = os.path.join(dirs.data, "settings.toml")
    with open(settings) as f:
        text = f.read()
    dirs.write("settings.toml", text + '\n[shortcuts]\nplay_history = "Ctrl+Alt+4"\n')


SLEEP = "sleep 4321"
RUN_SLEEPER = "type Sleeper Game\nwait 400\nclick Sleeper Game\nwait 300\nclick Launch\nexpect Close\nwait 500\n"


def sleeper(dirs):
    """The demo plus a Linux game whose program sleeps, so it runs until the scenario ends it."""
    dirs.use_demo()
    program = os.path.join(dirs.root, "games", "sleeper.sh")
    os.makedirs(os.path.dirname(program))
    with open(program, "w") as f:
        f.write(f"#!/bin/sh\n{SLEEP}\n")
    os.chmod(program, 0o755)
    dirs.write("library/games/linux-sleeper.toml",
               f'title = "Sleeper Game"\nplatform = "linux"\nexe = "{program}"\ninstall_dir = "{dirs.root}/games"\n'
               "added = 2026-01-01\n")


def sleeper_running(expected):
    """Whether the game outlived the app; ends it either way."""
    def check(result, dirs):
        stray = os.popen(f"pgrep -f '{SLEEP}'").read().split()
        for pid in stray:
            os.kill(int(pid), 9)
        if bool(stray) != expected:
            return ["the game " + ("was stopped by quitting" if expected else "was left running")]
    return check


def last_command(line):
    def check(result, dirs):
        commands = [out for out in result.output.splitlines() if out.startswith("uitest:")]
        if not commands or commands[-1] != line:
            return [f"the app didn't quit right after '{line}' (last: {commands[-1] if commands else 'none'})"]
    return check


def both(*checks):
    def check(result, dirs):
        return [problem for c in checks for problem in (c(result, dirs) or [])]
    return check


def demo_file(name):
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "demo-data", "library",
                           "games", name)) as f:
        return f.read()


def steam_waiting(dirs):
    dirs.use_demo()
    procs.append(steamfake.start_steam(dirs.data, dirs.home))


procs = []


def stop_steam(check):
    def wrapped(result, dirs):
        for proc in procs:
            proc.kill()
            proc.wait()
        procs.clear()
        return check(result, dirs)
    return wrapped


def undone(kept, gone):
    """The games in `gone` took Steam's values back; those in `kept` are as the demo has them."""
    def check(result, dirs):
        found = []
        for name in kept:
            if read_game(dirs, name) != demo_file(name):
                found.append(f"{name} changed though it wasn't ticked")
        for name, line in gone.items():
            if line in read_game(dirs, name):
                found.append(f"{name} still has {line}")
        return found
    return check


def still_waits(result, dirs):
    if 'launch_options = "-novid"' not in read_game(dirs, HADES):
        return ["Hades lost its waiting launch options though nothing was undone"]


def read_game(dirs, name):
    with open(os.path.join(dirs.data, "library", "games", name)) as f:
        return f.read()


# Steam is running (the fake client), so a launch option edited in Properties waits for it to close. The
# notice's Undo is found with click-first: the toast after Save has an Undo of its own, drawn later.
HADES_WAITS = ("type Hades\nwait 400\nclick Hades\nwait 300\nkey Alt+Return\nexpect Properties\nclick Launch\nwait 400\n"
               "click None\ntype -novid\nclick Save\nwait 1500\nclick-first Undo\nwait 600\n"
               "expect Undo the changes waiting for Steam?\nexpect 3 of 3 selected\n")
DIABLO, FFXIV, HADES = "proton-diablo-iv.toml", "proton-final-fantasy-xiv-online.toml", "steam-1145360.toml"


SCENARIOS = [
    Scenario("Ctrl+Q with nothing running quits at once, with no question",
             [("expect Hades\nkey Ctrl+Q\nwait 2000\nexpect Quit Lattis?\n", last_command("uitest: key Ctrl+Q"))],
             sleeper, quits=True),
    Scenario("Ctrl+Q while a game runs asks first; Cancel keeps the app and the game",
             [(RUN_SLEEPER + "key Ctrl+Q\nwait 500\nexpect Quit Lattis?\n"
               + "expect Sleeper Game is still running. It keeps running if you quit, but its play time isn't recorded.\n"
               + "click Cancel\nwait 500\nexpect-not Quit Lattis?\nexpect Close\nkey Escape\nwait 300\nkey Ctrl+Q\n"
               + "wait 500\nexpect Quit Lattis?\nkey Escape\nwait 500\nexpect-not Quit Lattis?\nexpect Close\n",
               sleeper_running(True))], sleeper),
    Scenario("Quit in the quit prompt quits and leaves the game running",
             [(RUN_SLEEPER + "key Ctrl+Q\nwait 500\nexpect Quit Lattis?\nclick Quit\nwait 2000\nexpect NOT REACHED\n",
               both(last_command("uitest: click Quit"), sleeper_running(True)))], sleeper, quits=True),
    Scenario("the waiting-changes prompt: Cancel changes nothing",
             [(HADES_WAITS + "click Cancel\nwait 600\nexpect-not Undo the changes waiting for Steam?\n",
               stop_steam(both(undone([DIABLO, FFXIV], {}), still_waits)))], steam_waiting),
    Scenario("the waiting-changes prompt undoes only the ticked games",
             [(HADES_WAITS + "click Diablo IV\nclick Final Fantasy XIV Online\nwait 300\nexpect 1 of 3 selected\n"
               + "click Undo\nwait 800\nexpect-not Undo the changes waiting for Steam?\n",
               stop_steam(undone([DIABLO, FFXIV], {HADES: 'launch_options = "-novid"'})))], steam_waiting),
]

SCENARIOS += [
    Scenario("a small dialog shows its title and its footer, and Escape closes it",
             [(FIRST + OPEN_COLL + "click New collection\nwait 400\nexpect Name of the collection\nexpect-has New collection\n"
               + "expect Cancel\nkey Escape\nwait 500\nexpect-not Name of the collection\n", None)], setup),
    Scenario("a medium dialog shows its title and its footer, and Escape closes it",
             [(HISTORY + "expect Total\nexpect Play history\nexpect Close\nkey Escape\nwait 500\nexpect-not Total\n", None)],
             setup),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
