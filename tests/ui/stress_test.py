import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# library_bench's generator (tests/library_bench.cpp generateFiles), in Python: 1000 game files over
# four platforms, random titles, tags, favorites, play time. The loose time bounds are the driver's: each
# command retries for 2 s and the whole script may take 60 s, on software rendering beside other tests.
WORDS = ["Shadow", "Legend", "Crystal", "Iron", "Star", "Ancient", "Last", "Dark", "Tales", "Kingdom", "Dragon",
         "Night", "Lost", "Eternal", "Fallen", "Hollow", "Rising", "Sky", "Storm", "Blade", "Knight", "Quest",
         "Chronicles", "Echoes", "Frontier", "Wild", "Neon", "Pixel", "Garden", "Ocean", "Void", "Ember"]
TAGS = ["Adventure", "Racing", "Party", "Platformer", "Co-op", "MMO", "JRPG",
        "RPG", "Roguelike", "Metroidvania", "Puzzle", "Strategy"]
PLATFORMS = ["switch", "3ds", "steam", "proton"]

JAPANESE = "ゼルダの伝説 ブレスオブザワイルド"
ACCENTS = "Ünïcödé Ñandú Çrème Brûlée"
EMOJI = "🎮 Pixel Party 🎉"
LONG = "Extremely " + "Long Winded Title Of Many Words " * 14 + "The End"
FIRST = "AAA First Game"
LAST = "ZZZ Last Game"
NEEDLE = "Zyxwv Needle"


def toml_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def library(dirs, special=()):
    rng = random.Random(1)
    dirs.write("library/library.toml", "version = 1\n")
    dirs.write("library/platforms.toml", "".join(
        f'[[platform]]\nid = "{p}"\nname = "{p.capitalize()}"\nicon = "{p}"\n\n' for p in PLATFORMS))
    dirs.write("library/tags.toml", "".join(
        f'[[tag]]\nname = "{t}"\ncolor = {i % 8}\n\n' for i, t in enumerate(TAGS)))
    titles = [FIRST, LAST, NEEDLE] + list(special)
    special_count = len(titles)
    while len(titles) < 1000:
        words = [rng.choice(WORDS) for _ in range(2 + rng.randrange(3))]
        if rng.randrange(3) == 0:
            words.append(str(2 + rng.randrange(4)))
        titles.append(" ".join(words))
    for i, title in enumerate(titles):
        platform = PLATFORMS[i % len(PLATFORMS)] if i >= special_count else ("3ds" if i == 1 else "switch")
        game_id = f"{platform}-game-{i}"
        text = f"title = {toml_string(title)}\nplatform = \"{platform}\"\nyear = {1990 + rng.randrange(36)}\n"
        text += 'description = "A plausible paragraph about this game."\n'
        text += f"rating = {rng.randrange(50) / 10:.1f}\n"
        text += "tags = [" + ", ".join(f'"{rng.choice(TAGS)}"' for _ in range(1 + rng.randrange(3))) + "]\n"
        if rng.randrange(5) == 0:
            text += "favorite = true\n"
        if platform in ("switch", "3ds"):
            text += f"play_minutes = {rng.randrange(20000)}\n"
            text += f"last_played = 2026-{1 + rng.randrange(9):02d}-{1 + rng.randrange(28):02d}T21:{rng.randrange(60):02d}:00-03:00\n"
        text += "added = 2026-01-12\n"
        if platform == "proton":
            text += (f'exe = "~/Games/{game_id}/game.exe"\ninstall_dir = "~/Games/{game_id}"\n'
                     f"shortcut = {3000000000 + i}\n")
        dirs.write(f"library/games/{game_id}.toml", text)


def plain(dirs):
    library(dirs)


def odd_titles(dirs):
    library(dirs, [JAPANESE, ACCENTS, EMOJI, LONG])


def empty_title(dirs):
    library(dirs, ["Keeps Company"])
    dirs.write("library/games/switch-game-empty.toml",
               'title = ""\nplatform = "switch"\nadded = 2026-01-12\n')


def search(text):
    return "type " + text + "\nwait 600\n"


