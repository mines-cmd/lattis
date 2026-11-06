import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

FIRST = "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
OPEN = FIRST + "key Ctrl+Comma\nwait 800\nexpect General\n"


def demo(dirs):
    dirs.use_demo()


def settings_has(*lines):
    def check(result, dirs):
        try:
            with open(os.path.join(dirs.data, "settings.toml")) as f:
                text = f.read()
        except OSError:
            return ["settings.toml wasn't written"]
        return [f"settings.toml lacks: {line}" for line in lines if line not in text]
    return check


THEMES = ("Moss", "Mythos", "Steam", "Catppuccin", "Gruvbox", "Rose Pine", "Nord", "Ember")
CUSTOMIZE = OPEN + "click Customize\nwait 800\n"


def style_has(*lines):
    def check(result, dirs):
        path = os.path.join(dirs.data, "themes", "styles", "Soft.toml")
        try:
            with open(path) as f:
                text = f.read()
        except OSError:
            return ["themes/styles/Soft.toml wasn't written"]
        return [f"Soft.toml lacks: {line}" for line in lines if line not in text]
    return check


def set_value(description, old, new):
    return (f"click Search settings\ntype {description}\nwait 800\nclick {old}\nkey Ctrl+A\ntype {new}\nkey Return\n"
            f"wait 500\nexpect {new}\nclick {description}\nkey Ctrl+A\nkey Delete\nwait 500\n")


SCENARIOS = [
    Scenario("Ctrl+Comma opens Settings, every category opens its page, Ctrl+W closes it",
             [(OPEN + "".join(f"click {name}\nwait 500\n" for name in (
                 "Game page", "Library", "Steam", "Heroic", "Mod managers", "Platforms", "Emulators", "Metadata",
                 "Appearance", "Customize", "Keyboard", "General"))
               + "key Ctrl+W\nwait 400\nexpect-not General\n" + FIRST, None)], demo),
    Scenario("General switches change live, persist in settings.toml and show the same after a restart",
             [(OPEN + "expect-off Show toolbar\nexpect-on Show rail\nexpect-off Reduce motion\n"
               + "toggle Show toolbar\ntoggle Show rail\ntoggle Reduce motion\ntoggle Start in fullscreen\n"
               + "toggle Use system locale\nexpect-on Show toolbar\nexpect-off Show rail\nexpect-on Reduce motion\n"
               + "wait 600\n",
               settings_has("toolbar = true", "rail_hidden = true", "reduce_motion = true", "start_fullscreen = true",
                            "system_locale = false")),
              (OPEN + "expect-on Show toolbar\nexpect-off Show rail\nexpect-on Reduce motion\n"
               + "expect-on Start in fullscreen\nexpect-off Use system locale\n"
               + "toggle Show toolbar\ntoggle Show rail\ntoggle Reduce motion\ntoggle Start in fullscreen\n"
               + "toggle Use system locale\nwait 600\n", None),
              (OPEN + "expect-off Show toolbar\nexpect-on Show rail\nexpect-off Reduce motion\n"
               + "expect-off Start in fullscreen\nexpect-on Use system locale\n", None)], demo),
    Scenario("every theme card applies its theme with no warnings, and the last one persists",
             [(OPEN + "click Appearance\nwait 500\n"
               + "".join(f"click {name}\nwait 600\nexpect This is the theme {name}\n" for name in THEMES)
               + "click Nord\nwait 600\nexpect This is the theme Nord\n",
               settings_has('colors = "Nord"')),
              (OPEN + "click Appearance\nwait 500\nexpect This is the theme Nord\n", None)], demo),
    Scenario("the style and colors lists switch the pair live, and it persists",
             [(OPEN + "click Appearance\nwait 500\nclick Soft\nwait 500\nclick Sharp\nwait 800\n"
               + "expect This pair isn't a saved theme\nclick Sharp\nwait 500\nclick Round\nwait 800\n",
               settings_has('style = "Round"')),
              (OPEN + "click Appearance\nwait 500\nexpect Round\n", None)], demo),
    Scenario("the radius tiers change live and persist in the style",
             [(CUSTOMIZE + set_value("Big panels", "12", "0") + set_value("Rows, tags, inputs", "6", "30")
               + set_value("Small details: icons", "4", "0")
               + "key Ctrl+W\nwait 400\n" + FIRST,
               style_has("large = 0", "small = 30", "tiny = 0")),
              (CUSTOMIZE + "click Search settings\ntype Big panels\nwait 800\nexpect 0\n", None)], demo),
    Scenario("gradient directions change with no warnings and persist",
             [(CUSTOMIZE + "click Search settings\ntype Default gradient direction\nwait 800\n"
               + "".join(f"click down\nwait 400\nclick {d}\nwait 500\nexpect {d}\n"
                         for d in ("up",))
               + "click up\nwait 400\nclick right\nwait 500\nclick right\nwait 400\nclick left\nwait 500\n"
               + "key Ctrl+W\nwait 400\n" + FIRST,
               style_has('gradient_direction = "left"'))], demo),
    Scenario("a followers link filters the list to the settings that follow it, Clear all brings it back",
             [(CUSTOMIZE + "click Search settings\ntype Default gradient direction\nwait 800\nclick 15 follow\nwait 800\n"
               + "expect Following\nexpect-has Following Layout\nclick Default gradient direction\nkey Ctrl+A\nkey Delete\n"
               + "wait 600\nexpect 15 settings\nexpect Rail\nexpect Launch button\nclick Clear all\nwait 600\n"
               + "expect-not Following\nexpect Palette\n", None)], demo),
    Scenario("the Show menu narrows Customize to colors or style, and back to all",
             [(CUSTOMIZE + "click Show\nwait 500\nclick Colors\nwait 600\nexpect Background\nexpect-not Large\n"
               + "click Style\nwait 600\nexpect Large\nexpect-not Background\n"
               + "click All\nwait 600\nexpect Background\nexpect Large\n", None)], demo),
    Scenario("the minimum session length persists",
             [(OPEN + "click 30\nkey Ctrl+A\ntype 45\nkey Return\nwait 800\n",
               settings_has("min_session_seconds = 45")),
              (OPEN + "expect 45\n", None)], demo),
]

sys.exit(run_scenarios(SCENARIOS))
