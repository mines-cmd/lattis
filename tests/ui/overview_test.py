import importlib.util
import os
import sys
import tomllib

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

OPEN = "key F7\nexpect Library overview\nexpect Needs attention\nwait 1200\n"
DOWN = "key End\nwait 300\n"
# The page is taller than the window: End shows the last cards, a page down the Status card.
STATUS = "key PageDown\nwait 300\n"
CARDS = "expect Games\nexpect Hours played\nexpect Backlog\nexpect Continue playing\n"


def demo(dirs):
    dirs.use_demo()


def empty_library(dirs):
    dirs.write("library/library.toml", "version = 1\n")


def overview_section(dirs):
    path = os.path.join(dirs.data, "settings.toml")
    try:
        with open(path, "rb") as f:
            return tomllib.load(f).get("overview", {})
    except (OSError, tomllib.TOMLDecodeError):
        return None


def overview_settings(hidden=None, left=None, below=None, placed=False):
    """Checks [overview] in settings.toml: the hidden cards, a card in the left column, the cards below the columns.
    `placed`: both columns are written, which is what the first change does to the automatic dealing."""
    def check(result, dirs):
        section = overview_section(dirs)
        if section is None:
            return ["settings.toml wasn't written"]
        problems = []
        if hidden is not None and section.get("hidden") != hidden:
            problems.append(f"hidden is {section.get('hidden')}, not {hidden}")
        if left is not None and left not in section.get("left", []):
            problems.append(f"{left} isn't in left: {section.get('left')}")
        if below is not None and section.get("below") != below:
            problems.append(f"below is {section.get('below')}, not {below}")
        if placed and not (section.get("left") and section.get("right")):
            problems.append("the columns weren't fixed by the first change")
        return problems
    return check


def overview_order(first, after):
    """Checks that `first` comes after `after` in the column that holds both."""
    def check(result, dirs):
        section = overview_section(dirs) or {}
        for column in (section.get("left", []), section.get("right", [])):
            if first in column and after in column:
                return [] if column.index(first) > column.index(after) else [f"{first} is above {after}"]
        return [f"{first} and {after} aren't in one column: {section}"]
    return check


def overview_is(**expected):
    """Checks [overview] key by key; a key that isn't written counts as an empty list."""
    def check(result, dirs):
        section = overview_section(dirs)
        if section is None:
            return ["settings.toml wasn't written"]
        return [f"{key} is {section.get(key, [])}, not {value}"
                for key, value in expected.items() if section.get(key, []) != value]
    return check


def overview_has(column, name):
    """Checks that a card is in one of the columns' lists."""
    def check(result, dirs):
        section = overview_section(dirs) or {}
        return [] if name in section.get(column, []) else [f"{name} isn't in {column}: {section}"]
    return check


def steam_achievements(dirs, app_id, game, count, unlocked):
    """Gives a demo Steam game `count` achievements, `unlocked` of them done, with the fixture builder's files."""
    spec = importlib.util.spec_from_file_location(
        "make_fixtures", os.path.join(REPO, "tests", "data", "steam", "appcache", "stats", "make-fixtures.py"))
    mk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mk)
    bits = [mk.achievement(i, f"A{i}", dict(english=f"Achievement {i}"), dict(english="Do it."), f"a{i}")
            for i in range(count)]
    stats = "steam/appcache/stats/"
    dirs.write(f"{stats}UserGameStatsSchema_{app_id}.bin", mk.schema(
        app_id, game, mk.kv_map("0", mk.kv_str("type", "ACHIEVEMENTS"), mk.kv_map("bits", *bits))) + b"\x08")
    dirs.write(f"{stats}UserGameStats_{mk.USER}_{app_id}.bin", mk.progress(
        mk.stat_progress(0, (1 << unlocked) - 1, {i: 1700000000 + i for i in range(unlocked)})) + b"\x08")


