import json
import os
import shutil
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import ROOT, Scenario, run_scenarios

# Text that comes from outside (a game's description, a Steam friend's name, IGDB's name for a game) starting
# with an <img> tag must be drawn as it is, not as rich text: rich text fetches the image, which tells the
# host who looks at what. Each scenario puts a local server's URL in the text; the server must see no request.


class Sink:
    """A local HTTP server that records every request it gets."""

    def __init__(self):
        self.requests = []
        requests = self.requests

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                requests.append(self.path)
                self.send_response(404)
                self.end_headers()

            def log_message(self, *args):
                pass

        self.server = HTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def hostile(self, rest):
        return f'<img src="http://127.0.0.1:{self.port}/image.png" width="16" height="16"> {rest}'

    def check(self, result, dirs):
        self.server.shutdown()
        self.server.server_close()
        return [f"the text fetched {path}" for path in self.requests]


def scenario(name, script, prepare):
    sink = Sink()
    return Scenario(name, [(script, sink.check)], lambda dirs: prepare(dirs, sink))


# --- A game's description (a file, or IGDB's summary) ---

DESCRIPTION_FILE = "library/games/switch-mario-kart-8-deluxe.toml"


def description(dirs, sink):
    dirs.use_demo()
    path = os.path.join(dirs.data, DESCRIPTION_FILE)
    with open(path) as f:
        lines = f.read().splitlines()
    start = next(i for i, line in enumerate(lines) if line.startswith("description"))
    end = next(i for i in range(start + 1, len(lines)) if lines[i].endswith('"""'))
    lines[start:end + 1] = ["description = '''" + sink.hostile("Kart racing.") + "'''"]
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


# --- A Steam friend's name ---

HADES = 1145360
NAMES = ["Mira", "Ben"]
IDS = [str(76561198000001000 + i) for i in range(len(NAMES))]


def write_json(path, value):
    with open(path, "w") as f:
        json.dump(value, f)


def friend(dirs, sink):
    dirs.use_demo()
    folder = os.path.join(dirs.root, "web")
    os.makedirs(folder)
    write_json(os.path.join(folder, "ISteamUser-GetFriendList-v1.json"),
               {"friendslist": {"friends": [{"steamid": i, "relationship": "friend"} for i in IDS]}})
    names = [sink.hostile(NAMES[0]), NAMES[1]]
    players = [{"steamid": i, "personaname": n, "avatarmedium": f"https://avatars.example/{i}_medium.jpg"}
               for i, n in zip(IDS, names)]
    players[0].update({"gameid": str(HADES), "gameextrainfo": "Hades"})
    write_json(os.path.join(folder, "ISteamUser-GetPlayerSummaries-v2.json"), {"response": {"players": players}})
    for i in IDS:
        write_json(os.path.join(folder, f"IPlayerService-GetOwnedGames-v1-{i}.json"),
                   {"response": {"game_count": 1, "games": [{"appid": HADES, "playtime_forever": 900}]}})
    dirs.env["LATTIS_STEAM_WEB_FIXTURES"] = folder
    dirs.write("credentials.toml", '[steam]\nweb_key = "TESTKEY"\n')


# --- IGDB's name for a game, in Look up missing info ---

FIXTURES = os.path.join(ROOT, "tests", "data", "metadata")


def igdb(dirs, sink):
    dirs.use_demo()
    dirs.write("roms/switch/Mario Tennis Aces [0100BDE00862A000][v0].nsp", b"")
    folder = os.path.join(dirs.root, "fixtures")
    shutil.copytree(FIXTURES, folder)
    path = os.path.join(folder, "igdb-search-mario-tennis-aces.json")
    with open(path) as f:
        data = json.load(f)
    data[0]["name"] = sink.hostile("Mario Tennis Aces")
    write_json(path, data)
    dirs.write("library/games/switch-mario-tennis-aces.toml", 'title = "Mario Tennis Aces"\nplatform = "switch"\n')
    dirs.env["LATTIS_METADATA_FIXTURES"] = folder


SCENARIOS = [
    scenario("a game's description that starts with an img tag fetches nothing",
             "expect Hades\nclick Mario Kart 8 Deluxe\nwait 1500\nexpect-has Kart racing.\nwait 1500\n",
             description),
    # The fetch starts 10 s after the first frame; the fixtures answer at once.
    scenario("a Steam friend's name that starts with an img tag fetches nothing",
             "expect Hades\nclick Hades\nwait 13000\nexpect View all\nexpect-has Mira\nwait 1500\n",
             friend),
    scenario("IGDB's name that starts with an img tag fetches nothing",
             "expect Mario Kart 8 Deluxe\nexpect Pikmin 4\nexpect Hades\n"
             "key Ctrl+Shift+L\nexpect Look up missing info\nclick Search titles\ntype Aces\nwait 500\n"
             "click Look up 1\nwait 1500\nexpect Review what was found\nexpect Mario Tennis Aces\nwait 1500\n",
             igdb),
]

sys.exit(run_scenarios(SCENARIOS))
