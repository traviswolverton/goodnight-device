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
};

void dimmerBegin(const DimmerSettings& settings);
void dimmerApply(const DimmerSettings& settings);
const DimmerSettings& dimmerSettings();

// Call every loop() with the current smoothed lux (NAN = unknown).
void dimmerUpdate(float lux);

// Call when any control is first touched. Returns true when the screen was
// off and this touch woke it -- the caller should then ignore the rest of
// that gesture. While already awake in the dark, a touch just restarts the
// wake timer and returns false.
bool dimmerWake();

DisplayLevel dimmerLevel();
const char* dimmerLevelName();
