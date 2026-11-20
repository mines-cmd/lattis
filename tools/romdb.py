#!/usr/bin/env python3
"""Builds the ROM name databases Lattis looks games up in (library/romdb.* in the app).

    tools/romdb.py <out dir> [platform ...]     # switch 3ds nds wii ps1 ps2 gba n64 nes snes genesis gb gbc sms
                                                # gamegear arcade; default: all
    tools/romdb.py --test                       # the parsers' own tests

One file per platform, `<out dir>/<platform>.tsv`: a first line starting with `#` that records each source's
URL and the date, then `<key><TAB><name>` lines sorted by key. The key is what the app reads from a game: the
Switch base title ID, the 3DS/DS/Wii/GameCube game code, the PlayStation serial, the CRC32 of a cartridge ROM
(GBA, Nintendo 64 big-endian, NES, SNES, Genesis, Game Boy, Game Boy Color, Master System, Game Gear), or an
arcade machine's name. Names are kept as the sources write them (`Legend of Zelda, The - Tri Force Heroes
(USA)`): the app tidies them with the same rules as file names. In arcade.tsv a machine that is a BIOS or a
device has the name `-`: the scan leaves its zip out instead of making it a game. Publish the files where
RomDbFetch::kDefaultUrl points, or anywhere LATTIS_ROMDB_URL names.

Sources and licences (also in docs/code/library.md): blawar/titledb (MIT), libretro-database's no-intro and
redump DATs (CC BY-SA 4.0; the data are No-Intro's and Redump's), GameTDB's text lists (free to use; credit
GameTDB), MAME's -listxml (the mameXXXXlx.zip release asset; only machine names and descriptions are kept).
Only Python's standard library is used.
"""
import datetime
import io
import json
import re
import sys
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

LIBRETRO = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/"
MAME_LISTXML = "https://github.com/mamedev/mame/releases/download/mame0289/mame0289lx.zip"
SKIP = "-"
SOURCES = {
    "switch": ["https://raw.githubusercontent.com/blawar/titledb/master/US.en.json"],
    "3ds": ["https://www.gametdb.com/3dstdb.txt"],
    "nds": ["https://www.gametdb.com/dstdb.txt"],
    "wii": ["https://www.gametdb.com/wiitdb.txt"],
    "ps1": [LIBRETRO + "redump/" + urllib.parse.quote("Sony - PlayStation.dat")],
    "ps2": [LIBRETRO + "redump/" + urllib.parse.quote("Sony - PlayStation 2.dat")],
    "gba": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Game Boy Advance.dat")],
    "n64": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Nintendo 64.dat")],
    "nes": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Nintendo Entertainment System.dat"),
            LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Family Computer Disk System.dat")],
    "snes": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Super Nintendo Entertainment System.dat")],
    "genesis": [LIBRETRO + "no-intro/" + urllib.parse.quote("Sega - Mega Drive - Genesis.dat")],
    "gb": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Game Boy.dat")],
    "gbc": [LIBRETRO + "no-intro/" + urllib.parse.quote("Nintendo - Game Boy Color.dat")],
    "sms": [LIBRETRO + "no-intro/" + urllib.parse.quote("Sega - Master System - Mark III.dat")],
    "gamegear": [LIBRETRO + "no-intro/" + urllib.parse.quote("Sega - Game Gear.dat")],
    "arcade": [MAME_LISTXML],
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
    """Cartridges: the CRC32 of each ROM, or only of those with this extension (a string or a tuple of them).
    N64 keeps the .z64 (big-endian) ones, the form the app hashes. libretro's NES list has each game twice: the
    `.nes` with its 16-byte iNES header (what a zip's stored CRC covers) and the `.unh` without it."""
    out = {}
    for name, _, roms in dat_games(text):
        for rom, crc in roms:
            if extension is None or rom.lower().endswith(extension):
                out.setdefault(crc.upper(), clean(name))
    return out


def parse_listxml(source):
    """MAME's -listxml (a path, bytes, text or a file): `<machine name="sf2"><description>...</description>`.
    Machines with no ROM or disk have no zip and are left out; BIOS, device and non-runnable machines
    (isbios, isdevice, runnable="no") map to SKIP, as their zips are not games."""
    if isinstance(source, str) and source.lstrip().startswith("<"):
        source = io.StringIO(source)
    elif isinstance(source, bytes):
        source = io.BytesIO(source)
    out = {}
    for _, element in ET.iterparse(source, events=("end",)):
        if element.tag != "machine":
            continue
        name = element.get("name")
        if name and (element.find("rom") is not None or element.find("disk") is not None):
            not_a_game = (element.get("isbios") == "yes" or element.get("isdevice") == "yes"
                          or element.get("runnable") == "no")
            description = clean(element.findtext("description") or "")
            if not_a_game:
                out.setdefault(name.upper(), SKIP)
            elif description:
                out.setdefault(name.upper(), description)
        element.clear()
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
    # One parser per source: the Famicom Disk System's list has the `.fds` dumps and, besides, `.qd` ones.
    "nes": [lambda text: parse_dat_crc(text, (".nes", ".unh")), lambda text: parse_dat_crc(text, ".fds")],
    "snes": lambda text: parse_dat_crc(text, ".sfc"),
    "genesis": parse_dat_crc,
    "gb": parse_dat_crc,
    "gbc": parse_dat_crc,
    "sms": parse_dat_crc,
    "gamegear": parse_dat_crc,
    "arcade": parse_listxml,
}


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (lattis romdb)"})
    with urllib.request.urlopen(request, timeout=600) as reply:
        body = reply.read()
    if url.endswith(".zip"):
        with zipfile.ZipFile(io.BytesIO(body)) as archive:
            name = next(n for n in archive.namelist() if n.endswith(".xml"))
            return archive.read(name)
    return body.decode("utf-8", "replace")


