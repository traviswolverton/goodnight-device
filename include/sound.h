#pragma once

#include <stdint.h>

// The passive piezo on kPinBuzzer: short tunes that say what a button
// press did (DESIGN.md "Sounds"). The server decides which to play and
// sends its name on bedside/sound; the sounds this firmware plays on its
// own are the tick when a hold becomes a long press and the easter egg
// (Mario, five stick clicks in a row -- main.cpp).
//
// Playback is non-blocking: soundPlay() starts a tune and soundUpdate(),
// called every loop(), steps through it. A new tune replaces whatever is
// still playing.

struct SoundSettings {
  bool enabled = true;
  uint8_t volume = 80;      // 0-100
  uint8_t darkVolume = 10;  // 0-100, while the room is dark (dimmer)
};

enum class Sound : uint8_t { Tick, Armed, Running, Success, Failure, Cancel, Mario };

void soundBegin(const SoundSettings& settings);
void soundApply(const SoundSettings& settings);
const SoundSettings& soundSettings();

// "armed", "running", "success", "failure", "cancel" (the bedside/sound
// names). False for anything else; Tick and Mario are device-only, so the
// server can't ask for them.
bool soundFromName(const char* name, Sound& out);

// Urgent sounds ignore the dark room and play at `volume` -- the server
// marks a failure urgent so it's heard at night. Disabled means silent,
// urgent or not.
void soundPlay(Sound sound, bool urgent);

void soundUpdate();
