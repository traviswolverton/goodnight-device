#pragma once

#include <stdint.h>

#include "input.h"

// What the OLED shows. For now this is a bench-bring-up view -- readings,
// connection state, and the last control touched -- so every part of the
// hardware can be checked on the desk. The real bedside layout (a clock
// and the room readings) comes later.

struct ScreenModel {
  float temperatureC;
  float humidity;
  float lux;
  bool wifi;
  bool mqtt;
  const char* hardwareNote;  // e.g. "no BH1750", or nullptr when all found
};

// The last control event, shown for a few seconds.
void screenNoteInput(const InputEvent& event);

// Redraws only when what's on screen would actually change.
void screenUpdate(const ScreenModel& model);
