import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# Selecting games in List view and in Grid view: every scenario runs in both, and checks what is selected
# by acting on it (Ctrl+D, the game menu) and reading the game files afterwards.
# Kirby, Mario Kart, Metroid and Pikmin follow each other in both views (the list's Switch group, the
# grid's titles in order). The Grid view gets small covers so every game is inside the window: the driver
# clicks an item's centre, and a cover below the window's edge gets nothing.

FILES = {
    "Animal Crossing: New Horizons": "switch-animal-crossing-new-horizons.toml",
    "Kirby and the Forgotten Land": "switch-kirby-and-the-forgotten-land.toml",
    "Mario Kart 8 Deluxe": "switch-mario-kart-8-deluxe.toml",
    "Metroid Dread": "switch-metroid-dread.toml",
    "Pikmin 4": "switch-pikmin-4.toml",
    "Super Mario Bros. Wonder": "switch-super-mario-bros-wonder.toml",
    "SuperTuxKart": "linux-supertuxkart.toml",
    "The Legend of Zelda: Tears of the Kingdom": "switch-the-legend-of-zelda-tears-of-the-kingdom.toml",
}
KIRBY, MARIO_KART, METROID, PIKMIN = ("Kirby and the Forgotten Land", "Mario Kart 8 Deluxe", "Metroid Dread",
                                      "Pikmin 4")
WONDER = "Super Mario Bros. Wonder"
FIRST_GAME = "Animal Crossing: New Horizons"


class View:
    def __init__(self, name, settings, nxt, prev, last_game):
        self.name = name
        self.settings = settings
        self.next = nxt
        self.prev = prev
        self.last_game = last_game

    def setup(self, dirs):
        dirs.use_demo()
        if self.settings:
            with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
                f.write(self.settings)


VIEWS = [
    View("List", "", "Down", "Up", "SuperTuxKart"),
    View("Grid", '\n[general]\nview = "grid"\ngrid_show_titles = true\ngrid_cover_width = 100\n',
         "Right", "Left", "The Legend of Zelda: Tears of the Kingdom"),
]

START = "expect Pikmin 4\nwait 500\n"


def game_file(dirs, title):
    with open(os.path.join(dirs.data, "library", "games", FILES[title])) as f:
        return f.read()


def marked(line, *titles, unmarked=()):
    def check(result, dirs):
        found = []
        for title in titles:
            if line not in game_file(dirs, title):
                found.append(f"{title} lacks '{line}'")
        for title in unmarked:
            if line in game_file(dirs, title):
                found.append(f"{title} has '{line}' though it wasn't selected")
        return found
    return check


def favorites(*titles, unmarked=()):
    return marked("favorite = true", *titles, unmarked=unmarked)


