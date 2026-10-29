import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

GUIDE = "expect Skip setup\nexpect Set up a new library\n"
LIBRARY = "expect Hades\nexpect Animal Crossing: New Horizons\nexpect-not Skip setup\n"
# The demo's Steam games come from its settings.toml, its ROM games from platforms.toml.
ROM_GAMES = "expect Animal Crossing: New Horizons\nexpect-not Skip setup\n"
STEAM_GAMES = "expect Hades\nexpect-not Skip setup\n"
READ_ONLY = LIBRARY + "expect-has Read-only:\n"
GARBAGE = b"\x00\xff\xfe not toml = = [[[ \x01\n"


def empty(dirs):
    pass


def demo(dirs):
    dirs.use_demo()


def demo_with(path, content):
    def setup(dirs):
        dirs.use_demo()
        dirs.write(path, content)
    return setup


def demo_without(*paths):
    def setup(dirs):
        dirs.use_demo()
        for path in paths:
            os.remove(os.path.join(dirs.data, path))
    return setup


def demo_with_every(content, *paths):
    def setup(dirs):
        dirs.use_demo()
        for path in paths:
            dirs.write(path, content)
    return setup


GAME = "library/games/switch-tetris.toml"

SCENARIOS = [
    Scenario("empty data dir shows the setup guide", [(GUIDE, None)], empty),
    Scenario("demo library shows the games, no guide", [(LIBRARY, None)], demo),
    Scenario("garbage settings.toml", [(ROM_GAMES, None)], demo_with("settings.toml", GARBAGE)),
    Scenario("zero-byte settings.toml", [(ROM_GAMES, None)], demo_with("settings.toml", "")),
    Scenario("garbage game file", [(LIBRARY, None)], demo_with(GAME, GARBAGE)),
    Scenario("zero-byte game file", [(LIBRARY, None)], demo_with(GAME, "")),
    Scenario("garbage library.toml opens read-only", [(READ_ONLY, None)],
             demo_with("library/library.toml", GARBAGE)),
    Scenario("zero-byte library.toml", [("expect Hades\n", None)], demo_with("library/library.toml", "")),
    Scenario("newer library version opens read-only", [(READ_ONLY, None)],
             demo_with("library/library.toml", "version = 999\n")),
    Scenario("garbage platforms.toml, tags.toml, steam.toml", [(STEAM_GAMES, None)],
             demo_with_every(GARBAGE, "library/platforms.toml", "library/tags.toml", "library/steam.toml")),
    Scenario("missing platforms.toml, tags.toml, steam.toml", [(STEAM_GAMES, None)],
             demo_without("library/platforms.toml", "library/tags.toml", "library/steam.toml")),
    Scenario("garbage emulators.toml and credentials.toml", [(LIBRARY, None)],
             demo_with_every(GARBAGE, "library/emulators.toml", "credentials.toml")),
    Scenario("garbage theme file", [(LIBRARY, None)],
             demo_with_every(GARBAGE, "themes/broken.toml", "theme.toml", "themes/default.toml")),
    Scenario("two starts in a row on one data dir", [(LIBRARY, None), (LIBRARY, None)], demo),
]

sys.exit(run_scenarios(SCENARIOS))
