#pragma once

#include <stdint.h>

// Presence-driven backlight, independent of screen state -- runs the
// same in HOME, MENU, and CAMERA. See DESIGN.md's "Backlight /
// presence" section.
void backlightBegin();

// Call every loop(). Reads the PIR pin directly and drives the
// backlight pin; doesn't touch screen state at all.
void backlightUpdate();

// Call whenever the encoder produces real input (rotation or a press).
// Extends the backlight's on-timer the same way PIR motion does -- the
// backlight stays lit on EITHER signal, whichever happened more
// recently, not just PIR. Without this, actively using the menu with a
// hand on the encoder (but not otherwise moving enough to retrigger the
// PIR) could dim the screen out from under you mid-interaction.
void backlightNotifyActivity();

// Adjustable range for the Configs menu -- exported so screen.cpp
// clamps against the same numbers this file enforces internally,
// rather than duplicating them and risking drift between the two.
constexpr uint16_t kBacklightMinTimeoutSeconds = 5;
constexpr uint16_t kBacklightMaxTimeoutSeconds = 120;

// Current timeout, in whole seconds, for the Configs menu to display.
uint16_t backlightGetTimeoutSeconds();

// Adjusts the timeout, clamped to [kBacklightMinTimeoutSeconds,
// kBacklightMaxTimeoutSeconds]. Does not persist across a reboot --
// resets to the compiled-in default every boot, same "fail toward
// showing" reasoning as mute/pin (see DESIGN.md).
void backlightSetTimeoutSeconds(uint16_t seconds);
