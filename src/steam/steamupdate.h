#pragma once

#include <QtGlobal>

// See docs/code/steam.md.
enum class SteamUpdate : qint8 { None, Waiting, Downloading, Paused };