SCENARIOS = [
    Scenario("a 1000-game library starts, and Home and End reach the first and last game",
             [("expect " + FIRST + "\nkey End\nwait 600\nexpect " + LAST + "\nexpect-not " + FIRST
               + "\nkey Home\nwait 600\nexpect " + FIRST + "\nexpect-not " + LAST + "\n", None)], plain),
    Scenario("holding Down and Page Down through the list stays responsive",
             [("expect " + FIRST + "\n" + "key Down\n" * 40 + "key PageDown\n" * 5 + "key PageUp\n" * 5
               + "key Home\nwait 600\nexpect " + FIRST + "\n", None)], plain),
    Scenario("a search finds the one game among 1000, and clearing it brings the list back",
             [("expect " + FIRST + "\n" + search("zyxwv") + "expect " + NEEDLE + "\nexpect-not " + FIRST
               + "\nkey Escape\nkey Escape\nkey Home\nwait 600\nexpect " + FIRST + "\n", None)], plain),
    Scenario("typing and deleting a long search keeps up",
             [("expect " + FIRST + "\n" + search("shadow legend crystal") + "key Backspace\n" * 12
               + "wait 600\nexpect-not " + FIRST + "\nkey Escape\nkey Escape\nkey Home\nwait 600\nexpect " + FIRST + "\n",
               None)], plain),
    Scenario("Ctrl+A selects all 1000 games, and Escape clears it",
             [("expect " + FIRST + "\nkey Ctrl+A\nexpect-has selected\nkey End\nwait 600\nexpect " + LAST
               + "\nkey Escape\nexpect-not-has selected\n", None)], plain),
    Scenario("every sort order lists 1000 games, and the title order comes back",
             [("expect " + FIRST + "\nkey Ctrl+Shift+S\nexpect Sorted by last played\nwait 3500\n"
               + "key Ctrl+Shift+S\nexpect Sorted by play time\nwait 3500\n"
               + "key Ctrl+Shift+S\nexpect Sorted by title\nwait 600\nexpect " + FIRST + "\n", None)], plain),
    Scenario("a platform filter and a search narrow 1000 games",
             [("expect " + FIRST + "\nkey Ctrl+2\nwait 600\n" + search("zyxwv") + "expect " + NEEDLE
               + "\nkey Ctrl+3\nwait 600\nexpect-not " + NEEDLE + "\nkey Ctrl+1\nwait 600\nexpect " + NEEDLE + "\n",
               None)], plain),
    Scenario("Japanese, accented and emoji titles show and can be searched",
             [("expect " + FIRST + "\nkey End\nwait 600\nexpect " + LAST + "\n"
               + search("Brûlée") + "expect " + ACCENTS + "\nexpect-not " + FIRST + "\nkey Escape\nkey Escape\n"
               + "wait 600\n" + search("ゼルダ") + "expect " + JAPANESE + "\nexpect-not " + ACCENTS + "\n"
               + "key Escape\nkey Escape\nwait 600\n" + search("🎮") + "expect " + EMOJI + "\n", None)], odd_titles),
    Scenario("a very long title shows, opens its page and Properties, and persists",
             [(search("extremely long winded") + "expect-has Extremely Long Winded\nclick " + LONG
               + "\nwait 400\nkey Alt+Return\nexpect Properties\nclick Cancel\nexpect-not Properties\n", None),
              (search("extremely long winded") + "expect-has Extremely Long Winded\n", None)], odd_titles),
    Scenario("a game with an empty title doesn't break the list, search or sort",
             [("expect " + FIRST + "\nkey End\nwait 600\nexpect " + LAST + "\nkey Home\nwait 600\n"
               + search("keeps") + "expect Keeps Company\nkey Escape\nkey Escape\nwait 600\n"
               + "key Ctrl+Shift+S\nwait 3500\nkey Ctrl+Shift+S\nwait 3500\nkey Ctrl+Shift+S\nkey Home\nwait 600\n"
               + "expect " + FIRST + "\n", None)], empty_title),
]

sys.exit(run_scenarios(SCENARIOS))
