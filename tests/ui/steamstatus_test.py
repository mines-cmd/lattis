import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios
import steamfake

LOG = "steam-args.txt"


def scenario(name, script, state="online", running=True, steam_enabled=True, expect_urls=(), expect_no_urls=()):
    procs = []

    def setup(dirs):
        dirs.use_demo()
        steamfake.add_status(dirs.data, state)
        if not steam_enabled:
            dirs.write("settings.toml", open(os.path.join(dirs.data, "settings.toml")).read() + "\n[steam]\nenabled = false\n")
        steamfake.fake_steam_command(dirs.bin, dirs.data, os.path.join(dirs.root, LOG))
        if running:
            procs.append(steamfake.start_steam(dirs.data, dirs.home))

    def check(result, dirs):
        for proc in procs:
            proc.kill()
            proc.wait()
        try:
            with open(os.path.join(dirs.root, LOG)) as f:
                urls = f.read().split()
        except FileNotFoundError:
            urls = []
        found = [f"steam wasn't handed {url}" for url in expect_urls if url not in urls]
        found += [f"steam was handed {url}" for url in expect_no_urls if url in urls]
        return found

    return Scenario(name, [(script, check)], setup)


SCENARIOS = [
    scenario("friends button shows the status in its tooltip text",
             "wait 500\nexpect Steam friends: Online\n", state="online"),
    scenario("away",
             "wait 500\nexpect Steam friends: Away\n", state="away"),
    scenario("invisible",
             "wait 500\nexpect Steam friends: Invisible\n", state="invisible"),
    scenario("no status key: no status in the text",
             "wait 500\nexpect Steam friends\nexpect-not Steam friends: Online\n", state=None),
    scenario("Steam closed: no status, statuses disabled, Open friends list still works",
             "wait 500\nexpect Steam friends\nexpect-not Steam friends: Online\n"
             "click Steam friends\nwait 400\nexpect Open friends list\nclick Away\nwait 400\n"
             "click Open friends list\nwait 600\n",
             running=False, expect_urls=["steam://open/friends"], expect_no_urls=["steam://friends/status/away"]),
    scenario("choosing Away hands Steam the URL, and the status follows the file",
             "wait 500\nexpect Steam friends: Online\nclick Steam friends: Online\nwait 400\nclick Away\nwait 1500\n"
             "wait 500\nexpect Steam friends: Away\n",
             expect_urls=["steam://friends/status/away"]),
    scenario("no friends button while Steam is off",
             "wait 500\nexpect Tetris\nexpect-not Steam friends\nexpect-not Steam friends: Online\n",
             steam_enabled=False),
]

sys.exit(run_scenarios(SCENARIOS))
