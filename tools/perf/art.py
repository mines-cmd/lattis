#!/usr/bin/env python3
"""Gives every game in a data dir realistic art, for the performance review.

    python3 -I tools/perf/art.py <data dir>

library_bench's art files are empty. This makes one hero (1920x620 JPEG), cover (600x900
JPEG), logo (800x300 PNG with transparency) and icon (256x256 PNG) once, and copies them
into art/<game id>/ for every game that has a file in library/games/ or an art folder,
replacing what's there. Each image is noise over a gradient, so it doesn't compress to
nothing and costs what a photo-like image costs to decode.
"""
import os
import random
import shutil
import sys
import tempfile

from PIL import Image, ImageDraw


def noisy(size, seed, alpha=False):
    rng = random.Random(seed)
    w, h = size
    base = Image.new("RGB", (w, h))
    px = base.load()
    c1 = tuple(rng.randrange(256) for _ in range(3))
    c2 = tuple(rng.randrange(256) for _ in range(3))
    for y in range(h):
        for x in range(w):
            t = (x / w + y / h) / 2
            px[x, y] = tuple(int(a + (b - a) * t) for a, b in zip(c1, c2))
    noise = Image.effect_noise((w, h), 40).convert("RGB")
    img = Image.blend(base, noise, 0.25)
    if alpha:
        img = img.convert("RGBA")
        mask = Image.new("L", (w, h), 0)
        ImageDraw.Draw(mask).rounded_rectangle((w // 8, h // 4, w * 7 // 8, h * 3 // 4), radius=h // 8, fill=255)
        img.putalpha(mask)
    return img


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    data = sys.argv[1]
    games = set()
    games_dir = os.path.join(data, "library", "games")
    if os.path.isdir(games_dir):
        games |= {os.path.splitext(f)[0] for f in os.listdir(games_dir) if f.endswith(".toml")}
    art_dir = os.path.join(data, "art")
    if os.path.isdir(art_dir):
        games |= {d for d in os.listdir(art_dir) if os.path.isdir(os.path.join(art_dir, d))}
    if not games:
        sys.exit("no games found in " + data)

    with tempfile.TemporaryDirectory() as tmp:
        files = {
            "hero.jpg": lambda: noisy((1920, 620), 1).save(os.path.join(tmp, "hero.jpg"), quality=88),
            "cover.jpg": lambda: noisy((600, 900), 2).save(os.path.join(tmp, "cover.jpg"), quality=88),
            "logo.png": lambda: noisy((800, 300), 3, alpha=True).save(os.path.join(tmp, "logo.png")),
            "icon.png": lambda: noisy((256, 256), 4).save(os.path.join(tmp, "icon.png")),
        }
        for make in files.values():
            make()
        for game in sorted(games):
            folder = os.path.join(art_dir, game)
            if os.path.isdir(folder):
                shutil.rmtree(folder)
            os.makedirs(folder)
            for name in files:
                shutil.copy(os.path.join(tmp, name), os.path.join(folder, name))
    print(f"art for {len(games)} games in {art_dir}")


main()
