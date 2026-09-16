// Desk Dashboard — ESP32 client
//
// Wires together connectivity (WiFi + MQTT), input (rotary encoder),
// output (display, LED strip, backlight), and presence (PIR) around the
// screen state machine in screen.cpp. See DESIGN.md for the full design
// — this file is just the glue.

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"
#include "status_payload.h"
#include "encoder.h"
#include "leds.h"
#include "backlight.h"
#include "screen.h"

namespace {

constexpr char kTopicStatus[] = "homelab/status";
constexpr char kTopicCameraAlert[] = "homelab/alert/camera";

// PubSubClient's default 256-byte buffer is too small for homelab/status
// once a couple of adapters are enabled (see DESIGN.md). Sized with
// headroom for adapter count growing past the initial 2-3.
constexpr size_t kMqttBufferSize = 2048;

constexpr unsigned long kWifiRetryIntervalMs = 5000;
constexpr unsigned long kMqttRetryIntervalMs = 5000;

// Encoder pins -- see DESIGN.md's "Pin assignments" table.
constexpr uint8_t kPinEncoderClk = 32;
constexpr uint8_t kPinEncoderDt = 33;
constexpr uint8_t kPinEncoderSw = 25;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;

// Latest parsed homelab/status, plus a one-shot flag for "did a new one
// just arrive this loop" -- screenUpdate() and ledsUpdate() both care
// about that distinction, not just the latest value, so they don't
// redraw/repaint every single loop() iteration forever after the first
// message ever arrives.
StatusPayload gLatestStatus;
bool gHasNewStatus = false;

void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  // Kept deliberately fast and non-blocking: parse, flag, return. Nothing
  // in here may do SPI/FastLED work (ledsUpdate/screen drawing) -- those
  // happen from loop() once this returns, not synchronously in here. A
  // slow callback delays the TCP ACK for the packet just received; the
  // broker's stack then retransmits it, our stack redelivers the
  // retransmission as a "new" message, and the cycle feeds itself. This
  // is not theoretical -- it's exactly what happened when ledsUpdate()
  // (i.e. FastLED.show(), which disables interrupts for its WS2812
  // timing) ran directly in here: the same message got redelivered
  // thousands of times a second, indefinitely, verified on real
  // hardware before this fix.
  String t(topic);
  if (t == kTopicStatus) {
    if (parseStatusPayload(payload, length, gLatestStatus)) {
      gHasNewStatus = true;
    }
  } else if (t == kTopicCameraAlert) {
    CameraAlert alert;
    if (parseCameraAlert(payload, length, alert)) {
      screenOnCameraAlert(alert);
    }
  } else {
    Serial.printf("[mqtt] unexpected topic: %s\n", topic);
  }
}

// Non-blocking: kicks off a connection attempt at most once per retry
// window, never spins waiting for it. WiFi.begin() itself returns
// immediately; the actual handshake happens in the background and
// WiFi.status() is polled from loop() to notice when it completes.
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
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

// PubSubClient's connect() blocks for the TCP handshake — a real
// limitation of the library, not something we can fully avoid — so this
// is rate-limited to one attempt per retry window rather than called
// every loop iteration, keeping the main loop responsive between tries.
void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();
  if (now - lastMqttAttemptMs < kMqttRetryIntervalMs) {
    return;
  }
  lastMqttAttemptMs = now;

  Serial.println("[mqtt] (re)connecting...");
  // Client ID left to PubSubClient's default random suffix isn't used
  // here — an explicit ID makes broker-side logs/ACLs easier to read.
  if (mqttClient.connect("desk-dashboard-esp32", MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("[mqtt] connected, subscribing...");
    mqttClient.subscribe(kTopicStatus);
    mqttClient.subscribe(kTopicCameraAlert);
  } else {
    Serial.printf("[mqtt] connect failed, rc=%d\n", mqttClient.state());
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("\n[boot] Desk Dashboard ESP32 client");

  WiFi.mode(WIFI_STA);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(kMqttBufferSize);
  mqttClient.setCallback(onMqttMessage);

  encoderBegin(kPinEncoderClk, kPinEncoderDt, kPinEncoderSw);
  ledsBegin();
  backlightBegin();
  screenBegin();
}

void loop() {
  ensureWifi();
  ensureMqtt();
  mqttClient.loop();

  int32_t encoderDelta = encoderReadDelta();
  if (encoderDelta != 0) {
    screenOnEncoderDelta(encoderDelta);
  }
  if (encoderPressed()) {
    screenOnEncoderPress();
  }

  backlightUpdate();

  if (gHasNewStatus) {
    ledsUpdate(gLatestStatus);
    // One-line confirmation that a message actually made it through
    // parsing and out to the LEDs -- without this, a working, silent
    // steady state is indistinguishable over Serial from a stuck one.
    Serial.printf("[status] %u services, level=%s\n", gLatestStatus.serviceCount,
                  gLatestStatus.overallLevel.c_str());
  }
  screenUpdate(gLatestStatus, gHasNewStatus);
  gHasNewStatus = false;
}
