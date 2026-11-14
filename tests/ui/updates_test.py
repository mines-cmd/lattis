import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "CMakeLists.txt")) as f:
    VERSION = re.search(r"project\(Lattis VERSION ([\d.]+)", f.read()).group(1)

# The daily check starts 10 s after the first frame.
CHECKED = "wait 11500\n"
ABOUT = "key F10\nwait 500\nclick About Lattis\nwait 600\nexpect Version " + VERSION + "\n"
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


# Emulators Lattis installed, each with a newer release waiting in the fixtures.
EMULATORS = {
    "mgba": ("github-mgba-emu-mgba.json", "mGBA-0.10.5-appimage-x64.appimage", "mGBA-0.10.6-appimage-x64.appimage",
             "0.10.5", "0.10.6"),
    "duckstation": ("github-stenzek-duckstation.json", "DuckStation-x64.AppImage", "DuckStation-x64.AppImage",
                    "v0.1-1", "v0.1-2"),
}


def emulator_updates(*ids):
    def setup(dirs):
        dirs.use_demo()
        fixtures = os.path.join(dirs.root, "fixtures")
        os.makedirs(fixtures)
        applications = os.path.join(dirs.home, "Applications")
        os.makedirs(applications)
        records = ""
        for id in ids:
            release_file, old, new, old_tag, new_tag = EMULATORS[id]
            with open(os.path.join(applications, old), "w") as f:
                f.write("old")
            with open(os.path.join(fixtures, release_file), "w") as f:
                json.dump({"tag_name": new_tag, "body": f"## {id}\n- a change in {new_tag}",
                           "assets": [{"id": 2, "name": new, "size": 9, "updated_at": "2026-11-01T10:00:00Z",
                                       "browser_download_url": f"https://example.invalid/{new}"}]}, f)
            records += (f'[{id}]\nroute = "appimage"\nfile = "{os.path.join(applications, old)}"\ntag = "{old_tag}"\n'
                        'asset_id = "1"\nasset_date = "2026-10-01T10:00:00Z"\n\n')
        state = os.path.join(dirs.root, "state", "lattis")
        os.makedirs(state)
        with open(os.path.join(state, "installs.toml"), "w") as f:
            f.write(records)
        dirs.env["LATTIS_INSTALL_FIXTURES"] = fixtures

    return setup


SCENARIOS = [
    Scenario("several emulator updates open one dialog with the notes and update one after another",
             [(CHECKED + "key F10\nwait 500\nclick Update 2 emulators\nwait 800\nexpect-has Update 2 emulators\n"
               + "expect-has a change in v0.1-2\nexpect-not-has a change in 0.10.6\n"
               + "click mGBA\nwait 500\nexpect-has a change in 0.10.6\nexpect-not-has a change in v0.1-2\n"
               + "click Update all\nwait 1200\nwait 7000\nexpect Done\nexpect-has Updated\n"
               + "click Done\nwait 600\nkey F10\nwait 500\nexpect-not-has Update 2 emulators\nexpect-not-has Updating\n", None)],
             emulator_updates("mgba", "duckstation")),
    Scenario("one emulator update shows its notes and a single Update button",
             [(CHECKED + "key F10\nwait 500\nclick Update mGBA to 0.10.6\nwait 800\nexpect-has Update mGBA to 0.10.6\n"
               + "expect-has a change in 0.10.6\nclick Update\nwait 5000\nexpect Done\n"
               + "expect-has Updated to 0.10.6\n", None)],
             emulator_updates("mgba")),
    scenario("a newer release shows the notice in About and the entry in the app menu",
             [(CHECKED + "key F10\nwait 500\nexpect-has Update to 99.0.0\nclick About Lattis\nwait 600\n"
               + UPDATE_NOTICE, None)],
             release("99.0.0")),
    scenario("the notice's Update button opens the update dialog",
             [(CHECKED + ABOUT + "click Update\nwait 800\nexpect-has 99.0.0\nexpect-not-has Update available\n", None)],
             release("99.0.0")),
    scenario("the same release shows no notice",
             [(CHECKED + "key F10\nwait 500\nexpect About Lattis\nexpect-not-has Update to\n"
               + "click About Lattis\nwait 600\nexpect Version " + VERSION + "\nexpect-not-has Update available\n", None)],
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
