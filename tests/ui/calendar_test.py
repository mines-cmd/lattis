import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# LATTIS_DATE_FIELD puts a date field in the window (Main.qml's dateFieldForScreenshot); "picked ..." shows its value.
# The limit is fixed so the tests don't depend on the day they run.


def field(when, *flags):
    return "/".join([when, "open", "max=2026-03-20", *flags])


def setup(locale, when, *flags):
    def run(dirs):
        dirs.write("library/library.toml", "version = 1\n")
        dirs.write("settings.toml", "[general]\nsystem_locale = true\n")
        dirs.env["LATTIS_DATE_FIELD"] = field(when, *flags)
        dirs.env["LC_ALL"] = locale
    return run


SUNDAY = "en_US.UTF-8"
MONDAY = "C.UTF-8"

SCENARIOS = [
    Scenario("the calendar opens on the field's month",
             [("expect February 2026\nexpect picked 2026-02-15 08:30\nexpect 15\n", None)],
             setup(SUNDAY, "2026-02-15 08:30")),
    # February 2022 starts on a Tuesday: the row before it holds Jan 30 and 31 from Sunday, only Jan 31 from Monday.
    Scenario("a Sunday-first locale shows two days of January before February 2022",
             [("expect February 2022\nexpect 30\nexpect 31\n", None)], setup(SUNDAY, "2022-02-15 08:30")),
    Scenario("a Monday-first locale shows one day of January before February 2022",
             [("expect February 2022\nexpect-not 30\nexpect 31\n", None)], setup(MONDAY, "2022-02-15 08:30")),
    Scenario("a day after the limit can't be picked, one before it can",
             [("expect March 2026\nclick 25\nexpect March 2026\nexpect picked 2026-03-15 08:30\n"
               + "click 18\nexpect-not March 2026\nexpect picked 2026-03-18 08:30\n", None)],
             setup(SUNDAY, "2026-03-15 08:30")),
    Scenario("a dimmed day of the next month can be picked",
             [("expect February 2026\nclick 5\nexpect-not February 2026\nexpect picked 2026-03-05 08:30\n", None)],
             setup(SUNDAY, "2026-02-15 08:30")),
    Scenario("Right moves a day across the month, Enter picks it and keeps the time",
             [("expect January 2026\nkey Right\nexpect February 2026\nkey Return\n"
               + "expect-not February 2026\nexpect picked 2026-02-01 08:30\n", None)],
             setup(SUNDAY, "2026-01-31 08:30")),
    Scenario("Down moves a week",
             [("key Down\nexpect February 2026\nkey Return\nexpect picked 2026-02-07 08:30\n", None)],
             setup(SUNDAY, "2026-01-31 08:30")),
    Scenario("Page Down moves a month and clamps the day",
             [("key PageDown\nexpect February 2026\nkey Return\nexpect picked 2026-02-28 08:30\n", None)],
             setup(SUNDAY, "2026-01-31 08:30")),
    Scenario("Page Down stops at the limit's month, Page Up goes back",
             [("key PageDown\nexpect March 2026\nkey PageDown\nexpect March 2026\nkey PageUp\nexpect February 2026\n"
               + "key Return\nexpect picked 2026-02-20 08:30\n", None)],
             setup(SUNDAY, "2026-03-15 08:30")),
    Scenario("Home goes to today, held at the limit",
             [("key Home\nexpect March 2026\nkey Return\nexpect picked 2026-03-20 08:30\n", None)],
             setup(SUNDAY, "2026-01-31 08:30")),
    Scenario("Escape closes it without changing the date, and the date field has the focus back",
             [("expect February 2026\nkey Escape\nexpect-not February 2026\nexpect picked 2026-02-15 08:30\n"
               + "key Backspace\nexpect nothing picked\n", None)],
             setup(SUNDAY, "2026-02-15 08:30")),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
