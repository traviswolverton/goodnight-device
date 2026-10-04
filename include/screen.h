#pragma once

#include <stdint.h>

#include "input.h"
#include "provisioning.h"

// What the OLED shows: one big line, because a 128x32 panel is too small
// to read three. The first page is the clock; joystick left/right steps
// through temperature, humidity and light (and a fault page when hardware
// is missing), and it returns to the clock a few seconds after the last
// move. A button or stick press shows briefly in place of the page.

struct ScreenModel {
  float temperatureC;
  float humidity;
  float lux;
  bool wifi;
  bool mqtt;
  const char* hardwareNote;  // e.g. "no BH1750", or nullptr when all found
  bool timeValid;            // false until NTP has set the clock
  uint8_t hour12;            // 1-12
  uint8_t minute;
  bool pm;
  // While setup mode is on (view.active), it takes over the whole screen:
  // the phone-facing name, the passkey to type, and how it's going.
  ProvisioningView setup;
};

// Whether button and stick presses show on the screen (`BTN 3`, `UP`...).
// Off by default; the stick's left/right still change the page either way.
void screenSetInputEcho(bool on);

// Shows a short message in place of the page for a moment (keep it to
// about 8 characters: the font is big).
void screenFlash(const char* text);

// Tells the screen about a control event: stick left/right change the
// page, and with the echo on the rest are shown briefly.
void screenNoteInput(const InputEvent& event);

// Redraws only when what's on screen would actually change.
void screenUpdate(const ScreenModel& model);
