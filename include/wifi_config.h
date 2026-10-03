#pragma once

#include <stdint.h>

// Which WiFi network (and password) to join, kept in NVS so it can be set
// from a phone (see DESIGN.md, "WiFi provisioning") instead of being fixed
// at compile time. Same trial-then-keep idea as device_config: credentials
// that were just entered are only a trial until a connection succeeds with
// them, so a typo can't replace working settings.
//
// Where the credentials in use come from, in order:
//   1. ones saved in NVS (a trial that connected)
//   2. otherwise WIFI_SSID / WIFI_PASSWORD compiled in from secrets.h, if
//      both are defined
//   3. otherwise none: wifiConfigHasCredentials() is false and the device
//      has nothing to join until it's provisioned

// Limits from 802.11 / WPA: an SSID is at most 32 bytes, a passphrase at
// most 63 (an open network has an empty one).
struct WifiCredentials {
  char ssid[33];
  char password[64];
};

enum class WifiTrialResult : uint8_t {
  None,       // no trial running, or it hasn't finished
  Succeeded,  // connected; the credentials are now saved
  Failed,     // no connection in time; the previous credentials are back
};

// Loads the credentials to start with (NVS, else secrets.h). Call once
// from setup(), before the first wifiCredentials().
void wifiConfigBegin();

// False when there's nothing to join: no NVS entry and no usable secrets.h
// values.
bool wifiConfigHasCredentials();

// The credentials in use now.
const WifiCredentials& wifiCredentials();

// Starts a trial of new credentials. They become the ones in use at once
// (so the caller's next WiFi.begin() uses them) but aren't saved yet. The
// caller should WiFi.disconnect() first: a still-live connection to the old
// network would otherwise count as the trial succeeding.
// Returns false, changing nothing, when the SSID is empty or either value
// doesn't fit.
bool wifiConfigBeginTrial(const char* ssid, const char* password);

// Call from loop() with whether WiFi is currently connected. While a trial
// is running it saves the credentials on the first connect, or rolls back
// after kTrialTimeoutMs without one. Returns the outcome exactly once, on
// the call where the trial ends; None otherwise.
WifiTrialResult wifiConfigUpdate(bool connected);

// True while a trial is running.
bool wifiConfigTrialActive();

// Forgets the saved credentials, back to the secrets.h ones (or none).
void wifiConfigClearSaved();
