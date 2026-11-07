import importlib.util
import os
import sys

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
    Scenario("Storage lists the largest games and switches to Dormant",
             [(OPEN + "expect Storage\nexpect Largest\nexpect Dormant\nexpect Baldur's Gate 3\n"
               + "expect-not Uninstall\nclick Dormant\nexpect Not played yet\n", None)], demo),
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
    Scenario("Save backups offers Back up now, which opens the saves backup",
             [(OPEN + DOWN + "expect Save backups\nexpect Played games\nclick Back up now\nwait 800\n"
               + "expect Backups kept per game\n", None)], demo),
    Scenario("Fun facts name the longest session and link to the Gallery",
             [(OPEN + DOWN + "expect Fun facts\nexpect Waiting longest\nclick Open the Gallery\nwait 800\n"
               + "expect-not Library overview\nexpect Gallery\n", None)], demo),
    Scenario("the Games tile opens the whole library, filters cleared",
             [(OPEN + STATUS + "click Dropped\nwait 500\nexpect-not Metroid Dread\n"
               + "key F7\nexpect Library overview\nkey Home\nwait 300\nclick Games\nwait 500\n"
               + "expect-not Library overview\nexpect Metroid Dread\nexpect Final Fantasy XIV Online\n", None)], demo),
]

sys.exit(run_scenarios(SCENARIOS))
