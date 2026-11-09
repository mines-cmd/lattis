import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN_FILTERS = "key Ctrl+Shift+F\nwait 600\nexpect SORT\n"
OPEN_COLL = "key Ctrl+Shift+C\nwait 600\nexpect COLLECTIONS\n"
OPEN_MORE ="click More filters\nwait 600\nexpect Fewer\nexpect RELEASE YEAR\n"
# Only these two demo games have 100 hours or more, and both are installed.
LONG = "expect Animal Crossing: New Horizons\nexpect-not Pikmin 4\n"


def with_list(text):
    def setup(dirs):
        dirs.use_demo()
        with open(os.path.join(dirs.data, "settings.toml"), "a") as f:
            f.write(text)
    return setup


def demo(dirs):
    dirs.use_demo()


SCENARIOS = [
    Scenario("More filters opens the second column, a choice filters, and the row closes it again",
             [(FIRST + OPEN_FILTERS + "expect-not Fewer\n" + OPEN_MORE
               + "expect PLAY TIME\nexpect LAST PLAYED\nexpect ACHIEVEMENTS\nexpect RATING\n"
               + "click Over 100 h\nwait 500\n" + LONG + "expect Played: Over 100 h\n"
               + "click More filters\nwait 600\nexpect-not Fewer\nexpect-not RELEASE YEAR\nexpect SORT\n"
               + LONG + "expect Played: Over 100 h\n", None)], demo),
    Scenario("the panel keeps its width after a restart",
             [(FIRST + OPEN_FILTERS + OPEN_MORE, None),
              (OPEN_FILTERS + "expect Fewer\nexpect RELEASE YEAR\nclick More filters\nwait 600\nexpect-not Fewer\nclick More filters\nwait 600\nexpect Fewer\nkey Tab\nkey Space\nwait 600\nexpect-not Fewer\n", None),
              (OPEN_FILTERS + "expect-not Fewer\nexpect More filters\n", None)], demo),
    Scenario("each new filter is a chip with its x, and Clear filters turns them off",
             [(FIRST + OPEN_FILTERS + OPEN_MORE
               + "click Over 100 h\nwait 400\nclick This year\nwait 400\nclick Not started\nwait 400\n"
               + "expect Played: Over 100 h\nexpect Played this year\nexpect Achievements: Not started\n"
               + "click Clear filters\nwait 500\nexpect-not Played: Over 100 h\nexpect-not Played this year\n"
               + "expect-not Achievements: Not started\n" + FIRST, None)], demo),
    Scenario("the year slider moves with the arrow keys, and the range filters and shows as a chip",
             [(FIRST + OPEN_FILTERS + OPEN_MORE + "key Tab\nkey Tab\nkey Right\nwait 500\n"
               + "expect 2013 – 2023\nexpect-not Kid Icarus: Uprising\nexpect The Legend of Zelda: A Link Between Worlds\n"
               + "key Right\nwait 500\nexpect 2014 – 2023\nkey Home\nwait 500\nexpect Kid Icarus: Uprising\nexpect-not 2012 – 2023\n", None)], demo),
    Scenario("a saved year range shows as a chip and in the column, and filters",
             [("expect The Legend of Zelda: A Link Between Worlds\nexpect-not Pikmin 4\nexpect-not Hades\n"
               + "expect 2013\n" + OPEN_FILTERS + OPEN_MORE + "expect-has 2013\n", None)],
             with_list("\n[list]\nyear_from = 2013\nyear_to = 2013\n")),
    Scenario("Save as collection copies a year range, the collection opens as a fixed scope and Edit shows the year",
             [("expect The Legend of Zelda: A Link Between Worlds\nexpect-not Pikmin 4\n" + OPEN_FILTERS
               + "click Save as collection\nwait 500\nexpect Save the filters as a collection\n"
               + "type Year 2013\nkey Return\nwait 600\nexpect-has Collection Year 2013 saved from the filters\n"
               + OPEN_FILTERS + "click Clear filters\nwait 500\nexpect Pikmin 4\n"
               + "key Escape\nwait 400\n" + OPEN_COLL + "click Year 2013\nwait 500\n"
               + "expect The Legend of Zelda: A Link Between Worlds\nexpect-not Pikmin 4\nexpect-not Hades\n", None),
              (OPEN_COLL + "expect Year 2013\nclick Year 2013\nwait 500\n"
               + "expect The Legend of Zelda: A Link Between Worlds\nexpect-not Pikmin 4\n"
               + "key Escape\nwait 400\n" + OPEN_COLL + "click Year 2013\nwait 500\nkey Menu\nwait 400\n"
               + "click Edit\nwait 600\nexpect Edit collection\nexpect Released\nexpect-has 2013\n", None)],
             with_list("\n[list]\nyear_from = 2013\nyear_to = 2013\n")),
    Scenario("a saved minimum rating shows as a chip and in the column",
             [("expect-not Pikmin 4\nexpect Rated 3.5 or more\n" + OPEN_FILTERS + OPEN_MORE
               + "expect 3.5 or more\n", None)],
             with_list("\n[list]\nmin_rating = 3.5\n")),
]

sys.exit(run_scenarios(SCENARIOS))
