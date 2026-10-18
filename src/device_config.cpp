#include "device_config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "http_get.h"
#include "secrets.h"

// Older secrets.h files predate this module; they still build (fetching
// disabled) as long as they have MQTT_*.
#if defined(SERVER_URL) && defined(DEVICE_TOKEN)
#define DEVICE_CONFIG_FETCH 1
#else
#define DEVICE_CONFIG_FETCH 0
#endif

namespace {

// A plain request to a LAN server normally answers in milliseconds; this
// only bounds how long the main loop can stall when the server is down.
constexpr unsigned long kFetchTimeoutMs = 2000;
constexpr unsigned long kRefreshIntervalMs = 15UL * 60 * 1000;
// Refetch early after this many failed connects in a row (the broker may
// have moved), but no more often than kMinFetchGapMs.
constexpr uint8_t kFailuresBeforeFetch = 3;
constexpr unsigned long kMinFetchGapMs = 60UL * 1000;
// A fetched change that can't connect this many times is rolled back.
constexpr uint8_t kTrialMaxFailures = 5;
// ...and that same answer is then ignored for this long.
constexpr unsigned long kRejectRetryMs = 60UL * 60 * 1000;

// NVS: separate from settings_store.cpp's "dashcfg" (15-char limit on names).
constexpr char kNamespace[] = "brokercfg";

BrokerConfig active_;
BrokerConfig previous_;  // what to go back to if a trial fails
BrokerConfig rejected_;
bool hasRejected_ = false;
unsigned long rejectedAtMs_ = 0;
bool trial_ = false;
uint8_t trialFailures_ = 0;
uint8_t consecutiveFailures_ = 0;
bool fetchedOnce_ = false;
unsigned long lastFetchMs_ = 0;

void copyField(char* dest, size_t size, const char* src) {
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
}

bool sameConfig(const BrokerConfig& a, const BrokerConfig& b) {
  return a.port == b.port && strcmp(a.host, b.host) == 0 && strcmp(a.username, b.username) == 0 &&
         strcmp(a.password, b.password) == 0;
}

void setFromSecrets(BrokerConfig& config) {
  copyField(config.host, sizeof(config.host), MQTT_HOST);
  config.port = MQTT_PORT;
  copyField(config.username, sizeof(config.username), MQTT_USERNAME);
  copyField(config.password, sizeof(config.password), MQTT_PASSWORD);
}

bool loadSaved(BrokerConfig& config) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return false;  // nothing saved yet -- see settings_store.cpp
  }
  bool ok = prefs.getString("host", config.host, sizeof(config.host)) > 1;
  config.port = prefs.getUShort("port", 1883);
  prefs.getString("user", config.username, sizeof(config.username));
  prefs.getString("pass", config.password, sizeof(config.password));
  prefs.end();
  return ok && config.host[0] != '\0';
}

void save(const BrokerConfig& config) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putString("host", config.host);
  prefs.putUShort("port", config.port);
  prefs.putString("user", config.username);
  prefs.putString("pass", config.password);
  prefs.end();
}

#if DEVICE_CONFIG_FETCH
// GET <SERVER_URL>/device/config. Never logs the password.
bool fetch(BrokerConfig& out) {
  char url[128];
  snprintf(url, sizeof(url), "%s/device/config", SERVER_URL);
  uint8_t body[512];
  size_t length = 0;
  if (!httpGetToBuffer(url, DEVICE_TOKEN, body, sizeof(body), millis() + kFetchTimeoutMs, &length)) {
    Serial.println("[config] couldn't get broker settings from the server");
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body, length)) {
    Serial.println("[config] server's answer isn't valid JSON");
    return false;
  }
  const char* host = doc["mqtt_host"] | "";
  uint32_t port = doc["mqtt_port"] | 0;
  const char* username = doc["mqtt_username"] | "";
  const char* password = doc["mqtt_password"] | "";
  if (host[0] == '\0' || port == 0 || port > 65535 || strlen(host) >= sizeof(out.host) ||
      strlen(username) >= sizeof(out.username) || strlen(password) >= sizeof(out.password)) {
    Serial.println("[config] server's broker settings are missing or too long; ignoring them");
    return false;
  }
  copyField(out.host, sizeof(out.host), host);
  out.port = (uint16_t)port;
  copyField(out.username, sizeof(out.username), username);
  copyField(out.password, sizeof(out.password), password);
  return true;
}
#endif

}  // namespace

void deviceConfigBegin() {
  if (loadSaved(active_)) {
    Serial.printf("[config] broker %s:%u (saved from the server)\n", active_.host, active_.port);
  } else {
    setFromSecrets(active_);
    Serial.printf("[config] broker %s:%u (from secrets.h)\n", active_.host, active_.port);
  }
}

const BrokerConfig& deviceBroker() { return active_; }

bool deviceConfigUpdate(bool connectAttempted, bool connectSucceeded) {
  unsigned long now = millis();

  if (connectAttempted) {
    if (connectSucceeded) {
      consecutiveFailures_ = 0;
      if (trial_) {
        trial_ = false;
        save(active_);
        Serial.println("[config] new broker settings work; saved");
      }
    } else {
      consecutiveFailures_++;
      if (trial_ && ++trialFailures_ >= kTrialMaxFailures) {
        Serial.printf("[config] can't connect with the server's new settings (%s:%u); going back\n",
                      active_.host, active_.port);
        rejected_ = active_;
        hasRejected_ = true;
        rejectedAtMs_ = now;
        active_ = previous_;
        trial_ = false;
        consecutiveFailures_ = 0;
        return true;
      }
    }
  }

#if DEVICE_CONFIG_FETCH
  if (trial_) {
    return false;  // let the trial finish before asking again
  }
  bool due = !fetchedOnce_ || now - lastFetchMs_ >= kRefreshIntervalMs ||
             (consecutiveFailures_ >= kFailuresBeforeFetch && now - lastFetchMs_ >= kMinFetchGapMs);
  if (!due) {
    return false;
  }
  fetchedOnce_ = true;
  lastFetchMs_ = now;

  BrokerConfig fetched;
  if (!fetch(fetched)) {
    return false;
  }
  if (sameConfig(fetched, active_)) {
    Serial.println("[config] server's broker settings match the ones in use");
    return false;
  }
  if (hasRejected_ && sameConfig(fetched, rejected_) && now - rejectedAtMs_ < kRejectRetryMs) {
    return false;
  }
  Serial.printf("[config] server says: broker %s:%u; trying it\n", fetched.host, fetched.port);
  previous_ = active_;
  active_ = fetched;
  trial_ = true;
  trialFailures_ = 0;
  consecutiveFailures_ = 0;
  return true;
#else
  return false;
#endif
}
