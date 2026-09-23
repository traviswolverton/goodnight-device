#pragma once

#include <stdint.h>

// Thin wrapper around the ESP32's NVS-backed Preferences library, for the
// two Configs-menu values that are meant to survive a reboot: backlight
// timeout and card sensitivity. See DESIGN.md's "Backlight / presence"
// and "Configs" sections for why those two (no safety angle either way)
// and NOT mute/pin (deliberately reset every boot -- fail toward showing
// alerts, not silently staying muted after an unattended reboot). Keep
// it that way: don't add a persist function here for mute/pin without
// re-reading that reasoning first.
//
// Each getter falls back to the given default when nothing's been saved
// yet (first boot, or after a full chip erase) -- callers don't need to
// special-case "no value stored" themselves.

uint16_t settingsLoadBacklightTimeoutSeconds(uint16_t defaultValue);
void settingsSaveBacklightTimeoutSeconds(uint16_t seconds);

int32_t settingsLoadCardStepDivisor(int32_t defaultValue);
void settingsSaveCardStepDivisor(int32_t divisor);
