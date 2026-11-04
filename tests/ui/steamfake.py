"""The demo library's fake Steam made to look signed in: a status in localconfig.vdf, a profile picture, a running client.

`add_status` works on any data dir that already holds the demo; the UI tests and the screenshots both use it.
"""
import os
import shutil
import struct
import subprocess
import zlib

ACCOUNT = "39734273"
STEAMID64 = 76561197960265728 + int(ACCOUNT)
PERSONA = {"online": 1, "away": 3, "invisible": 7}
CONFIG = os.path.join("steam", "userdata", ACCOUNT, "config", "localconfig.vdf")


def png(size=64):
    """A gradient square, as a PNG."""
    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            rows += bytes((40 + 180 * x // size, 90 + 100 * y // size, 200 - 120 * x // size))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows))) + chunk(b"IEND", b"")


def add_status(data, state, avatar=True, toolbar=True):
    """Writes the status (None: no key) and the picture into the demo's Steam under `data`, and turns the toolbar on."""
    path = os.path.join(data, CONFIG)
    with open(path) as f:
        text = f.read()
    if state:
        block = '\t"WebStorage"\n\t{\n\t\t"FriendStoreLocalPrefs_%s"\t\t"{\\"ePersonaState\\":%d}"\n\t}\n' % (
            ACCOUNT, PERSONA[state])
        text = text[:text.rindex("}")] + block + "}\n"
    with open(path, "w") as f:
        f.write(text)
    if avatar:
        folder = os.path.join(data, "steam", "config", "avatarcache")
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, "%d.png" % STEAMID64), "wb") as f:
            f.write(png())
    if toolbar:
        with open(os.path.join(data, "settings.toml"), "a") as f:
            f.write("\n[general]\ntoolbar = true\n")


def start_steam(data, home):
    """A process whose executable is the fake client's, with the pid file Steam writes; returns it to kill."""
    folder = os.path.join(data, "steam", "ubuntu12_32")
    os.makedirs(folder, exist_ok=True)
    exe = os.path.join(folder, "steam")
    shutil.copy(shutil.which("sleep"), exe)
    os.makedirs(os.path.join(home, ".steam"), exist_ok=True)
    proc = subprocess.Popen([exe, "300"])
    with open(os.path.join(home, ".steam", "steam.pid"), "w") as f:
        f.write(str(proc.pid))
    return proc


def fake_steam_command(bin_dir, data, log):
    """A `steam` on PATH: logs its arguments, and a status URL rewrites the status the way Steam does."""
    script = os.path.join(bin_dir, "steam")
    config = os.path.join(data, CONFIG)
    with open(script, "w") as f:
        f.write("#!/bin/sh\n")
        f.write('echo "$@" >> "%s"\n' % log)
        for name, value in PERSONA.items():
            f.write('[ "$1" = "steam://friends/status/%s" ] && sed -i \'s/ePersonaState\\\\":[0-9]*/ePersonaState\\\\":%d/\' "%s"\n'
                    % (name, value, config))
        f.write("exit 0\n")
    os.chmod(script, 0o755)
