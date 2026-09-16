#include "ota.h"

#include <ArduinoOTA.h>

#include "secrets.h"

namespace {
bool started_ = false;
}

void otaBegin() {
  if (started_) {
    return;  // ArduinoOTAClass::begin() itself also guards this, but
             // skip the hostname/callback setup redundantly too.
  }

  ArduinoOTA.setHostname("desk-dashboard-esp32");
  // Without a password, anything on the LAN could push firmware to this
  // board -- OTA_PASSWORD lives in secrets.h same as the WiFi/MQTT
  // credentials, never committed.
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() { Serial.println("[ota] update starting"); });
  ArduinoOTA.onEnd([]() { Serial.println("[ota] update complete, rebooting"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[ota] %u%%\n", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("[ota] error %d\n", error); });

  ArduinoOTA.begin();
  started_ = true;
  Serial.println("[ota] ready");
}

void otaHandle() {
  if (started_) {
    ArduinoOTA.handle();
  }
}
