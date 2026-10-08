// Bedside Dashboard — ESP32 client
//
// Wires together connectivity (WiFi + MQTT + OTA), input (8 buttons and a
// joystick through the MCP23017 and ADC), sensors (BH1750 light, DHT11
// climate, AM312 motion), and output (SSD1306 OLED, dimmed by room light; a piezo
// buzzer). See
// DESIGN.md for the design -- this file is just the glue.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

#include "device_config.h"
#include "diagnostics.h"
#include "dimmer.h"
#include "display.h"
#include "input.h"
#include "local_time.h"
#include "ota.h"
#include "pins.h"
#include "screen.h"
#include "secrets.h"

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
#error "secrets.h must define WIFI_SSID and WIFI_PASSWORD (scripts/gen_secrets.py writes them from OpenBao)"
#endif
#include "sensors.h"
#include "settings_store.h"
#include "sound.h"

namespace {

constexpr char kClientId[] = "bedside-dashboard-esp32";

// Subscribed.
constexpr char kTopicConfig[] = "bedside/config";
constexpr char kTopicSound[] = "bedside/sound";
constexpr char kTopicDiagnosticsRequest[] = "bedside/diagnostics/request";
// Published.
constexpr char kTopicButton[] = "bedside/button";
constexpr char kTopicStick[] = "bedside/stick";
constexpr char kTopicDiagnostics[] = "bedside/diagnostics";
constexpr char kTopicSensors[] = "bedside/sensors";
constexpr char kTopicAvailability[] = "bedside/availability";

constexpr size_t kMqttBufferSize = 4096;
constexpr unsigned long kWifiRetryIntervalMs = 5000;
constexpr unsigned long kMqttRetryIntervalMs = 5000;
constexpr unsigned long kSensorPublishMs = 30000;

// Button 8 belongs to the device. It's never published, so nothing on the
// server can be bound to it, and holding it for 10 s turns the on-screen
// echo of button and stick presses on or off (off at every boot).
constexpr uint8_t kLocalButton = 8;
bool inputEchoOn = false;

// Easter egg: this many stick clicks in a row, each within kEggGapMs of
// the last, play the Mario theme. Any other input in between starts over.
constexpr uint8_t kEggClicks = 5;
constexpr unsigned long kEggGapMs = 2000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
// The last ensureMqtt() connect attempt's outcome, handed to
// deviceConfigUpdate() (which refetches / rolls back on failures).
bool mqttAttempted = false;
bool mqttSucceeded = false;
unsigned long lastSensorPublishMs = 0;
bool sensorPublishDue = true;
DisplayLevel lastPublishedLevel = DisplayLevel::Full;
bool lastPublishedMotion = false;


char hardwareNote[32] = "";

// ---- Incoming ---------------------------------------------------------------

float clampF(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void applyDimmer(JsonObjectConst d) {
  if (d.isNull()) {
    return;
  }
  DimmerSettings s = dimmerSettings();
  s.brightLux = clampF(d["bright_lux"] | s.brightLux, 0.0f, 65000.0f);
  s.darkLux = clampF(d["dark_lux"] | s.darkLux, 0.0f, s.brightLux);
  s.dimContrast = (uint8_t)clampF(d["dim_contrast"] | (float)s.dimContrast, 0.0f, 255.0f);
  s.wakeSeconds = (uint16_t)clampF(d["wake_seconds"] | (float)s.wakeSeconds, 3.0f, 600.0f);
  dimmerApply(s);
  settingsSaveDimmer(s);
  Serial.printf("[config] bright=%.1flx dark=%.1flx dimContrast=%u wake=%us\n", s.brightLux,
                s.darkLux, s.dimContrast, s.wakeSeconds);
}

void applySounds(JsonObjectConst d) {
  if (d.isNull()) {
    return;
  }
  SoundSettings s = soundSettings();
  s.enabled = d["enabled"] | s.enabled;
  s.volume = (uint8_t)clampF(d["volume"] | (float)s.volume, 0.0f, 100.0f);
  s.darkVolume = (uint8_t)clampF(d["dark_volume"] | (float)s.darkVolume, 0.0f, 100.0f);
  soundApply(s);
  settingsSaveSound(s);
  Serial.printf("[config] sounds %s volume=%u dark=%u\n", s.enabled ? "on" : "off", s.volume,
                s.darkVolume);
}

// bedside/config, retained, from the server:
//   {"dimmer": {"bright_lux": 20, "dark_lux": 2, "dim_contrast": 1,
//               "wake_seconds": 15},
//    "sounds": {"enabled": true, "volume": 80, "dark_volume": 10}}
// Any field left out keeps its current value.
void applyConfig(const uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    Serial.println("[config] unparseable, ignored");
    return;
  }
  applyDimmer(doc["dimmer"]);
  applySounds(doc["sounds"]);
}

// bedside/sound, from the server: {"sound": "armed", "urgent": false}.
// Not retained -- a beep that arrives late is worse than none.
void playSoundMessage(const uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  Sound sound;
  if (deserializeJson(doc, payload, length) || !soundFromName(doc["sound"], sound)) {
    Serial.println("[sound] unknown or unparseable, ignored");
    return;
  }
  soundPlay(sound, doc["urgent"] | false);
}

void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  // Kept fast: parse and return. Drawing happens from loop().
  if (strcmp(topic, kTopicConfig) == 0) {
    applyConfig(payload, length);
  } else if (strcmp(topic, kTopicSound) == 0) {
    playSoundMessage(payload, length);
  } else if (strcmp(topic, kTopicDiagnosticsRequest) == 0) {
    diagnosticsApplyRequest(payload, length);
  } else {
    Serial.printf("[mqtt] unexpected topic: %s\n", topic);
  }
}

