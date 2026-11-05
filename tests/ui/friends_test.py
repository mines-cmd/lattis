import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Scenario, run_scenarios

# The demo's Steam games: Hades 1145360. Friends 0 is in the game, 1-2 played it recently, 3-4 before.
HADES = 1145360
GAMES = [
    {"appid": HADES, "playtime_forever": 900, "playtime_2weeks": 120},
    {"appid": HADES, "playtime_forever": 3000, "playtime_2weeks": 985},
    {"appid": HADES, "playtime_forever": 1500, "playtime_2weeks": 60},
    {"appid": HADES, "playtime_forever": 400},
    {"appid": HADES, "playtime_forever": 100},
]
NAMES = ["Mira", "Ben", "Kasper", "Noor", "Tomasz"]
IDS = [str(76561198000001000 + i) for i in range(len(NAMES))]

# The fetch starts 10 s after the first frame; the fixtures answer at once.
FETCHED = "wait 13000\n"
FIRST = "expect Hades\n"
# Searched, so the three rows are in view.
SETTINGS = ("key Ctrl+Comma\nwait 800\nexpect General\nclick Game page\nwait 500\n"
            "click Search the game page settings\ntype Friends\nwait 500\n")


def write_json(path, value):
    with open(path, "w") as f:
        json.dump(value, f)


def setup(key=True, steam="", game_page=""):
    def run(dirs):
        dirs.use_demo()
        folder = os.path.join(dirs.root, "web")
        os.makedirs(folder)
        write_json(os.path.join(folder, "ISteamUser-GetFriendList-v1.json"),
                   {"friendslist": {"friends": [{"steamid": i, "relationship": "friend"} for i in IDS]}})
        players = [{"steamid": i, "personaname": n, "avatarmedium": f"https://avatars.example/{n.lower()}_medium.jpg"}
                   for i, n in zip(IDS, NAMES)]
        players[0].update({"gameid": str(HADES), "gameextrainfo": "Hades"})
        write_json(os.path.join(folder, "ISteamUser-GetPlayerSummaries-v2.json"), {"response": {"players": players}})
        for i, game in zip(IDS, GAMES):
            write_json(os.path.join(folder, f"IPlayerService-GetOwnedGames-v1-{i}.json"),
                       {"response": {"game_count": 1, "games": [game]}})
        dirs.env["LATTIS_STEAM_WEB_FIXTURES"] = folder
        if key:
            dirs.write("credentials.toml", '[steam]\nweb_key = "TESTKEY"\n')
        path = os.path.join(dirs.data, "settings.toml")
        with open(path) as f:
            text = f.read()
        # The demo's [steam] section is already there: a second one would be a TOML error.
        with open(path, "w") as f:
            f.write(text.replace('path = "steam"', 'path = "steam"' + steam) + game_page)
    return run


def settings_has(*lines):
    def check(result, dirs):
        with open(os.path.join(dirs.data, "settings.toml")) as f:
            text = f.read()
        return [f"settings.toml lacks: {line}" for line in lines if line not in text]
    return check


SCENARIOS = [
    Scenario("a Steam game's page lists the friends who play it, by group",
             [("click Hades\n" + FETCHED + "expect View all\nexpect Playing now\nexpect 2 friends have played recently\n"
               "expect 2 friends have played previously\nexpect Ben\nexpect Mira\nexpect-not Tomasz\n", None)],
             setup()),
    Scenario("no panel on a game that isn't Steam's",
             [("click Hades\n" + FETCHED + "expect View all\nclick Mario Kart 8 Deluxe\nwait 500\n"
               "expect Mario Kart 8 Deluxe\nexpect-not View all\nexpect-not Playing now\n", None)],
             setup()),
    Scenario("no key, no panel",
             [("click Hades\n" + FETCHED + "expect Hades\nexpect-not View all\nexpect-not Playing now\n", None)],
             setup(key=False)),
    Scenario("the switch in settings.toml turns the fetch off",
             [("click Hades\n" + FETCHED + "expect Hades\nexpect-not View all\n", None)],
             setup(steam="\nfriends_activity = false")),
    Scenario("a group that is off is left out of the panel; with all three off the panel is gone",
             [("click Hades\n" + FETCHED + "expect View all\nexpect Playing now\n"
               + SETTINGS + "toggle Friends playing now\nwait 300\nexpect-off Friends playing now\n"
               "expect 3 friends have played recently\nexpect 2 friends have played previously\n"
               "toggle Friends who played recently\nwait 300\nexpect-not 3 friends have played recently\n"
               "toggle Friends who played previously\nwait 300\nexpect-off Friends who played previously\n"
               "expect-not View all\nexpect-not friends have played\n"
               "key Ctrl+W\nwait 500\nexpect Hades\nexpect-not View all\n",
               settings_has('friends_groups = []'))],
             setup()),
    Scenario("the chip and the info row count the friends who played, whatever the groups; none on a non-Steam game",
             [("click Hades\n" + FETCHED + "expect FRIENDS\nexpect 5 play\nexpect Friends\n"
               "click Mario Kart 8 Deluxe\nwait 500\nexpect Mario Kart 8 Deluxe\nexpect-not 5 play\n", None)],
             setup(game_page='\n[game_page]\nchips = ["friends_playing"]\ninfo_rows = ["friends_playing"]\n'
                             'friends_groups = []\n')),
    Scenario("Settings → Game page lists the friends chip and row and the three groups",
             [("expect Hades\nkey Ctrl+Comma\nwait 800\nexpect General\nclick Game page\nwait 500\n"
               "click Search the game page settings\ntype Friends who play\nwait 500\n"
               "expect Friends who played recently\nexpect Friends who played previously\n", None)],
             setup()),
    Scenario("no chip without a key or with the switch off",
             [("click Hades\n" + FETCHED + "expect Hades\nexpect-not 5 play\n", None)],
             setup(key=False, game_page='\n[game_page]\nchips = ["friends_playing"]\ninfo_rows = ["friends_playing"]\n')),
    Scenario("friends_groups in settings.toml picks the groups and survives a restart",
             [("click Hades\n" + FETCHED + "expect View all\nexpect 3 friends have played recently\n"
               "expect-not Playing now\nexpect-not friends have played previously\n", None),
              ("click Hades\nwait 800\nexpect 3 friends have played recently\nexpect-not Playing now\n", None)],
             setup(game_page='\n[game_page]\nfriends_groups = ["recent"]\n')),
]

sys.exit(run_scenarios(SCENARIOS))
