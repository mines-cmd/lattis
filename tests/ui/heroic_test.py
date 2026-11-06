import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import ROOT, Scenario, run_scenarios

FAKE = os.path.join(ROOT, "tests", "data", "heroic")
OPEN_SETTINGS = "key Ctrl+Comma\nexpect General\nwait 300\nclick Heroic\nwait 500\n"

# Stands in for Heroic: logs its arguments, and for a launch closes the game at once, as a Heroic that
# was closed does. Its total grows by 10 minutes.
FAKE_HEROIC = """#!/bin/sh
echo "$*" >> "{log}"
case "$*" in
*appName=HikeApp*) cat > "{timestamp}" <<'EOF'
{played}
EOF
;;
esac
"""
PLAYED = {"HikeApp": {"firstPlayed": "2026-10-01T18:00:00.000Z", "lastPlayed": "2026-11-03T10:00:00.000Z",
                      "totalPlayed": 135}}


def heroic_log(dirs):
    return os.path.join(dirs.root, "heroic.txt")


def demo_with_heroic(dirs):
    dirs.use_demo()
    root = os.path.join(dirs.root, "heroic")
    shutil.copytree(FAKE, root)
    with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
        f.write(f'\n[heroic]\npath = "{root}"\n')
    program = os.path.join(dirs.bin, "heroic")
    with open(program, "w") as f:
        f.write(FAKE_HEROIC.format(log=heroic_log(dirs), timestamp=os.path.join(root, "store", "timestamp.json"),
                                   played=json.dumps(PLAYED)))
    os.chmod(program, 0o755)


def heroic_with_lookup(dirs):
    demo_with_heroic(dirs)
    dirs.env["LATTIS_METADATA_FIXTURES"] = os.path.join(ROOT, "tests", "data", "metadata")


def select(title):
    return f"type {title}\nwait 400\nclick {title}\nwait 300\n"


def select_hidden(title):
    return f"type {title}\nwait 400\nclick Show not installed\nwait 400\nclick {title}\nwait 300\n"


def called_with(*expected):
    def check(result, dirs):
        calls = open(heroic_log(dirs)).read().splitlines() if os.path.exists(heroic_log(dirs)) else []
        if calls != list(expected):
            return [f"heroic was called with {calls}"]
    return check


def covers(result, dirs):
    art = os.path.join(dirs.data, "art")
    found = []
    for path in ("epic-a-short-hike/cover.jpg", "epic-fez/cover.png"):
        if not os.path.exists(os.path.join(art, path)):
            found.append(f"{path} wasn't copied from images-cache")
    if os.path.exists(os.path.join(art, "epic-rime")):
        found.append("a game Heroic has no image of has an art folder")
    return found


def game_file(dirs, name):
    return open(os.path.join(dirs.data, "library", "games", f"{name}.toml")).read()


def adds_the_growth(result, dirs):
    if "play_minutes = 135" not in game_file(dirs, "epic-a-short-hike"):
        return ["Heroic's 10 new minutes weren't added: " + game_file(dirs, "epic-a-short-hike")]


def settings_has(text):
    def check(result, dirs):
        if text not in open(os.path.join(dirs.data, "settings.toml")).read():
            return [f"settings.toml has no {text}"]
    return check


SCENARIOS = [
    Scenario("the Epic games are listed under their platform, and the covers Heroic has are copied",
             [("expect EPIC\nexpect A Short Hike\nexpect-not Fez\n", covers)], demo_with_heroic),
    Scenario("Launch hands the game to Heroic, and its new minutes are counted when it closes",
             [(select("A Short Hike") + "click Launch\nwait 2500\nexpect Launch\nexpect-not Close\n",
               lambda r, d: (called_with("--no-gui heroic://launch?appName=HikeApp&runner=legendary&gui=false")(r, d) or [])
               + (adds_the_growth(r, d) or []))], demo_with_heroic),
    Scenario("a game that isn't installed has Install, which opens Heroic and doesn't start a session",
             [(select_hidden("Fez") + "expect Install\nexpect-not Launch\nclick Install\nwait 800\nexpect Install Fez in Heroic\n"
               "expect-not Close\n",
               called_with(""))], demo_with_heroic),
    Scenario("Properties shows where it's installed and Open Heroic, with no launch settings",
             [(select("A Short Hike") + "key Alt+Return\nexpect Properties\nclick Launch\nwait 300\n"
               "expect-has /games/AShortHike\nexpect Open Heroic\nexpect-not Proton version\nexpect-not Launch options\n"
               "click Open Heroic\nwait 500\nclick Cancel\nexpect-not Properties\n", called_with(""))],
             demo_with_heroic),
    Scenario("play time can be edited in Properties",
             [(select("A Short Hike") + "key Alt+Return\nexpect Properties\nexpect-not from Steam\nclick 2\nwait 200\n"
               "type 9\nkey Tab\nclick Save\nwait 800\nexpect-not Properties\n",
               lambda r, d: None if "play_minutes = 545" in game_file(d, "epic-a-short-hike")
               else ["the edit wasn't saved: " + game_file(d, "epic-a-short-hike")])], demo_with_heroic),
    Scenario("Settings → Heroic shows what it found, and the whole library can be turned off and back",
             [(OPEN_SETTINGS + "expect-has 3 Epic game\nexpect-on Import Epic games\nexpect-on Import the whole library\n"
               "toggle Import the whole library\nwait 1000\nexpect-has 1 Epic game\nexpect-off Import the whole library\n"
               "wait 600\n", settings_has("whole_library = false")),
              (OPEN_SETTINGS + "expect-off Import the whole library\ntoggle Import the whole library\nwait 1000\n"
               "expect-has 3 Epic game\nwait 600\n", None)], demo_with_heroic),
    Scenario("a toast counts the new games while they're looked up, and goes when it's done",
             [("expect-has Looking up new games\nwait 4000\nexpect-not-has Looking up new games\n", None)],
             heroic_with_lookup),
    Scenario("a folder without Heroic's files says so",
             [(OPEN_SETTINGS + "expect-has There's no Heroic in\nexpect Find automatically\nexpect-not EPIC\n", None)],
             lambda dirs: (dirs.use_demo(), dirs.write("settings.toml", '[steam]\npath = "steam"\n\n[heroic]\npath = "nothing"\n'))),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