// ---- Connectivity -------------------------------------------------------------

bool wifiWasConnected = false;

// Non-blocking: one attempt per retry window; WiFi.begin() returns at once
// and the handshake finishes in the background.
void ensureWifi() {
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  if (isConnected != wifiWasConnected) {
    wifiWasConnected = isConnected;
    if (isConnected) {
      Serial.print("[wifi] connected, IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[wifi] disconnected");
    }
  }
  if (isConnected) {
    return;
  }
  unsigned long now = millis();
  if (now - lastWifiAttemptMs < kWifiRetryIntervalMs) {
    return;
  }
  lastWifiAttemptMs = now;
  Serial.println("[wifi] (re)connecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// PubSubClient's connect() blocks for the TCP handshake, so it's
// rate-limited rather than tried every loop.
void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) {
    return;
  }
  unsigned long now = millis();
  if (now - lastMqttAttemptMs < kMqttRetryIntervalMs) {
    return;
  }
  lastMqttAttemptMs = now;

  Serial.println("[mqtt] (re)connecting...");
  // Last will: the broker marks the device offline (retained) if the
  // connection drops, which is what Home Assistant's availability reads.
  // Broker and login come from the server when it's set up to hand them
  // out (device_config.h), else from secrets.h.
  const BrokerConfig& broker = deviceBroker();
  mqttAttempted = true;
  mqttSucceeded = mqttClient.connect(kClientId, broker.username[0] ? broker.username : nullptr,
                                     broker.username[0] ? broker.password : nullptr,
                                     kTopicAvailability, 0, true, "offline");
  if (mqttSucceeded) {
    Serial.println("[mqtt] connected");
    mqttClient.publish(kTopicAvailability, "online", true);
    mqttClient.subscribe(kTopicConfig);
    mqttClient.subscribe(kTopicSound);
    mqttClient.subscribe(kTopicDiagnosticsRequest);
    diagnosticsPublishSoon();
    sensorPublishDue = true;
  } else {
    Serial.printf("[mqtt] connect failed, rc=%d\n", mqttClient.state());
  }
}

// ---- Outgoing -----------------------------------------------------------------

void setOrNull(JsonDocument& doc, const char* key, float value, int decimals) {
  if (isnan(value)) {
    doc[key] = nullptr;
  } else {
    float scale = powf(10.0f, decimals);
    doc[key] = roundf(value * scale) / scale;
  }
}

// bedside/sensors, retained: the latest readings plus what the screen is
// doing, and whether the PIR sees motion. Published every kSensorPublishMs,
// on (re)connect, and whenever the screen level or the motion state changes.
void publishSensors() {
  JsonDocument doc;
  setOrNull(doc, "temperature_c", sensorsTemperatureC(), 1);
  setOrNull(doc, "humidity", sensorsHumidity(), 0);
  setOrNull(doc, "lux", sensorsLux(), 1);
  doc["screen"] = dimmerLevelName();
  doc["motion"] = sensorsMotion();
  char buffer[160];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));
  mqttClient.publish(kTopicSensors, reinterpret_cast<const uint8_t*>(buffer), length, true);
}

