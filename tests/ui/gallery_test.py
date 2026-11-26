import os
import struct
import sys
import zlib
from datetime import datetime, timezone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The Gallery over two games of the scenario's own, each with a media folder of generated screenshots: Alpha
# Quest has two from 2026, Beta Run one from 2025. The demo library around them has no media of its own.
# A tile has no text for the driver to click, so tiles are opened from the keyboard (arrows, Return), the
# way the page's own keys do it. The viewer's bars, which hold its texts and buttons, would hide 2 s after the
# last mouse move, which the driver never makes: the style turns the hiding off.

SHOTS = {
    "alpha": [("alpha-1.png", datetime(2026, 6, 12, 12, tzinfo=timezone.utc)),
              ("alpha-2.png", datetime(2026, 5, 10, 12, tzinfo=timezone.utc))],
    "beta": [("beta-1.png", datetime(2025, 11, 1, 12, tzinfo=timezone.utc))],
}
TITLES = {"alpha": "Alpha Quest", "beta": "Beta Run"}


def png(width, height, rgb):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    rows = b"".join(b"\0" + bytes(rgb) * width for _ in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def setup(dirs):
    dirs.use_demo()
    dirs.write("themes/styles/Soft.toml", "[media]\nviewer_hide_delay = 0\n")
    for game, shots in SHOTS.items():
        folder = os.path.join(dirs.home, "media", game)
        os.makedirs(folder)
        for number, (name, when) in enumerate(shots):
            path = os.path.join(folder, name)
            with open(path, "wb") as f:
                f.write(png(64, 36, (40 * number + 60, 90, 150)))
            os.utime(path, (when.timestamp(), when.timestamp()))
        dirs.write(f"library/games/linux-{game}.toml",
                   f'title = "{TITLES[game]}"\nplatform = "linux"\nexe = "/bin/true"\ninstall_dir = "{folder}"\n'
                   f'media_folders = ["{folder}"]\nadded = 2025-01-01\n')


def media_favorites(dirs):
    path = os.path.join(dirs.data, "library", "media.toml")
    if not os.path.exists(path):
        return ""
    with open(path) as f:
        return f.read()


def favorite_media(*names, unmarked=()):
    def check(result, dirs):
        text = media_favorites(dirs)
        return ([f"{name} isn't in library/media.toml" for name in names if name not in text]
                + [f"{name} is in library/media.toml though it wasn't marked" for name in unmarked if name in text])
    return check


def game_favorite(name):
    def check(result, dirs):
        with open(os.path.join(dirs.data, "library", "games", name)) as f:
            if "favorite = true" not in f.read():
                return [f"{name} isn't a favorite: Go to game didn't select it"]
    return check


OPEN = "wait 1000\nkey F8\nwait 1500\nexpect Gallery\nexpect 3 items\n"
VIEWER_FIRST = "key Home\nkey Return\nwait 800\nexpect-has 1 of 3\nexpect Go to game\n"


SCENARIOS = [
    Scenario("Return on a tile opens it in the viewer, the arrows step through, Escape closes the viewer only",
             [(OPEN + VIEWER_FIRST + "expect Alpha Quest\nkey Right\nwait 400\nexpect-has 2 of 3\nkey Right\n"
               + "wait 400\nexpect-has 3 of 3\nexpect Beta Run\nkey Escape\nwait 600\nexpect-not Go to game\n"
               + "expect Gallery\nkey Escape\nwait 600\nexpect-not Gallery\nexpect Pikmin 4\n", None)], setup),
    Scenario("the viewer's star marks the screenshot, Favorites only then shows it alone, and it persists",
             [(OPEN + VIEWER_FIRST + "click Add to favorites (Ctrl+D)\nwait 600\nexpect Remove from favorites (Ctrl+D)\nkey Escape\n"
               + "wait 600\nclick Favorites only\nwait 600\nexpect 1 of 3 items\nclick Clear all\nwait 600\n"
               + "expect 3 items\n", favorite_media("alpha-1.png", unmarked=("alpha-2.png", "beta-1.png"))),
              (OPEN + "click Favorites only\nwait 600\nexpect 1 of 3 items\n", None)], setup),
    Scenario("Go to game in the viewer closes the Gallery on that game",
             [(OPEN + "key End\nkey Return\nwait 800\nexpect-has 3 of 3\nexpect Beta Run\nclick Go to game\n"
               + "wait 1000\nexpect-not Gallery\nkey Ctrl+D\nwait 600\n", game_favorite("linux-beta.toml"))], setup),
    # The sidebar is drawn after the grid, so "click <game>" reaches the sidebar's entry, not a tile's caption.
    Scenario("a game in the filters narrows the Gallery to its screenshots, a second click lifts it",
             [(OPEN + "click Beta Run\nwait 600\nexpect 1 of 3 items\nexpect-not May 2026\nexpect November 2025\n"
               + "click Beta Run\nwait 600\nexpect 3 items\nexpect May 2026\n", None)], setup),
    Scenario("a type with nothing in it says so, and Clear all filters brings everything back",
             [(OPEN + "click Clips\nwait 600\nexpect Nothing matches these filters\nexpect-not May 2026\n"
               + "click Clear all filters\nwait 600\nexpect 3 items\nclick Screenshots\nwait 600\n"
               + "expect 3 of 3 items\n", None)], setup),
    # With a game picked and let go first, the empty grid leaves the sticky heading pointing at a row that is gone.
    Scenario("a type with nothing after a game filter was lifted",
             [(OPEN + "click Beta Run\nwait 600\nclick Beta Run\nwait 600\nclick Clips\nwait 600\n"
               + "expect Nothing matches these filters\nclick Clear all filters\nwait 600\nexpect 3 items\n", None)],
             setup, known_bugs=[("GalleryPage.qml:1077:13: Unable to assign [undefined] to int",
                                 "the Gallery's sticky heading reads past headerOfRow when the grid empties")]),
    Scenario("the game search narrows the filter's list of games and says when none has media",
             [(OPEN + "click Search games\ntype Alp\nwait 600\nexpect Alp\nexpect 3 items\nclick Alpha Quest\nwait 600\nexpect 2 of 3 items\n"
               + "click Alp\ntype zz\nwait 600\nexpect No game by that name has media\nexpect 2 of 3 items\n", None)], setup),
    Scenario("a year and a month in the filters, then Clear all",
             [(OPEN + "click 2025\nwait 600\nexpect 1 of 3 items\nexpect-not June 2026\nclick 2025\nwait 600\n"
               + "click May\nwait 600\nexpect 1 of 3 items\nexpect May 2026\nexpect-not June 2026\n"
               + "click Clear all\nwait 600\nexpect 3 items\nexpect June 2026\n", None)], setup),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
