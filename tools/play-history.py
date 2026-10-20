#!/usr/bin/env python3
"""Gives every played game in a data dir a made-up play history, to fill the statistics page.

    python3 -I tools/play-history.py <data dir> [--until YYYY-MM-DD] [--months N] [--seed N]

Each game's sessions add up to three quarters of its play time (the rest counts as played
before the history began) and end at its last-played date. Games still being played (status
"playing", or last played within two weeks of the most recent game) keep going until --until
(default yesterday), so the current month has data; their last-played dates move there, in the
game file or, for Steam and Proton games, in the fake Steam's localconfig.vdf. Steam and Proton
games get a few sessions marked merged, as when Steam's play time rose while Lattis was closed.

Overwrites library/history/<id>.toml for those games; steam-seen.toml is left alone. Seeded, so
a rerun with the same arguments writes the same files.
"""
import argparse
import random
import re
import tomllib
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

COVERED = 0.75
MERGED_SHARE = 0.1


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("data_dir", type=Path)
    p.add_argument("--until", type=date.fromisoformat, default=date.today() - timedelta(days=1))
    p.add_argument("--months", type=int, default=16, help="how far back the history goes")
    p.add_argument("--seed", type=int, default=7)
    return p.parse_args()


def steam_config(data_dir):
    settings = tomllib.loads((data_dir / "settings.toml").read_text())
    path = settings.get("steam", {}).get("path")
    if not path:
        return None
    configs = sorted((data_dir / path).glob("userdata/*/config/localconfig.vdf"))
    return configs[0] if configs else None


def steam_apps(vdf_text):
    """{key: (playtime minutes, last played datetime or None)} from localconfig.vdf's apps."""
    apps = {}
    for m in re.finditer(r'"(-?\d+)"\s*\{([^{}]*)\}', vdf_text):
        playtime = re.search(r'"Playtime"\s+"(\d+)"', m.group(2))
        last = re.search(r'"LastPlayed"\s+"(\d+)"', m.group(2))
        if playtime:
            apps[m.group(1)] = (int(playtime.group(1)),
                                datetime.fromtimestamp(int(last.group(1))).astimezone() if last else None)
    return apps


def steam_key(game_id, game):
    if game_id.startswith("steam-"):
        return game_id.removeprefix("steam-")
    if "shortcut" in game:
        # Steam keeps a shortcut's play time under its id as a signed 32-bit number.
        n = int(game["shortcut"])
        return str(n - 2**32 if n >= 2**31 else n)
    return None


def as_datetime(value):
    if isinstance(value, datetime):
        return value if value.tzinfo else value.astimezone()
    return None


class Calendar:
    """Keeps a day's sessions from overlapping, across games."""

    def __init__(self, rng):
        self.rng = rng
        self.busy = {}

    def place(self, day, minutes, tz, fixed_end=None):
        for _ in range(40):
            if fixed_end:
                end = fixed_end
            else:
                afternoon = day.weekday() >= 5 and self.rng.random() < 0.4
                hour = self.rng.choice([14, 15, 16, 17] if afternoon else [19, 20, 21, 22, 23])
                end = datetime(day.year, day.month, day.day, hour, self.rng.randrange(60), tzinfo=tz) \
                    + timedelta(minutes=minutes)
            start = end - timedelta(minutes=minutes)
            taken = self.busy.setdefault(start.date(), [])
            if fixed_end or all(end <= s or start >= e for s, e in taken):
                taken.append((start, end))
                return end
        return None


def sessions_for(rng, calendar, total, first, last_day, last_end, tz, merged_share):
    target = int(total * COVERED)
    mean = min(150, max(25, total / 45))
    days = [first + timedelta(d) for d in range((last_day - first).days + 1)]
    weights = [1.6 if d.weekday() >= 5 else 1.0 for d in days]
    lengths = []
    while sum(lengths) < target:
        lengths.append(max(10, int(rng.gauss(mean, mean * 0.45))))
    lengths = lengths[:len(days)]
    scale = target / sum(lengths)
    lengths = [max(8, round(m * scale)) for m in lengths]
    picked = set()
    while len(picked) < len(lengths) - 1:
        picked.add(rng.choices(range(len(days) - 1), weights[:-1])[0])
    chosen = sorted(days[i] for i in picked) + [days[-1]]
    out = []
    for i, (day, minutes) in enumerate(zip(chosen, lengths)):
        is_last = i == len(chosen) - 1
        end = calendar.place(day, minutes, tz, last_end if is_last else None)
        if end is None:
            continue
        merged = not is_last and rng.random() < merged_share
        out.append((end, round(minutes * 1.8) if merged else minutes, merged))
    return sorted(out)


def write_history(path, sessions):
    blocks = []
    for end, minutes, merged in sessions:
        block = f"[[session]]\nend = {end.isoformat(timespec='seconds')}\nminutes = {minutes}\n"
        if merged:
            block += "merged = true\n"
        blocks.append(block)
    path.write_text("\n".join(blocks))


def main():
    args = parse_args()
    rng = random.Random(args.seed)
    calendar = Calendar(rng)
    library = args.data_dir / "library"
    since = (args.until.replace(day=1) - timedelta(days=30 * args.months)).replace(day=1)

    vdf_path = steam_config(args.data_dir)
    vdf_text = vdf_path.read_text() if vdf_path else ""
    apps = steam_apps(vdf_text)

    games = []
    for path in sorted((library / "games").glob("*.toml")):
        game = tomllib.loads(path.read_text())
        if game.get("hidden"):
            continue
        game_id = path.stem
        key = steam_key(game_id, game)
        if key is not None:
            total, last = apps.get(key, (0, None))
        else:
            total, last = game.get("play_minutes", 0), as_datetime(game.get("last_played"))
        if total > 0:
            games.append((game_id, path, game, key, total, last))

    newest = max(last for *_, last in games if last)
    history = library / "history"
    history.mkdir(exist_ok=True)
    for game_id, path, game, key, total, last in games:
        tz = last.tzinfo if last else datetime.now().astimezone().tzinfo
        current = last is not None and (game.get("status") == "playing" or last >= newest - timedelta(days=14))
        if current:
            last_day = args.until - timedelta(days=rng.randrange(3))
            last_end = None
        elif last is None:
            last_day = args.until - timedelta(days=rng.randrange(30, 240))
            last_end = None
        else:
            last_day, last_end = last.date(), last
        if last_day < since:
            continue
        span = min(300, max(10, round(total / 60 * 2.5)))
        first = max(since, last_day - timedelta(days=span))
        sessions = sessions_for(rng, calendar, total, first, last_day, last_end, tz,
                                MERGED_SHARE if key is not None else 0)
        write_history(history / f"{game_id}.toml", sessions)
        new_last = sessions[-1][0]
        if current:
            if key is None:
                text = path.read_text()
                path.write_text(re.sub(r"^last_played = .*$", f"last_played = {new_last.isoformat(timespec='seconds')}",
                                       text, count=1, flags=re.M))
            else:
                vdf_text = re.sub(rf'("{key}"\s*\{{[^{{}}]*?"LastPlayed"\s+")\d+(")',
                                  rf"\g<1>{int(new_last.timestamp())}\g<2>", vdf_text)
        print(f"{game_id}: {len(sessions)} sessions, {sum(m for _, m, _ in sessions)} of {total} min, "
              f"last {new_last.isoformat(timespec='minutes')}{' (moved)' if current else ''}")
    if vdf_path:
        vdf_path.write_text(vdf_text)


if __name__ == "__main__":
    main()
