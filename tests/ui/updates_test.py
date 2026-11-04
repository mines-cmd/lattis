import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The daily check starts 10 s after the first frame.
CHECKED = "wait 11500\n"
ABOUT = "key F10\nwait 500\nclick About Lattis\nwait 600\nexpect Version 1.1.1\n"
UPDATE_NOTICE = "expect-has Update available\nexpect-has 99.0.0\nexpect Update\n"


def release(tag, assets=True):
    name = f"Lattis-{tag}-x86_64.AppImage"
    return json.dumps({
        "tag_name": f"v{tag}",
        "published_at": "2026-11-01T10:00:00Z",
        "body": "## What's new\n- a thing",
        "assets": [{"id": 1, "name": name, "size": 9, "digest": "sha256:" + "ab" * 32,
                    "browser_download_url": f"https://example.invalid/{name}"}] if assets else [],
    })


def scenario(name, steps, body=None, enabled=True):
    def setup(dirs):
        dirs.use_demo()
        if body is not None:
            dirs.write("../release.json", body)
        if enabled:
            appimage = os.path.join(dirs.root, "Lattis.AppImage")
            with open(appimage, "w") as f:
                f.write("old")
            dirs.env.update({
                "LATTIS_INSTALL_FIXTURES": os.path.join(dirs.root, "fixtures"),
                "LATTIS_UPDATE_RELEASE": os.path.join(dirs.root, "release.json"),
                "LATTIS_UPDATE_APPIMAGE": appimage,
            })

    return Scenario(name, steps, setup)


SCENARIOS = [
    scenario("a newer release shows the notice in About and the entry in the app menu",
             [(CHECKED + "key F10\nwait 500\nexpect-has Update to 99.0.0\nclick About Lattis\nwait 600\n"
               + UPDATE_NOTICE, None)],
             release("99.0.0")),
    scenario("the notice's Update button opens the update dialog",
             [(CHECKED + ABOUT + "click Update\nwait 800\nexpect-has 99.0.0\nexpect-not-has Update available\n", None)],
             release("99.0.0")),
    scenario("the same release shows no notice",
             [(CHECKED + "key F10\nwait 500\nexpect About Lattis\nexpect-not-has Update to\n"
               + "click About Lattis\nwait 600\nexpect Version 1.1.1\nexpect-not-has Update available\n", None)],
             release("0.0.1")),
    scenario("a release without an AppImage fails the check quietly",
             [(CHECKED + ABOUT + "expect-not-has Update available\n", None)],
             release("99.0.0", assets=False)),
    scenario("an unreadable answer fails the check quietly",
             [(CHECKED + ABOUT + "expect-not-has Update available\n", None)],
             "{ this is not json"),
    scenario("an empty answer fails the check quietly",
             [(CHECKED + ABOUT + "expect-not-has Update available\n", None)],
             ""),
    scenario("a missing release file fails the check quietly",
             [(CHECKED + ABOUT + "expect-not-has Update available\n", None)]),
    scenario("a build that can't update shows no notice, even with a newer release around",
             [(CHECKED + ABOUT + "expect-not-has Update available\n", None)],
             release("99.0.0"), enabled=False),
]

sys.exit(run_scenarios(SCENARIOS))