def build(platform):
    entries = {}
    parsers = PARSERS[platform]
    for index, url in enumerate(SOURCES[platform]):
        parse = parsers[index] if isinstance(parsers, list) else parsers
        for key, name in parse(fetch(url)).items():
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
    nes = 'game (\n\tname "Super Mario Bros. (World)"\n\trom ( name "Super Mario Bros. (World).nes" size 40976 crc 393A432F )\n)\n' \
          'game (\n\tname "Super Mario Bros. (World)"\n\trom ( name "Super Mario Bros. (World).unh" size 40960 crc 3337EC46 )\n)\n' \
          'game (\n\tname "Mapper chip (World)"\n\trom ( name "Mapper chip (World).bin" size 1024 crc 00000001 )\n)\n'
    assert parse_dat_crc(nes, (".nes", ".unh")) == {"393A432F": "Super Mario Bros. (World)",
                                                    "3337EC46": "Super Mario Bros. (World)"}
    fds = 'game (\n\tname "Zelda (Japan)"\n\trom ( name "Zelda (Japan).fds" size 131000 crc 11112222 )\n)\n' \
          'game (\n\tname "Zelda (Japan)"\n\trom ( name "Zelda (Japan).qd" size 131072 crc 33334444 )\n)\n'
    assert parse_dat_crc(fds, ".fds") == {"11112222": "Zelda (Japan)"}
    snes = 'game (\n\tname "Super Metroid (Japan, USA) (En,Ja)"\n\trom ( name "Super Metroid.sfc" size 3145728 crc D63ED5F8 )\n)\n' \
           'game (\n\tname "CX4 (World) (Enhancement Chip)"\n\trom ( name "CX4.bin" size 3072 crc 00000002 )\n)\n'
    assert parse_dat_crc(snes, ".sfc") == {"D63ED5F8": "Super Metroid (Japan, USA) (En,Ja)"}

    listxml = '<?xml version="1.0"?>\n<mame build="0.289">\n' \
              '<machine name="sf2" sourcefile="capcom/cps1.cpp"><description>Street Fighter II: The World Warrior (World 910522)</description>' \
              '<rom name="a.bin" size="1"/></machine>\n' \
              '<machine name="neogeo" isbios="yes"><description>Neo-Geo MV-6F</description><rom name="b.bin" size="1"/></machine>\n' \
              '<machine name="qsound" isdevice="yes" runnable="no"><description>Capcom QSound</description><rom name="c.bin" size="1"/></machine>\n' \
              '<machine name="ay8910" isdevice="yes" runnable="no"><description>AY-3-8910 PSG</description></machine>\n' \
              '<machine name="noroms"><description>No ROMs</description></machine>\n' \
              '<machine name="sf2ce" cloneof="sf2"><description>Street Fighter II\': Champion Edition (World 920513)</description>' \
              '<disk name="d" /></machine>\n</mame>\n'
    assert parse_listxml(listxml) == {"SF2": "Street Fighter II: The World Warrior (World 910522)", "NEOGEO": SKIP,
                                      "QSOUND": SKIP, "SF2CE": "Street Fighter II': Champion Edition (World 920513)"}, \
        parse_listxml(listxml)
    assert parse_listxml(listxml.encode()) == parse_listxml(listxml)
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