void maybePublishSensors() {
  if (!mqttClient.connected()) {
    return;
  }
  unsigned long now = millis();
  DisplayLevel level = dimmerLevel();
  bool motion = sensorsMotion();
  unsigned long interval = diagnosticsFast() ? 1000 : kSensorPublishMs;
  if (sensorPublishDue || level != lastPublishedLevel || motion != lastPublishedMotion ||
      now - lastSensorPublishMs >= interval) {
    publishSensors();
    sensorPublishDue = false;
    lastPublishedLevel = level;
    lastPublishedMotion = motion;
    lastSensorPublishMs = now;
  }
}

// bedside/button: {"button": 3, "press": "short"|"long"}. Not retained --
// a press is an event, not a state; replaying it on reconnect would
// trigger whatever it's bound to a second time.
void publishButton(uint8_t button, bool longPress) {
  if (!mqttClient.connected()) {
    return;  // dropped, not queued, for the same reason
  }
  char buffer[48];
  int length = snprintf(buffer, sizeof(buffer), "{\"button\":%u,\"press\":\"%s\"}", button,
                        longPress ? "long" : "short");
  mqttClient.publish(kTopicButton, reinterpret_cast<const uint8_t*>(buffer), length, false);
}

// bedside/diagnostics, retained. See diagnostics.h.
void maybePublishDiagnostics() {
  if (!mqttClient.connected() || !diagnosticsDue()) {
    return;
  }
  char buffer[640];
  size_t length = diagnosticsBuild(buffer, sizeof(buffer));
  static bool logged = false;
  if (!logged) {
    logged = true;
    Serial.printf("[diag] %s\n", buffer);  // once, so the shape can be checked
  }
  mqttClient.publish(kTopicDiagnostics, reinterpret_cast<const uint8_t*>(buffer), length, true);
}

// bedside/stick: {"stick": "up"|"down"|"left"|"right"|"press"}. An event,
// like bedside/button: not retained, dropped while disconnected.
void publishStick(const char* name) {
  if (!mqttClient.connected()) {
    return;
  }
  char buffer[32];
  int length = snprintf(buffer, sizeof(buffer), "{\"stick\":\"%s\"}", name);
  mqttClient.publish(kTopicStick, reinterpret_cast<const uint8_t*>(buffer), length, false);
}

void noteEasterEgg(const InputEvent& event) {
  static uint8_t clicks = 0;
  static unsigned long lastClickMs = 0;
  if (event.kind != InputKind::JoyPress && event.kind != InputKind::JoyWakeClick) {
    clicks = 0;
    return;
  }
  unsigned long now = millis();
  clicks = (clicks > 0 && now - lastClickMs <= kEggGapMs) ? clicks + 1 : 1;
  lastClickMs = now;
  if (clicks >= kEggClicks) {
    clicks = 0;
    soundPlay(Sound::Mario, false);
  }
}

