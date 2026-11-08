import os
import sys
from datetime import datetime, timedelta

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The play history popup (opened by a key bound in [shortcuts]) and its Add session dialog, on Mario Tennis Aces,
# whose one session ended yesterday at 22:00 after an hour. Everything is set on yesterday, so the tests don't
# depend on the time of day; only the future refusal uses today.

HISTORY = os.path.join("library", "history", "switch-mario-tennis-aces.toml")
OPEN = "type Mario Tennis Aces\nwait 400\nclick Show not installed\nwait 400\nclick Mario Tennis Aces\nwait 300\nkey Ctrl+Alt+4\nwait 500\nexpect Total\n"
# The dialog opens with the start date focused. Tab goes date, time, Today, Yesterday, hours, minutes, end date, end time.
NEW = "click Add session\nwait 500\n"


def setup(dirs):
    dirs.use_demo()
    settings = os.path.join(dirs.data, "settings.toml")
    with open(settings) as f:
        text = f.read()
    dirs.write("settings.toml", text + '\n[shortcuts]\nplay_history = "Ctrl+Alt+4"\n')
    end = (datetime.now().astimezone() - timedelta(days=1)).replace(hour=22, minute=0, second=0, microsecond=0)
    dirs.write(HISTORY, f"[[session]]\nend = {end.isoformat()}\nminutes = 60\n")


def tab(n):
    return "key Tab\n" * n


def type_into(text):
    return f"key Ctrl+A\ntype {text}\n"


# Start yesterday at the time; focus ends on the hours field.
def start_at(time):
    return tab(1) + type_into(time) + tab(2) + "key Space\n" + tab(1)


def history_text(dirs):
    path = os.path.join(dirs.data, HISTORY)
    with open(path) as f:
        return f.read()


def added(result, dirs):
    text = history_text(dirs)
    return [] if text.count("[[session]]") == 2 and "manual = true" in text else [f"no manual session in: {text}"]


def not_added(result, dirs):
    text = history_text(dirs)
    return [] if text.count("[[session]]") == 1 and "manual" not in text else [f"history changed: {text}"]


SCENARIOS = [
    Scenario("the dialog starts at yesterday's start with an hour, and shows the result",
             [(OPEN + NEW + start_at("20:00") + "expect-has Yesterday, 20:00, 1h\nexpect 21:00\n", None)], setup),
    Scenario("a length ending where another session ends is refused",
             [(OPEN + NEW + start_at("20:00") + type_into("2") + "expect Another session of this game ends at that time\n"
               + "key Escape\n", None)], setup),
    Scenario("an overlapping session is warned about and can still be added",
             [(OPEN + NEW + start_at("20:00") + type_into("1") + tab(1) + type_into("30")
               + "expect-has Yesterday, 20:00, 1h 30m. Overlaps a session on\n"
               + "click Add\nwait 600\nexpect Session added\n", added)], setup),
    Scenario("editing the end works the length out",
             [(OPEN + NEW + start_at("20:00") + tab(3) + type_into("20:30")
               + "expect-has Yesterday, 20:00, 30m\nexpect 30\n", None)], setup),
    Scenario("changing the start keeps the length and moves the end",
             [(OPEN + NEW + start_at("20:00") + "key Shift+Tab\nkey Shift+Tab\nkey Shift+Tab\n"
               + type_into("19:00") + "expect-has Yesterday, 19:00, 1h\nexpect 20:00\n", None)], setup),
    Scenario("an end before the start is refused",
             [(OPEN + NEW + start_at("20:00") + tab(3) + type_into("19:00")
               + "expect The end must be after the start\n", None)], setup),
    Scenario("a length of nothing is refused",
             [(OPEN + NEW + start_at("20:00") + type_into("0") + tab(1) + type_into("0")
               + "expect The length must be at least a minute\n", None)], setup),
    Scenario("a start in the future is refused",
             [(OPEN + NEW + tab(1) + type_into("23:59") + tab(1) + "key Space\n"
               + "expect The start can't be in the future\n", None)], setup),
    Scenario("Add puts a hand-added session in the list with a toast, and the file",
             [(OPEN + NEW + start_at("20:00") + "click Add\nwait 600\nexpect Session added\n", added)], setup),
    Scenario("Undo on the toast takes the added session out again",
             [(OPEN + NEW + start_at("20:00") + "click Add\nwait 600\nexpect Session added\nclick Undo\nwait 600\n"
               + "expect-not Session added\n", not_added)], setup),
    Scenario("a Steam game has no Add session",
             [("type Hades\nwait 400\nclick Hades\nwait 300\nkey Ctrl+Alt+4\nwait 500\nexpect Total\n"
               + "expect-not Add session\n", None)], setup),
]

if __name__ == "__main__":
    sys.exit(run_scenarios(SCENARIOS))