def with_achievements(dirs):
    dirs.use_demo()
    steam_achievements(dirs, 1145360, "Hades", 5, 4)  # 80%: nearly done
    steam_achievements(dirs, 367520, "Hollow Knight", 10, 5)  # 50%
    steam_achievements(dirs, 413150, "Stardew Valley", 4, 4)  # perfect: done, not nearly


SCENARIOS = [
    Scenario("F7 opens the overview with its tiles and Continue playing, Escape closes it",
             [(OPEN + CARDS + "key Escape\nexpect-not Library overview\nexpect Hades\n", None)], demo),
    Scenario("F7 again closes the overview",
             [(OPEN + "key F7\nexpect-not Library overview\n", None)], demo),
    Scenario("Play statistics opens from the overview and takes its place",
             [(OPEN + "click Play statistics\nexpect-not Library overview\n"
               + "expect Sessions appear here once a game is played.\n", None)], demo),
    Scenario("the overview replaces Play statistics and F9 swaps back",
             [("key F9\nexpect Sessions appear here once a game is played.\n"
               + "key F7\nexpect Library overview\nexpect-not Sessions appear here once a game is played.\n"
               + "key F9\nexpect Sessions appear here once a game is played.\nexpect-not Library overview\n", None)],
             demo),
    Scenario("the app menu's Library overview entry opens the overview",
             [("key F10\nwait 600\nclick Library overview\nwait 300\nexpect Back to games\nexpect Storage\n", None)],
             demo),
    Scenario("Storage lists the largest games",
             [(OPEN + "expect Storage\nexpect Largest games\nexpect-not Dormant\nexpect Baldur's Gate 3\n"
               + "expect-not Uninstall\n", None)], demo),
    Scenario("Games by platform, Status, Top tags and Release years list the library",
             [(OPEN + "expect Games by platform\nexpect Status\nexpect Top tags\nexpect Release years\n"
               + "expect Dropped\nexpect Roguelike\n", None)], demo),
    Scenario("a tag in Top tags opens the game list with that tag on",
             [(OPEN + DOWN + "click Roguelike\nwait 500\nexpect-not Library overview\nexpect Hades\n"
               + "expect-not Metroid Dread\n", None)], demo),
    Scenario("a status in the overview opens the game list with that status on, uninstalled games included",
             [(OPEN + STATUS + "click Dropped\nwait 500\nexpect-not Library overview\nexpect Final Fantasy XIV Online\n"
               + "expect-not Metroid Dread\n", None)], demo),
    Scenario("a platform in the overview opens the game list on that platform, and the next click replaces the filters",
             [(OPEN + STATUS + "click Proton\nwait 500\nexpect-not Library overview\nexpect Final Fantasy XIV Online\n"
               + "expect-not Metroid Dread\n"
               + "key F7\nexpect Library overview\n" + STATUS + "click Completed\nwait 500\nexpect Metroid Dread\n"
               + "expect-not Final Fantasy XIV Online\n", None)], demo),
    Scenario("Needs attention lists what to fix and Look up opens Look up missing info",
             [(OPEN + "key PageDown\nkey PageDown\nwait 300\nexpect Needs attention\nexpect Games without a cover\nexpect Games without game info\n"
               + "click Look up\nwait 800\nexpect Missing info\nexpect Games not on IGDB\n", None)], demo),
    Scenario("Needs attention leaves out a row with nothing to report",
             [(OPEN + DOWN + "expect-not Steam updates waiting\n", None)], demo),
    Scenario("From your backlog offers a game with Play and Shuffle, and Shuffle keeps the card",
             [(OPEN + DOWN + "expect From your backlog\nexpect Play\nexpect Shuffle\nclick Shuffle\nwait 500\n"
               + "expect From your backlog\nexpect Play\n", None)], demo),
    Scenario("the Achievements tile says it covers Steam games and Nearly done lists those at 80% or more",
             [(OPEN + "expect Achievements\nexpect 1 perfect · Steam games\n" + DOWN
               + "expect Nearly done\nexpect 4 of 5\nexpect-not 5 of 10\nexpect-not 4 of 4\n", None)],
             with_achievements),
    Scenario("without achievements the tile says so and Nearly done is left out",
             [(OPEN + "expect Achievements\nexpect No achievements in Steam games\n" + DOWN
               + "expect-not Nearly done\n", None)], demo),
    Scenario("the Achievements tile opens the Steam games",
             [(OPEN + "click Achievements\nwait 500\nexpect-not Library overview\nexpect Hades\n"
               + "expect-not Metroid Dread\n", None)], with_achievements),
    Scenario("Compatibility says what it covers, counts native and Proton games and asks ProtonDB for the rest",
             [(OPEN + DOWN + "expect Compatibility\nexpect ProtonDB tiers of your installed Steam games\n"
               + "expect Native games\nexpect Proton games\n", None)], demo),
    Scenario("Save backups offers Back up now, which backs up the played games and stays on the overview",
             [(OPEN + DOWN + "expect Save backups\nexpect-not Not protected\nclick Back up now\nwait 1500\n"
               + "expect Library overview\nexpect Back up now\nexpect-not Backups kept per game\n", None)], demo),
    Scenario("Fun facts name the game waiting longest",
             [(OPEN + DOWN + "expect Fun facts\nexpect Waiting longest\n"
               + "expect-not Open the Gallery\n", None)], demo),
    Scenario("a release year column opens the game list with that year's games, other filters cleared",
             [(OPEN + STATUS + "click Dropped\nwait 500\nexpect-not Metroid Dread\n"
               # The mouse is still where Dropped was, over this column: its open tooltip has the same text,
               # and the driver would click that. A click on the card's title moves the pointer away first.
               + "key F7\nexpect Library overview\nwait 1200\n" + DOWN + "wait 600\nclick Release years\nwait 600\n"
               + "click 2013 · 2 games\nwait 500\n"
               + "expect-not Library overview\nexpect The Legend of Zelda: A Link Between Worlds\n"
               + "expect Final Fantasy XIV Online\nexpect-not Pikmin 4\nexpect-not Hades\nexpect 2013\n", None)], demo),
    Scenario("the Games tile opens the whole library, filters cleared",
             [(OPEN + STATUS + "click Dropped\nwait 500\nexpect-not Metroid Dread\n"
               + "key F7\nexpect Library overview\nkey Home\nwait 300\nclick Games\nwait 500\n"
               + "expect-not Library overview\nexpect Metroid Dread\nexpect Final Fantasy XIV Online\n", None)], demo),
    Scenario("hidden cards and the hidden tiles are left out",
             [(OPEN + "expect-not Hours played\nexpect-not Continue playing\nexpect-not Storage\n"
               + "expect Games by platform\n", None)],
             lambda dirs: (dirs.use_demo(), dirs.write("settings.toml", '[overview]\nhidden = ["tiles", "continue_playing", "storage"]\n'))),
    Scenario("cards placed in the columns and the rows below them all show, and a hidden one stays out",
             [("key F7\nexpect Library overview\nwait 1200\nexpect Hours played\nexpect Continue playing\n"
               + "expect Games by platform\nexpect-not Storage\nexpect-not Top tags\n" + DOWN
               + "expect Fun facts\nexpect Release years\n", None)],
             lambda dirs: (dirs.use_demo(), dirs.write(
                 "settings.toml",
                 '[overview]\nleft = ["facts", "platforms"]\nright = ["years", "statuses"]\n'
                 'hidden = ["storage", "tags"]\nabove = ["continue_playing"]\nbelow = ["tiles"]\n'))),
    Scenario("a card's right-click menu hides it, and it stays hidden after a restart",
             [(OPEN + "rightclick Storage\nexpect Hide card\nexpect Move down\nexpect-not Move up\n"
               + "expect-not Move to left column\nclick Hide card\nwait 300\nexpect-not Storage\n"
               + "expect Games by platform\n", overview_settings(hidden=["storage"], placed=True)),
              (OPEN + "expect-not Storage\nexpect Games by platform\n", None)], demo),
    Scenario("a card moves to the other column from its menu, and stays there after a restart",
             [(OPEN + "rightclick Games by platform\nexpect Move to left column\nexpect-not Move to right column\n"
               + "click Move to left column\nwait 300\n", overview_settings(left="platforms")),
              (OPEN + STATUS + "rightclick Games by platform\nexpect Move to right column\nexpect-not Move to left column\n"
               + "key Escape\n", None)], demo),
    Scenario("moving a card down puts it under its neighbour",
             [(OPEN + "rightclick Storage\nclick Move down\nwait 300\n", overview_order("storage", after="attention")),
              (OPEN + "rightclick Needs attention\nexpect Move down\nexpect-not Move up\n", None)], demo),
    Scenario("right-clicking a bar or a game in a card opens the card's menu, and a left click still acts",
             [(OPEN + "rightclick The Legend of Zelda: Tears of the Kingdom\nexpect Hide card\n"
               + "expect Move below the cards\nkey Escape\nwait 300\n" + STATUS
               + "rightclick Dropped\nexpect Hide card\nkey Escape\nwait 300\n"
               + "click Dropped\nwait 500\nexpect-not Library overview\n", None)], demo),
    Scenario("the tiles' menu moves them below the cards and hides them",
             [(OPEN + "rightclick Hours played\nexpect Move below the cards\nexpect-not Move above the cards\n"
               + "expect-not Move up\nexpect Move down\nclick Move below the cards\nwait 300\n",
               overview_settings(below=["tiles"])),
              (OPEN + DOWN + "rightclick Hours played\nexpect Move above the cards\nexpect-not Move below the cards\n"
               + "click Hide card\nwait 300\nexpect-not Hours played\n", overview_settings(hidden=["tiles"]))], demo),
    Scenario("Customize turns on edit mode: every card shows, empty ones with a note",
             [(OPEN + "expect Customize\nexpect-not Reset layout\nexpect-not Nothing to show now\n"
               + "click Customize\nwait 400\nexpect Reset layout\nexpect Done\nexpect-not Customize\n"
               + "expect Hide Storage\nexpect Move Storage\nexpect Hide the tiles\nexpect Hide Continue playing\n"
               + STATUS + STATUS + "expect Nearly done\nexpect Nothing to show now\n", None)], demo),
    Scenario("Escape leaves edit mode, and the next one leaves the page",
             # (A scroll key first would take the keyboard focus to the list, which Escape then closes nothing on.)
             [(OPEN + "click Customize\nwait 400\nexpect Done\nkey Escape\nwait 400\nexpect Library overview\n"
               + "expect Customize\nexpect-not Reset layout\nexpect-not Nothing to show now\nexpect-not Hide Storage\n"
               + "key Escape\nexpect-not Library overview\n", None)], demo),
    Scenario("Done leaves edit mode and Customize comes back",
             [(OPEN + "click Customize\nwait 400\nexpect Done\nclick Done\nwait 400\nexpect Customize\n"
               + "expect-not Reset layout\nexpect-not Done\nexpect-not Hide Storage\n", None)], demo),
    Scenario("the card menu's Customize overview turns on edit mode",
             [(OPEN + "rightclick Storage\nexpect Customize overview\nexpect-not Customize overview…\n"
               + "click Customize overview\nwait 400\nexpect Done\nexpect Hide Storage\n", None)], demo),
    Scenario("cards take no clicks and no menu while editing",
             [(OPEN + "click Customize\nwait 400\nclick Games\nwait 400\nexpect Library overview\nexpect Done\n"
               + "rightclick Storage\nwait 300\nexpect-not Hide card\n", None)], demo),
    Scenario("the eye hides a card, which stays in place dimmed; its eye shows it again",
             [(OPEN + "click Customize\nwait 400\nclick Hide Storage\nwait 400\nexpect Show Storage\nexpect Storage\n"
               + "expect-not Hide Storage\nclick Done\nwait 400\nexpect-not Storage\nexpect Games by platform\n",
               overview_settings(hidden=["storage"], placed=True)),
              (OPEN + "expect-not Storage\nclick Customize\nwait 400\nexpect Show Storage\nclick Show Storage\nwait 400\n"
               + "expect Hide Storage\nclick Done\nwait 400\nexpect Storage\n", overview_is(hidden=[]))], demo),
    Scenario("the eye hides the tiles and Continue playing too",
             [(OPEN + "click Customize\nwait 400\nclick Hide the tiles\nwait 400\nexpect Show the tiles\n"
               + "click Hide Continue playing\nwait 400\nexpect Show Continue playing\nexpect Hours played\n"
               + "click Done\nwait 400\nexpect-not Hours played\nexpect-not Continue playing\n",
               overview_is(hidden=["tiles", "continue_playing"]))], demo),
    Scenario("Reset layout shows every card again and goes back to automatic dealing",
             [(OPEN + "expect-not Storage\nexpect-not Top tags\nclick Customize\nwait 400\nclick Reset layout\nwait 400\n"
               + "expect Hide Storage\nclick Done\nwait 400\nexpect Storage\nexpect Games by platform\n"
               + "expect-not Nothing to show now\n",
               overview_is(hidden=[], left=[], right=[], below=[]))],
             lambda dirs: (dirs.use_demo(), dirs.write(
                 "settings.toml",
                 '[overview]\nleft = ["facts", "platforms"]\nright = ["years", "statuses"]\n'
                 'hidden = ["storage", "tags"]\nabove = ["continue_playing"]\nbelow = ["tiles"]\n'))),
    Scenario("dragging a card by its grip moves it into the other column, and it stays there after a restart",
             [(OPEN + "click Customize\nwait 400\ndrag Move Storage by 800 0\nwait 800\n"
               + "click Done\nwait 400\nexpect Storage\n", overview_has("right", "storage")),
              (OPEN + STATUS + "rightclick Storage\nexpect Move to left column\nexpect-not Move to right column\n"
               + "key Escape\n", None)], demo),
    Scenario("dragging a card down its column puts it under the cards it passes",
             [(OPEN + "click Customize\nwait 400\ndrag Move Storage by 0 450\nwait 800\n"
               + "click Done\nwait 400\nexpect Storage\n", overview_order("storage", after="attention"))], demo),
    Scenario("the tiles drag below the cards and swap with Continue playing",
             [(OPEN + "click Customize\nwait 400\ndrag Move the tiles by 0 400\nwait 800\n",
               overview_is(above=["continue_playing", "tiles"])),
              (OPEN + "click Customize\nwait 400\ndrag Move the tiles by 0 3000\nwait 800\n",
               overview_is(above=["continue_playing"], below=["tiles"]))], demo),
    Scenario("with one column hiding stays but dragging isn't offered",
             [(OPEN + "click Customize\nwait 400\nexpect Hide Storage\nexpect Hide the tiles\n"
               + "expect-not Move Storage\nexpect-not Move the tiles\nclick Hide Continue playing\nwait 400\n"
               + "expect Show Continue playing\n", overview_is(hidden=["continue_playing"]))],
             lambda dirs: (dirs.use_demo(), dirs.write(
                 "themes/styles/Soft.toml", "[overview]\ncompact_width = 4000\nnarrow_width = 4000\n"))),
    Scenario("an empty library shows the tiles and leaves out the cards with nothing to show",
             [("key F7\nexpect Library overview\nwait 800\n" + CARDS.replace("expect Continue playing\n", "")
               + "expect-not Continue playing\nexpect-not Storage\nexpect-not Needs attention\n"
               + "expect-not From your backlog\nexpect-not Nearly done\nkey Escape\nexpect-not Library overview\n",
               None)], empty_library),
]

sys.exit(run_scenarios(SCENARIOS))
