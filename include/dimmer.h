#pragma once

#include <stdint.h>

#include "display.h"

// Room light decides how bright the screen is:
//
//   lux >= brightLux            Full
//   darkLux <= lux < brightLux  Dim
//   lux < darkLux               Off -- until a control is touched, then Dim
//                               for wakeSeconds, then Off again
//
// Whatever the room light says, the screen also turns Off after
// idleOffMinutes with no activity (a control touched, or movement seen by
// the PIR). Any activity ends that: a touch wakes it (and, like a wake in
// the dark, is swallowed), movement just brings it back. 0 turns this off.
//
// No light sensor (or no reading yet) means Full: fail toward a readable
// screen, never toward a dark one nobody can explain.
//
// Each boundary has a little hysteresis so a room sitting right on a
// threshold doesn't flicker between two levels.

struct DimmerSettings {
  float brightLux = 20.0f;
  float darkLux = 2.0f;
  uint8_t dimContrast = 1;  // 0-255, see displaySetDimContrast()
  uint16_t wakeSeconds = 15;
  uint16_t idleOffMinutes = 15;  // 0 = never turn off for being idle
};

void dimmerBegin(const DimmerSettings& settings);
void dimmerApply(const DimmerSettings& settings);
const DimmerSettings& dimmerSettings();

// Call every loop() with the current smoothed lux (NAN = unknown).
void dimmerUpdate(float lux);

// Call when any control is first touched. Returns true when the screen was
// off (dark, or off for being idle) and this touch woke it -- the caller
// should then ignore the rest of that gesture. While already awake, a touch
// just restarts the timers and returns false.
bool dimmerWake();

// Call while the PIR sees movement. Restarts the idle timer, and brings the
// screen back if it was off for being idle (it stays off if the room is dark).
void dimmerMotion();

// True while the screen is off for being idle.
bool dimmerIdle();

DisplayLevel dimmerLevel();

// True while the room is below darkLux (whether or not a touch has woken
// the screen) -- when sound.h plays at its quieter dark volume.
bool dimmerRoomIsDark();
const char* dimmerLevelName();
