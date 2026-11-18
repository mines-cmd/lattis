#pragma once

#include "heroicgameconfig.h"
#include "steampresets.h"

// See docs/code/heroic.md.
namespace HeroicPresets {
bool isOn(const HeroicGameSettings &settings, const SteamPreset &preset);
void turnOn(HeroicGameSettings &settings, const SteamPreset &preset);
void turnOff(HeroicGameSettings &settings, const SteamPreset &preset);
}
