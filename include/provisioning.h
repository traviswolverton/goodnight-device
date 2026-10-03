#pragma once

#include <stdint.h>

#include "wifi_config.h"

// Setting the WiFi network from a phone over Bluetooth LE (DESIGN.md,
// "WiFi provisioning"). In setup mode the device advertises as
// "Bedside-XXXX" and shows a 6-digit passkey on the OLED. The phone pairs
// (typing that passkey), writes the SSID and password to two
// characteristics, then writes the Apply characteristic; the result comes
// back on a Status characteristic. The credentials are a trial in
// wifi_config until a connection succeeds -- a wrong password changes
// nothing.
//
// BLE is only up while setup mode is. When it ends, the whole stack is
// shut down and its memory returned.
//
// Setup mode starts when there are no credentials at all, or the
// joystick is held pressed at power-up (see main.cpp). It ends when a trial
// succeeds, or after kIdleTimeoutMs with no phone activity -- except with
// no credentials at all, where it starts over (there is nothing else to
// do without a network).

struct ProvisioningView {
  bool active;
  const char* name;     // what the phone sees, e.g. "Bedside-A1B2"
  uint32_t passkey;     // 6 digits, shown on the OLED
  uint16_t secondsLeft; // until the idle timeout
  const char* status;   // short, fits the OLED
};

// Starts setup mode. Does nothing when it's already running.
void provisioningStart();

// Call every loop(). Does the work the BLE callbacks hand over (they run
// on another task), and ends setup mode when it's over.
void provisioningUpdate();

// Call with each result wifiConfigUpdate() returns.
void provisioningOnTrialResult(WifiTrialResult result);

bool provisioningActive();
ProvisioningView provisioningView();
