#include "diagnostics.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>

#include "display.h"
#if __has_include("version_generated.h")
#include "version_generated.h"
#endif
#include "input.h"
#include "provisioning.h"
#include "sensors.h"

namespace {

constexpr unsigned long kNormalIntervalMs = 30UL * 1000;
constexpr unsigned long kFastIntervalMs = 1000;
constexpr long kMaxFastSeconds = 600;

bool fast_ = false;
unsigned long fastUntilMs_ = 0;
unsigned long lastPublishMs_ = 0;
bool dueNow_ = true;

void expireFast() {
  if (fast_ && (long)(millis() - fastUntilMs_) >= 0) {
    fast_ = false;
  }
}

}  // namespace

void diagnosticsApplyRequest(const uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    Serial.println("[diag] unparseable request, ignored");
    return;
  }
  long seconds = doc["fast_seconds"] | -1L;
  if (seconds < 0) {
    return;
  }
  if (seconds > kMaxFastSeconds) {
    seconds = kMaxFastSeconds;
  }
  fast_ = seconds > 0;
  fastUntilMs_ = millis() + seconds * 1000UL;
  dueNow_ = true;
  Serial.printf("[diag] fast publishing for %lds\n", seconds);
}

bool diagnosticsFast() {
  expireFast();
  return fast_;
}

bool diagnosticsDue() {
  expireFast();
  unsigned long now = millis();
  unsigned long interval = fast_ ? kFastIntervalMs : kNormalIntervalMs;
  if (!dueNow_ && now - lastPublishMs_ < interval) {
    return false;
  }
  dueNow_ = false;
  lastPublishMs_ = now;
  return true;
}

void diagnosticsPublishSoon() {
  dueNow_ = true;
}

size_t diagnosticsBuild(char* out, size_t size) {
  JsonDocument doc;
#ifdef FIRMWARE_VERSION
  doc["firmware"] = FIRMWARE_VERSION;
#else
  doc["firmware"] = "unknown";
#endif
  doc["uptime_s"] = millis() / 1000;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["setup_mode"] = provisioningActive();

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["rssi"] = WiFi.RSSI();
  wifi["ip"] = WiFi.localIP().toString();

  // What answered at boot (or since): a missing one is a wiring problem.
  JsonObject hw = doc["hardware"].to<JsonObject>();
  hw["oled"] = displayPresent();
  hw["light"] = sensorsLightPresent();
  hw["expander"] = inputExpanderPresent();
  hw["joystick"] = inputJoystickPresent();
  hw["climate"] = !isnan(sensorsTemperatureC());

  JsonObject stick = doc["stick"].to<JsonObject>();
  int x = 0;
  int y = 0;
  if (inputJoystickRaw(x, y)) {
    stick["x"] = x;
    stick["y"] = y;
  } else {
    stick["x"] = nullptr;
    stick["y"] = nullptr;
  }
  stick["direction"] = inputJoystickDirection();
  stick["pressed"] = inputJoystickPressed();

  // Held right now (debounced). Presses themselves go out as events on
  // bedside/button, so the page can flash on those and show this as held.
  JsonArray down = doc["buttons_down"].to<JsonArray>();
  uint8_t mask = inputButtonsDown();
  for (uint8_t i = 0; i < 8; i++) {
    if (mask & (1u << i)) {
      down.add(i + 1);
    }
  }

  size_t n = serializeJson(doc, out, size);
  return n < size ? n : size - 1;
}
