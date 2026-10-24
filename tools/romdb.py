#!/usr/bin/env python3
"""Builds the ROM name databases Lattis looks games up in (library/romdb.* in the app).

    tools/romdb.py <out dir> [platform ...]     # switch 3ds nds wii ps1 ps2 gba n64; default: all
    tools/romdb.py --test                       # the parsers' own tests

One file per platform, `<out dir>/<platform>.tsv`: a first line starting with `#` that records each source's
URL and the date, then `<key><TAB><name>` lines sorted by key. The key is what the app reads from a game: the
Switch base title ID, the 3DS/DS/Wii/GameCube game code, the PlayStation serial, or the CRC32 of a GBA or
Nintendo 64 (big-endian) ROM. Names are kept as the sources write them (`Legend of Zelda, The - Tri Force
Heroes (USA)`): the app tidies them with the same rules as file names. Publish the files where
RomDbFetch::kDefaultUrl points, or anywhere LATTIS_ROMDB_URL names.

Sources and licences (also in docs/code/library.md): blawar/titledb (MIT), libretro-database's no-intro and
redump DATs (CC BY-SA 4.0; the data are No-Intro's and Redump's), GameTDB's text lists (free to use; credit
GameTDB). Only Python's standard library is used.
"""
import datetime
import json
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path

LIBRETRO = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/"
SOURCES = {
    "switch": ["https://raw.githubusercontent.com/blawar/titledb/master/US.en.json"],
    "3ds": ["https://www.gametdb.com/3dstdb.txt"],
    "nds": ["https://www.gametdb.com/dstdb.txt"],
    "wii": ["https://www.gametdb.com/wiitdb.txt"],
    "ps1": [LIBRETRO + "redump/" + urllib.parse.quote("Sony - PlayStation.dat")],
    "ps2": [LIBRETRO + "redump/" + urllib.parse.quote("Sony - PlayStation 2.dat")],
    "gba": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Game Boy Advance.dat")],
    "n64": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Nintendo 64.dat")],
}
PARSERS = {}


def clean(name):
    return re.sub(r"[™®©]", "", name).replace("\t", " ").replace("\n", " ").strip()


def parse_titledb(text):
    """blawar/titledb: {"<nsuId>": {"id": "01007EF00011E000", "name": "...", "isDemo": false, ...}}.
    Only base games (IDs ending in 000) and no demos."""
    out = {}
    for entry in json.loads(text).values():
        title_id = (entry.get("id") or "").upper()
        name = entry.get("name")
        if len(title_id) == 16 and title_id.endswith("000") and name and not entry.get("isDemo"):
            out.setdefault(title_id, clean(name))
    return out


def parse_gametdb(text):
    """GameTDB: `TITLES = https://www.gametdb.com (type: Wii ...)` then `ID = Name`."""
    out = {}
    for line in text.splitlines():
        key, sep, name = line.partition(" = ")
        if not sep or key == "TITLES" or not re.fullmatch(r"[0-9A-Za-z]{4,6}", key.strip()):
            continue
        out.setdefault(key.strip().upper(), clean(name))
    return out


def dat_games(text):
    """clrmamepro DATs: `game ( name "..." serial "..." rom ( name "x.ext" size N crc XXXXXXXX ... ) )`."""
    for block in re.split(r"^game \(", text, flags=re.MULTILINE)[1:]:
        name = re.search(r'^\s*name "(.*)"\s*$', block, re.MULTILINE)
        if not name:
            continue
        serials = re.findall(r'^\s*serial "(.*?)"', block, re.MULTILINE)
        roms = re.findall(r'rom \( name "(.*?)" size \d+ crc ([0-9A-Fa-f]{8})', block)
        yield name.group(1), serials, roms


def parse_dat_crc(text, extension=None):
    """GBA and N64: the CRC32 of each ROM; N64 keeps the .z64 (big-endian) ones, the form the app hashes."""
    out = {}
    for name, _, roms in dat_games(text):
        for rom, crc in roms:
            if extension is None or rom.lower().endswith(extension):
                out.setdefault(crc.upper(), clean(name))
    return out


def parse_dat_serial(text):
    """PlayStation: `serial "SLUS-20312"`; redump adds a disc index (`SLPM-65002-0`) and joins several with commas."""
    out = {}
    for name, serials, _ in dat_games(text):
        for serial in serials:
            for one in re.split(r"[,\s]+", serial):
                one = re.sub(r"(?<=\d)-\d$", "", one.upper())
                if re.fullmatch(r"[A-Z]{4}-\d{5}", one):
                    out.setdefault(one, clean(name))
    return out


