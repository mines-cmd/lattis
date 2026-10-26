#!/usr/bin/env python3
"""Writes the achievement fixtures next to this script (python3 make-fixtures.py).

Made up, not copied from a real Steam install. App 9001 has four achievements over two
stats (bit numbers restart in each), one hidden, `type` as text in one stat and as 4 in the
other, and progress for user 39734273. App 9002 has a schema and no progress file; app 9003
has a schema without achievements.
"""
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
USER = 39734273


def cstr(text):
    return text.encode("utf-8") + b"\0"


def kv_map(key, *children):
    return b"\x00" + cstr(key) + b"".join(children) + b"\x08"


def kv_str(key, value):
    return b"\x01" + cstr(key) + cstr(value)


def kv_int(key, value):
    return b"\x02" + cstr(key) + struct.pack("<i", value)


def texts(**by_language):
    return [kv_str(language, text) for language, text in by_language.items()]


def achievement(bit, name, display_name, desc, icon, hidden=0):
    return kv_map(
        str(bit),
        kv_str("name", name),
        kv_map("display",
               kv_map("name", kv_str("token", name + "_NAME"), *texts(**display_name)),
               kv_map("desc", kv_str("token", name + "_DESC"), *texts(**desc)),
               kv_int("hidden", hidden),
               kv_str("icon", icon + ".jpg"),
               kv_str("icon_gray", icon + "_gray.jpg")))


def schema(app_id, game, *stats):
    return kv_map(str(app_id), kv_str("gamename", game), kv_int("version", 3), kv_map("stats", *stats))


def progress(*stats):
    cache = [kv_int("crc", 123456789), kv_int("PendingChanges", 0)]
    return kv_map("cache", *cache, *stats)


def stat_progress(stat, mask, times):
    return kv_map(str(stat), kv_int("data", mask),
                  kv_map("AchievementTimes", *[kv_int(str(bit), when) for bit, when in times.items()]))


def write(name, body):
    with open(os.path.join(HERE, name), "wb") as f:
        f.write(body + b"\x08")


write("UserGameStatsSchema_9001.bin", schema(
    9001, "Fixture Quest",
    kv_map("0", kv_str("type", "ACHIEVEMENTS"), kv_map(
        "bits",
        achievement(0, "FIRST_STEP", dict(english="First Step", german="Erster Schritt"),
                    dict(english="Leave the village.", german="Verlasse das Dorf."), "firststep"),
        achievement(1, "SECRET_ROOM", dict(english="Secret Room", german="Geheimer Raum"),
                    dict(english="Find the hidden room.", german="Finde den geheimen Raum."), "secretroom",
                    hidden=1))),
    kv_map("1", kv_int("type", 4), kv_map(
        "bits",
        achievement(0, "LONG_WALK", dict(english="Long Walk", german="Langer Weg"),
                    dict(english="Walk 10 km.", german="Gehe 10 km."), "longwalk"),
        achievement(1, "FINALE", dict(french="Finale"), dict(french="Termine le jeu."), "finale"))),
    kv_map("2", kv_str("type", "INT"), kv_str("name", "STEPS"))))

# Stat 0 has bit 0 set; stat 1 has bit 0 set too, later: the same bit number in the second stat.
write(f"UserGameStats_{USER}_9001.bin", progress(
    stat_progress(0, 0b01, {0: 1700000000}),
    stat_progress(1, 0b01, {0: 1750000000})))

write("UserGameStatsSchema_9002.bin", schema(
    9002, "No Progress", kv_map("0", kv_str("type", "ACHIEVEMENTS"), kv_map(
        "bits", achievement(0, "ONLY_ONE", dict(english="Only One"), dict(english="Do the thing."), "onlyone")))))

write("UserGameStatsSchema_9003.bin", schema(
    9003, "No Achievements", kv_map("0", kv_str("type", "INT"), kv_str("name", "SCORE"))))