void handleInput() {
  InputEvent event;
  while (inputNextEvent(event)) {
    if (event.kind == InputKind::JoyWakeClick) {
      noteEasterEgg(event);  // only the easter egg looks at a wake click
      continue;
    }
    if (event.kind == InputKind::ButtonVeryLong) {
      if (event.button == kLocalButton) {
        inputEchoOn = !inputEchoOn;
        screenSetInputEcho(inputEchoOn);
        screenFlash(inputEchoOn ? "KEYS ON" : "KEYS OFF");
        soundPlay(inputEchoOn ? Sound::Armed : Sound::Cancel, false);
        Serial.printf("[input] echo %s\n", inputEchoOn ? "on" : "off");
      }
      continue;  // a 10 s hold is never published
    }
    if (event.button == kLocalButton &&
        (event.kind == InputKind::ButtonShort || event.kind == InputKind::ButtonLong)) {
      noteEasterEgg(event);  // reserved: no tick, no echo, nothing published
      continue;
    }
    screenNoteInput(event);
    noteEasterEgg(event);
    switch (event.kind) {
      case InputKind::ButtonShort:
      case InputKind::ButtonLong:
        Serial.printf("[input] button %u %s\n", event.button,
                      event.kind == InputKind::ButtonLong ? "long" : "short");
        if (event.kind == InputKind::ButtonLong) {
          // Fires the moment a hold counts as long, so you know you can
          // let go. The only sound played without the server asking.
          soundPlay(Sound::Tick, false);
        }
        publishButton(event.button, event.kind == InputKind::ButtonLong);
        break;
      default: {
        // The joystick drives on-device navigation; nothing to navigate
        // yet on the bench screen, so it's only shown (and logged).
        const char* name = event.kind == InputKind::JoyUp      ? "up"
                           : event.kind == InputKind::JoyDown  ? "down"
                           : event.kind == InputKind::JoyLeft  ? "left"
                           : event.kind == InputKind::JoyRight ? "right"
                                                               : "press";
        Serial.printf("[input] stick %s\n", name);
        publishStick(name);
        break;
      }
    }
  }
}

void scanI2c() {
  Serial.print("[i2c] found:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
    }
  }
  Serial.println();
}

// Asks the server which broker to use when that's due (device_config.h)
// and switches to it. Before ensureMqtt(), so the first connect after boot
// already uses the server's answer.
void ensureBrokerConfig() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  bool changed = deviceConfigUpdate(mqttAttempted, mqttSucceeded);
  mqttAttempted = false;
  if (changed) {
    const BrokerConfig& broker = deviceBroker();
    Serial.printf("[mqtt] switching to %s:%u\n", broker.host, broker.port);
    mqttClient.disconnect();
    mqttClient.setServer(broker.host, broker.port);
    lastMqttAttemptMs = millis() - kMqttRetryIntervalMs;  // connect right away
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("\n[boot] Bedside Dashboard ESP32 client");

  Wire.begin(kPinI2cSda, kPinI2cScl, kI2cClockHz);
  scanI2c();

  WiFi.mode(WIFI_STA);
  localTimeBegin();
  deviceConfigBegin();
  mqttClient.setServer(deviceBroker().host, deviceBroker().port);
  mqttClient.setBufferSize(kMqttBufferSize);
  mqttClient.setCallback(onMqttMessage);

  displayBegin();
  sensorsBegin();
  dimmerBegin(settingsLoadDimmer(DimmerSettings{}));
  soundBegin(settingsLoadSound(SoundSettings{}));
  inputBegin(dimmerWake);

  // One line naming whatever's missing, so a loose wire shows on the
  // screen itself during bring-up rather than only over serial.
  const char* missing[3];
  int n = 0;
  if (!sensorsLightPresent()) missing[n++] = "BH1750";
  if (!inputExpanderPresent()) missing[n++] = "MCP";
  if (!inputJoystickPresent()) missing[n++] = "stick";
  if (n > 0) {
    int used = snprintf(hardwareNote, sizeof(hardwareNote), "No");
    for (int i = 0; i < n; i++) {
      used += snprintf(hardwareNote + used, sizeof(hardwareNote) - used, " %s", missing[i]);
    }
  }
}

void loop() {
  ensureWifi();
  ensureBrokerConfig();
  ensureMqtt();
  mqttClient.loop();

  // OTA is gated on WiFi only, so a broken broker never blocks the one way
  // to fix broken firmware.
  if (WiFi.status() == WL_CONNECTED) {
    otaBegin();  // no-op after the first successful call
    otaHandle();
  }

  sensorsUpdate();
  inputUpdate();
  handleInput();
  dimmerUpdate(sensorsLux());
  soundUpdate();
  maybePublishSensors();
  maybePublishDiagnostics();
  displayRefresh();

  uint8_t hour12 = 0;
  uint8_t minute = 0;
  bool pm = false;
  bool timeValid = localTimeNow12h(hour12, minute, pm);
  ScreenModel model{
      sensorsTemperatureC(),
      sensorsHumidity(),
      sensorsLux(),
      WiFi.status() == WL_CONNECTED,
      mqttClient.connected(),
      hardwareNote[0] ? hardwareNote : nullptr,
      timeValid,
      hour12,
      minute,
      pm,
  };
  screenUpdate(model);
}