def scenarios():
    found = []
    for view in VIEWS:
        v = view.name
        found += [
            Scenario(f"{v}: a click selects one game, a second click another",
                     [(START + f"click {KIRBY}\nexpect-not-has selected\nclick {PIKMIN}\nexpect-not-has selected\n"
                       + "key Ctrl+D\nwait 600\n",
                       favorites(PIKMIN, unmarked=(KIRBY, MARIO_KART, METROID, FIRST_GAME)))], view.setup),
            Scenario(f"{v}: Shift+click selects a range from the anchor, a second one moves its end",
                     [(START + f"click {KIRBY}\nshift-click {PIKMIN}\nexpect 4 selected\n"
                       + f"shift-click {MARIO_KART}\nexpect 2 selected\nkey Ctrl+D\n"
                       + "expect 2 games added to favorites\nexpect 2 selected\nwait 600\n",
                       favorites(KIRBY, MARIO_KART, unmarked=(METROID, PIKMIN)))], view.setup),
            Scenario(f"{v}: Ctrl+click adds a game to the selection and takes one out",
                     [(START + f"click {KIRBY}\nctrl-click {METROID}\nexpect 2 selected\nctrl-click {PIKMIN}\n"
                       + f"expect 3 selected\nctrl-click {METROID}\nexpect 2 selected\nkey Ctrl+D\n"
                       + "expect 2 games added to favorites\nwait 600\n",
                       favorites(KIRBY, PIKMIN, unmarked=(MARIO_KART, METROID)))], view.setup),
            Scenario(f"{v}: Ctrl+click on the last other game of a selection leaves one game, unselected",
                     [(START + f"click {KIRBY}\nctrl-click {METROID}\nexpect 2 selected\nctrl-click {KIRBY}\n"
                       + "expect-not-has selected\nkey Ctrl+D\nwait 600\n",
                       favorites(METROID, unmarked=(KIRBY, MARIO_KART)))], view.setup),
            Scenario(f"{v}: Ctrl+A selects what the search lists, and only that is changed",
                     [(START + "type mario\nwait 400\nkey Escape\nkey Ctrl+A\nexpect 2 selected\nkey Ctrl+D\n"
                       + "expect 2 games added to favorites\nwait 600\n",
                       favorites(MARIO_KART, WONDER, unmarked=(KIRBY, METROID, PIKMIN)))], view.setup),
            Scenario(f"{v}: Shift+{view.next} and Shift+{view.prev} grow and shrink the selection",
                     [(START + f"click {KIRBY}\nkey Shift+{view.next}\nexpect 2 selected\nkey Shift+{view.next}\n"
                       + f"key Shift+{view.next}\nexpect 4 selected\nkey Shift+{view.prev}\nexpect 3 selected\n"
                       + "key Ctrl+D\nexpect 3 games added to favorites\nwait 600\n",
                       favorites(KIRBY, MARIO_KART, METROID, unmarked=(PIKMIN, FIRST_GAME)))], view.setup),
            Scenario(f"{v}: Shift+{view.prev} back to the anchor ends the selection",
                     [(START + f"click {MARIO_KART}\nkey Shift+{view.next}\nexpect 2 selected\nkey Shift+{view.prev}\n"
                       + "expect-not-has selected\nkey Ctrl+D\nwait 600\n",
                       favorites(MARIO_KART, unmarked=(KIRBY, METROID)))], view.setup),
            Scenario(f"{v}: End with a selection ends it on the last game, Home on the first",
                     [(START + f"click {KIRBY}\nshift-click {PIKMIN}\nexpect 4 selected\nkey End\n"
                       + "expect-not-has selected\nkey Ctrl+D\nwait 400\n"
                       + f"click {KIRBY}\nshift-click {PIKMIN}\nexpect 4 selected\nkey Home\n"
                       + "expect-not-has selected\nkey Ctrl+D\nwait 600\n",
                       favorites(view.last_game, FIRST_GAME, unmarked=(KIRBY, MARIO_KART, METROID, PIKMIN)))],
                     view.setup),
            Scenario(f"{v}: the menu of a selected game acts on the whole selection and keeps it",
                     [(START + f"click {KIRBY}\nshift-click {METROID}\nexpect 3 selected\n"
                       + f"rightclick {MARIO_KART}\nwait 500\nclick Add to favorites\n"
                       + "expect 3 games added to favorites\nexpect 3 selected\nwait 600\n",
                       favorites(KIRBY, MARIO_KART, METROID, unmarked=(PIKMIN, WONDER)))], view.setup),
            Scenario(f"{v}: the menu of a game outside the selection acts on that game alone",
                     [(START + f"click {KIRBY}\nshift-click {METROID}\nexpect 3 selected\n"
                       + f"rightclick {WONDER}\nwait 500\nclick Add to favorites\nwait 600\n",
                       favorites(WONDER, unmarked=(KIRBY, MARIO_KART, METROID)))], view.setup),
            # A cover the grid let go of stays visible to the driver (Qt culls it instead of hiding it), so
            # the games' absence is checked after a restart.
            Scenario(f"{v}: Hide in the menu of a selection hides every selected game",
                     [(START + f"click {KIRBY}\nshift-click {METROID}\nexpect 3 selected\n"
                       + f"rightclick {MARIO_KART}\nwait 500\nclick Hide\nexpect 3 games hidden\nwait 600\n",
                       marked("hidden = true", KIRBY, MARIO_KART, METROID, unmarked=(PIKMIN, FIRST_GAME))),
                      (START + f"expect-not {KIRBY}\nexpect-not {MARIO_KART}\nexpect-not {METROID}\n", None)],
                     view.setup),
        ]
    grid = VIEWS[1]
    found.append(Scenario("Grid: Shift+Down selects to the cover below, Shift+Up back ends the selection",
                          [(START + f"click {KIRBY}\nkey Shift+Down\nexpect-has selected\nexpect-not 1 selected\n"
                            + "expect-not 2 selected\nkey Shift+Up\nexpect-not-has selected\nkey Ctrl+D\nwait 600\n",
                            favorites(KIRBY, unmarked=(MARIO_KART,)))], grid.setup))
    return found


if __name__ == "__main__":
    sys.exit(run_scenarios(scenarios()))