PARSERS = {
    "switch": parse_titledb,
    "3ds": parse_gametdb,
    "nds": parse_gametdb,
    "wii": parse_gametdb,
    "ps1": parse_dat_serial,
    "ps2": parse_dat_serial,
    "gba": parse_dat_crc,
    "n64": lambda text: parse_dat_crc(text, ".z64"),
}


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (lattis romdb)"})
    with urllib.request.urlopen(request, timeout=120) as reply:
        return reply.read().decode("utf-8", "replace")


def build(platform):
    entries = {}
    for url in SOURCES[platform]:
        for key, name in PARSERS[platform](fetch(url)).items():
            entries.setdefault(key, name)
    return entries


def write(path, platform, entries):
    today = datetime.date.today().isoformat()
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("# romdb 1: %s, from %s, %s\n" % (platform, " ".join(SOURCES[platform]), today))
        for key in sorted(entries):
            out.write("%s\t%s\n" % (key, entries[key]))


def test():
    titledb = '{"1": {"id": "0100152000022000", "name": "Mario Kart 8 Deluxe", "isDemo": false},' \
              ' "2": {"id": "0100152000022800", "name": "Update"},' \
              ' "3": {"id": "01007EF00011E000", "name": "The Legend of Zelda™: Breath of the Wild"},' \
              ' "4": {"id": "0100AAAAAAAAA000", "name": "A Demo", "isDemo": true}, "5": {"id": null, "name": "x"}}'
    assert parse_titledb(titledb) == {"0100152000022000": "Mario Kart 8 Deluxe",
                                      "01007EF00011E000": "The Legend of Zelda: Breath of the Wild"}, parse_titledb(titledb)

    gametdb = "TITLES = https://www.gametdb.com (type: 3DS language: EN version: 1)\nA2AE = Pokémon Ultra Sun\nbad line\n" \
              "007E01 = Auto Erase Disc\n"
    assert parse_gametdb(gametdb) == {"A2AE": "Pokémon Ultra Sun", "007E01": "Auto Erase Disc"}

    dat = 'clrmamepro (\n\tname "x"\n)\n\ngame (\n\tname "Pokemon - Emerald Version (USA, Europe)"\n\tregion "USA"\n\tserial "BPEE"\n' \
          '\trom ( name "Pokemon - Emerald Version (USA, Europe).gba" size 16777216 crc 1F1C475A md5 AA sha1 BB serial "BPEE" )\n)\n' \
          'game (\n\tname "Zelda (USA)"\n\trom ( name "Zelda (USA).v64" size 1 crc 11111111 )\n)\n' \
          'game (\n\tname "Zelda (USA)"\n\trom ( name "Zelda (USA).z64" size 1 crc 22222222 )\n)\n'
    assert parse_dat_crc(dat) == {"1F1C475A": "Pokemon - Emerald Version (USA, Europe)", "11111111": "Zelda (USA)",
                                  "22222222": "Zelda (USA)"}
    assert parse_dat_crc(dat, ".z64") == {"22222222": "Zelda (USA)"}

    redump = 'game (\n\tname "0 Story (Japan) (Disc 1)"\n\tserial "SLPM-65002"\n\trom ( name "a.iso" size 1 crc 1C6ED37F )\n)\n' \
             'game (\n\tname "0 Story (Japan) (Disc 1)"\n\tserial "SLPM-65002-0"\n\trom ( name "a.iso" size 1 crc 1C6ED37F )\n)\n' \
             'game (\n\tname "Two (USA)"\n\tserial "SLUS-00001, SLUS-00002-1"\n\trom ( name "b.bin" size 1 crc 1C6ED37E )\n)\n' \
             'game (\n\tname "No serial"\n\trom ( name "c.bin" size 1 crc 1C6ED37D )\n)\n'
    assert parse_dat_serial(redump) == {"SLPM-65002": "0 Story (Japan) (Disc 1)", "SLUS-00001": "Two (USA)",
                                        "SLUS-00002": "Two (USA)"}, parse_dat_serial(redump)
    print("ok")


def main(argv):
    if argv[1:] == ["--test"]:
        test()
        return 0
    if len(argv) < 2:
        print(__doc__)
        return 2
    out = Path(argv[1])
    out.mkdir(parents=True, exist_ok=True)
    platforms = argv[2:] or list(SOURCES)
    for platform in platforms:
        if platform not in SOURCES:
            print("unknown platform %s (known: %s)" % (platform, " ".join(SOURCES)), file=sys.stderr)
            return 2
        entries = build(platform)
        write(out / (platform + ".tsv"), platform, entries)
        print("%s: %d entries" % (platform, len(entries)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
