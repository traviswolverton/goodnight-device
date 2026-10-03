#include "wifi_config.h"

#include <Arduino.h>
#include <Preferences.h>

#include "secrets.h"

// Older secrets.h files always had these; newer ones may leave them out and
// rely on provisioning instead.
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
#define WIFI_CONFIG_FROM_SECRETS 1
#else
#define WIFI_CONFIG_FROM_SECRETS 0
#endif

namespace {

// Long enough for a slow router and a WPA handshake, short enough that a
// wrong password is reported while the person is still holding the phone.
constexpr unsigned long kTrialTimeoutMs = 20UL * 1000;

// NVS: its own namespace, apart from "brokercfg" and settings_store.cpp's
// (15-char limit on names).
constexpr char kNamespace[] = "wificfg";

WifiCredentials active_;
WifiCredentials previous_;  // what to go back to if a trial fails
bool trial_ = false;
unsigned long trialStartedMs_ = 0;

void copyField(char* dest, size_t size, const char* src) {
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
}

bool loadSaved(WifiCredentials& out) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return false;  // nothing saved yet -- see settings_store.cpp
  }
  bool ok = prefs.getString("ssid", out.ssid, sizeof(out.ssid)) > 1;
  prefs.getString("pass", out.password, sizeof(out.password));
  prefs.end();
  return ok && out.ssid[0] != '\0';
}

void save(const WifiCredentials& credentials) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    Serial.println("[wifi] couldn't open NVS; credentials not saved");
    return;
  }
  prefs.putString("ssid", credentials.ssid);
  prefs.putString("pass", credentials.password);
  prefs.end();
}

}  // namespace

void wifiConfigBegin() {
  memset(&active_, 0, sizeof(active_));
  if (loadSaved(active_)) {
    Serial.printf("[wifi] network \"%s\" (saved)\n", active_.ssid);
    return;
  }
  memset(&active_, 0, sizeof(active_));  // loadSaved may have half-filled it
#if WIFI_CONFIG_FROM_SECRETS
  copyField(active_.ssid, sizeof(active_.ssid), WIFI_SSID);
  copyField(active_.password, sizeof(active_.password), WIFI_PASSWORD);
  Serial.printf("[wifi] network \"%s\" (from secrets.h)\n", active_.ssid);
#else
  Serial.println("[wifi] no network configured");
#endif
}

bool wifiConfigHasCredentials() {
  return active_.ssid[0] != '\0';
}

const WifiCredentials& wifiCredentials() {
  return active_;
}

bool wifiConfigBeginTrial(const char* ssid, const char* password) {
  if (ssid == nullptr || password == nullptr || ssid[0] == '\0' ||
      strlen(ssid) >= sizeof(active_.ssid) || strlen(password) >= sizeof(active_.password)) {
    return false;
  }
  // A second trial before the first finished keeps the original fallback,
  // not the first trial's unproven credentials.
  if (!trial_) {
    previous_ = active_;
  }
  copyField(active_.ssid, sizeof(active_.ssid), ssid);
  copyField(active_.password, sizeof(active_.password), password);
  trial_ = true;
  trialStartedMs_ = millis();
  Serial.printf("[wifi] trying \"%s\"\n", active_.ssid);  // never the password
  return true;
}

WifiTrialResult wifiConfigUpdate(bool connected) {
  if (!trial_) {
    return WifiTrialResult::None;
  }
  if (connected) {
    save(active_);
    trial_ = false;
    Serial.printf("[wifi] \"%s\" works; saved\n", active_.ssid);
    return WifiTrialResult::Succeeded;
  }
  if (millis() - trialStartedMs_ >= kTrialTimeoutMs) {
    active_ = previous_;
    trial_ = false;
    Serial.println("[wifi] trial failed; previous network restored");
    return WifiTrialResult::Failed;
  }
  return WifiTrialResult::None;
}

bool wifiConfigTrialActive() {
  return trial_;
}

void wifiConfigClearSaved() {
  Preferences prefs;
  if (prefs.begin(kNamespace, /*readOnly=*/false)) {
    prefs.clear();
    prefs.end();
  }
  trial_ = false;
  wifiConfigBegin();
}
