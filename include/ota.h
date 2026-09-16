#pragma once

// Over-the-air firmware updates via ArduinoOTA (built into the ESP32
// Arduino core -- no extra library dependency). Once this is running,
// `pio run -t upload -e esp32dev-ota` flashes new firmware over WiFi
// instead of needing physical USB access.

// Starts the ArduinoOTA service. Call once WiFi has actually connected
// for the first time -- not unconditionally from setup(), since WiFi
// connects asynchronously and OTA's mDNS advertisement needs a real IP
// to announce. Safe to call repeatedly (e.g. every loop() while
// connected): it's a one-time no-op after the first successful call.
void otaBegin();

// Call every loop() once WiFi is connected. Non-blocking; processes at
// most one pending OTA packet per call, same shape as mqttClient.loop().
void otaHandle();
