#pragma once

#include "dimmer.h"

// Thin wrapper around the ESP32's NVS-backed Preferences library, for the
// settings that should survive a reboot even when the server is
// unreachable. Today that's the dimmer: the server sends it on
// bedside/config (retained), and the last copy received is kept here so a
// power cut in the night doesn't bring the screen back at full brightness
// with defaults until the WiFi and broker come up.
//
// Loading falls back to the given defaults for anything never saved
// (first boot, or after a full chip erase).

DimmerSettings settingsLoadDimmer(const DimmerSettings& defaults);

// Writes only when something differs from what's stored, so a retained
// config re-delivered on every reconnect costs no flash writes.
void settingsSaveDimmer(const DimmerSettings& settings);
