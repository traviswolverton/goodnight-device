#pragma once

#include <stdint.h>

// Which MQTT broker (and login) to use, fetched from the server's
// GET /device/config instead of being fixed at compile time -- so the
// broker can be changed from the server's web UI (Connections -> Bedside
// device) without a reflash. See DESIGN.md's "Broker settings from the
// server" section.
//
// Needs SERVER_URL and DEVICE_TOKEN in secrets.h; without them this is
// all a no-op and the MQTT_* values in secrets.h are used, as before.
//
// Where the settings in use come from, in order:
//   1. ones fetched from the server, kept in NVS once they've WORKED (an
//      MQTT connection succeeded with them), so a server outage or reboot
//      never loses them
//   2. otherwise the MQTT_* values compiled in from secrets.h
// A fetched answer that differs from what's in use is only a trial until
// it connects: after kTrialMaxFailures failed connects the previous
// settings come back, and that answer is ignored for a while
// (kRejectRetryMs) so a bad value on the server can't strand the display.

// Sizes match the server's limits (app/api/connections.py DISPLAY_LIMITS).
struct BrokerConfig {
  char host[64];
  uint16_t port;
  char username[48];
  char password[96];
};

// Loads the settings to start with (NVS, else secrets.h). Call once from
// setup(), before the first deviceBroker().
void deviceConfigBegin();

// The settings in use now. Stable storage: safe to hand host to
// PubSubClient::setServer(), which keeps the pointer.
const BrokerConfig& deviceBroker();

// Call from loop() while WiFi is up, with the result of every MQTT connect
// attempt since the last call (connectAttempted false = none). Fetches
// from the server when it's due (first time, every
// kRefreshIntervalMs, and after repeated connect failures) -- a blocking
// HTTP request of up to kFetchTimeoutMs. Returns true when the settings in
// use changed, i.e. the caller must reconnect with deviceBroker().
bool deviceConfigUpdate(bool connectAttempted, bool connectSucceeded);
