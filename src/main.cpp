// Desk Dashboard — ESP32 client
//
// First working slice: WiFi + MQTT connectivity only, no display/LED/PIR
// code yet (those come next — see DESIGN.md's "Firmware architecture").
// Subscribes to both topics from the locked schema and dumps whatever
// arrives to Serial. Reconnect logic is deliberately not an afterthought
// here, per DESIGN.md's "MQTT reconnect logic is a real piece of work" —
// both WiFi and MQTT reconnect attempts are millis()-gated and retried
// from loop(), never blocking it with delay().

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"

namespace {

constexpr char kTopicStatus[] = "homelab/status";
constexpr char kTopicCameraAlert[] = "homelab/alert/camera";

// PubSubClient's default 256-byte buffer is too small for homelab/status
// once a couple of adapters are enabled (see DESIGN.md). Sized with
// headroom for adapter count growing past the initial 2-3.
constexpr size_t kMqttBufferSize = 2048;

constexpr unsigned long kWifiRetryIntervalMs = 5000;
constexpr unsigned long kMqttRetryIntervalMs = 5000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;

void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  Serial.printf("[mqtt] %s (%u bytes): ", topic, length);
  for (unsigned int i = 0; i < length; i++) {
    Serial.write(payload[i]);
  }
  Serial.println();
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
}

void loop() {
  ensureWifi();
  ensureMqtt();
  mqttClient.loop();
}
