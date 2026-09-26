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
#include "ota.h"

namespace {

constexpr char kTopicStatus[] = "homelab/status";
constexpr char kTopicCameraAlert[] = "homelab/alert/camera";
// Actions (DESIGN.md "Actions"): the one topic this device publishes to,
// and the server's replies.
constexpr char kTopicAction[] = "homelab/action";
constexpr char kTopicActionResult[] = "homelab/action/result";

// PubSubClient's default 256-byte buffer is too small for homelab/status
// once a couple of adapters are enabled (see DESIGN.md). Sized with
// headroom for adapter count growing past the initial 2-3. Doubled
// 2048 -> 4096 with Actions: up to 12 menu entries add ~1KB to every
// status message, and a message over the buffer is dropped whole.
constexpr size_t kMqttBufferSize = 4096;

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

// TEMPORARY diagnostic for investigating a suspected heap fragmentation
// issue (repeated String/JsonDocument allocation on every message) --
// see DESIGN.md. getMaxAllocHeap() is the fragmentation-sensitive
// number (largest contiguous allocatable block); getFreeHeap() is total
// free regardless of contiguity. If the former trends down over time
// while the latter stays roughly flat, that's fragmentation, not a
// leak. Remove once this is resolved one way or the other.
unsigned long lastHeapLogMs = 0;
constexpr unsigned long kHeapLogIntervalMs = 30000;

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
  // strcmp, not a String, for the same heap-churn reason as
  // status_payload's fixed buffers -- this runs on every single message,
  // forever.
  if (strcmp(topic, kTopicStatus) == 0) {
    if (parseStatusPayload(payload, length, gLatestStatus)) {
      gHasNewStatus = true;
    }
  } else if (strcmp(topic, kTopicActionResult) == 0) {
    ActionResult result;
    if (parseActionResult(payload, length, result)) {
      screenOnActionResult(result);  // records it; drawing happens in loop()
    }
  } else if (strcmp(topic, kTopicCameraAlert) == 0) {
    CameraAlert alert;
    if (parseCameraAlert(payload, length, alert)) {
      screenOnCameraAlert(alert);
    }
  } else {
    Serial.printf("[mqtt] unexpected topic: %s\n", topic);
  }
}

bool wifiWasConnected = false;

// Non-blocking: kicks off a connection attempt at most once per retry
// window, never spins waiting for it. WiFi.begin() itself returns
// immediately; the actual handshake happens in the background and
// WiFi.status() is polled from loop() to notice when it completes.
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
    mqttClient.subscribe(kTopicActionResult);
  } else {
    Serial.printf("[mqtt] connect failed, rc=%d\n", mqttClient.state());
  }
}

// Handed to screen.cpp, which decides WHEN to send (a press on the
// Actions menu) but has no MQTT client of its own. False = not sent
// (offline, or a malformed id), so the menu can say so straight away
// instead of waiting out a reply that will never come.
bool sendActionRequest(const char* actionId, uint16_t req) {
  if (!mqttClient.connected()) {
    return false;
  }
  char buffer[64];
  size_t length = buildActionRequest(buffer, sizeof(buffer), actionId, req);
  if (length == 0) {
    return false;
  }
  Serial.printf("[action] sending %s (req %u)\n", actionId, (unsigned)req);
  return mqttClient.publish(kTopicAction, reinterpret_cast<const uint8_t*>(buffer), length, false);
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
  screenSetActionSender(sendActionRequest);
}

void loop() {
  ensureWifi();
  ensureMqtt();
  mqttClient.loop();

  unsigned long nowForHeapLog = millis();
  if (nowForHeapLog - lastHeapLogMs >= kHeapLogIntervalMs) {
    lastHeapLogMs = nowForHeapLog;
    Serial.printf("[heap] free=%u minFree=%u maxAlloc=%u uptimeMs=%lu\n", ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), nowForHeapLog);
  }

  // OTA doesn't depend on MQTT at all -- gated on WiFi only, so a broken
  // broker/server never blocks the one way to fix a broken firmware.
  if (WiFi.status() == WL_CONNECTED) {
    otaBegin();  // one-time no-op after the first successful call
    otaHandle();
  }

  int32_t encoderDelta = encoderReadDelta();
  bool encoderWasPressed = encoderPressed();
  if (encoderDelta != 0 || encoderWasPressed) {
    // Any real encoder input counts as activity for the backlight,
    // same as PIR motion -- whichever happened more recently keeps it
    // lit. Unconditional here regardless of what screen.cpp does with
    // the input; harmless to call twice in one loop (idempotent).
    backlightNotifyActivity();
  }
  if (encoderDelta != 0) {
    screenOnEncoderDelta(encoderDelta);
  }
  if (encoderWasPressed) {
    screenOnEncoderPress();
  }

  backlightUpdate();

  if (gHasNewStatus) {
    ledsUpdate(gLatestStatus);
    // One-line confirmation that a message actually made it through
    // parsing and out to the LEDs -- without this, a working, silent
    // steady state is indistinguishable over Serial from a stuck one.
    Serial.printf("[status] %u services, level=%s\n", gLatestStatus.serviceCount,
                  gLatestStatus.overallLevel);
  }
  screenUpdate(gLatestStatus, gHasNewStatus);
  gHasNewStatus = false;

  // Called after screenUpdate() so a freshly-entered CAMERA screen gets
  // its "Loading snapshot..." chrome drawn and actually shown before
  // this potentially blocks for up to ~5s fetching/decoding the image --
  // see screen.h and DESIGN.md's "Camera snapshot fetch" note for why
  // this can't run from the MQTT callback instead. A no-op most loop()
  // iterations (nothing pending).
  screenCameraFetchIfPending();
}
