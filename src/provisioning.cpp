#include "provisioning.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>

#include "dimmer.h"

namespace {

// Random, made up for this project. Nothing else needs to know them but
// the phone-side tool (nRF Connect, or a page on the server).
constexpr char kServiceUuid[] = "6e5b0001-2f5c-4c0e-9b7d-5a8f0c3e1d00";
constexpr char kSsidUuid[] = "6e5b0002-2f5c-4c0e-9b7d-5a8f0c3e1d00";      // write
constexpr char kPasswordUuid[] = "6e5b0003-2f5c-4c0e-9b7d-5a8f0c3e1d00";  // write
constexpr char kApplyUuid[] = "6e5b0004-2f5c-4c0e-9b7d-5a8f0c3e1d00";     // write any byte
constexpr char kStatusUuid[] = "6e5b0005-2f5c-4c0e-9b7d-5a8f0c3e1d00";    // read, notify

constexpr unsigned long kIdleTimeoutMs = 5UL * 60 * 1000;
// After success, long enough for the "connected" notification to reach
// the phone before the radio it rides on is shut down.
constexpr unsigned long kLingerMs = 3000;
// WiFi.disconnect() is asynchronous; a trial started while the old
// connection is still reported up would "succeed" at once.
constexpr unsigned long kDisconnectWaitMs = 1000;

enum class Phase : uint8_t { Off, Waiting, Connecting, Linger };

Phase phase_ = Phase::Off;
char name_[16] = "";
uint32_t passkey_ = 0;
unsigned long lastActivityMs_ = 0;
unsigned long lingerStartMs_ = 0;

NimBLECharacteristic* statusChar_ = nullptr;

// Written by the BLE task, read by loop(): the buffers are only looked at
// after applyRequested_ is set, which the Apply write does last.
char ssid_[33] = "";
char password_[64] = "";
volatile bool applyRequested_ = false;
volatile bool clientActivity_ = false;

// Full text for the phone, short text for the OLED.
char statusLong_[48] = "waiting";
const char* statusShort_ = "Waiting for phone";

// Why the last connection attempt failed, from the WiFi disconnect event.
volatile uint8_t lastDisconnectReason_ = 0;
bool wifiEventRegistered_ = false;

void setStatus(const char* longText, const char* shortText) {
  snprintf(statusLong_, sizeof(statusLong_), "%s", longText);
  statusShort_ = shortText;
  if (statusChar_ != nullptr) {
    statusChar_->setValue(reinterpret_cast<const uint8_t*>(statusLong_), strlen(statusLong_));
    statusChar_->notify();
  }
}

void onWifiEvent(arduino_event_id_t, arduino_event_info_t info) {
  uint8_t reason = info.wifi_sta_disconnected.reason;
  // The retry loop in main.cpp disconnects on purpose every few seconds;
  // that says nothing about why the join failed.
  if (reason != WIFI_REASON_ASSOC_LEAVE) {
    lastDisconnectReason_ = reason;
  }
}

const char* failureText(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
      return "network not found";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "wrong password";
    default:
      return "could not connect";
  }
}

// Copies a characteristic's bytes into a fixed buffer; false if they
// don't fit (the value is then left empty so Apply rejects it).
bool copyValue(NimBLECharacteristic* c, char* dest, size_t size) {
  const NimBLEAttValue value = c->getValue();
  if (value.size() >= size) {
    dest[0] = '\0';
    return false;
  }
  memcpy(dest, value.data(), value.size());
  dest[value.size()] = '\0';
  return true;
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    clientActivity_ = true;
    Serial.println("[setup] phone connected");
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    clientActivity_ = true;
    Serial.println("[setup] phone disconnected");
  }
  uint32_t onPassKeyDisplay() override { return passkey_; }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (!info.isEncrypted() || !info.isAuthenticated()) {
      // Never let an unpaired phone stay connected.
      Serial.println("[setup] pairing failed; dropping the connection");
      NimBLEDevice::getServer()->disconnect(info);
      return;
    }
    clientActivity_ = true;
    Serial.println("[setup] paired");
  }
};

class WriteCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    clientActivity_ = true;
    const std::string uuid = c->getUUID().toString();
    if (uuid == kSsidUuid) {
      copyValue(c, ssid_, sizeof(ssid_));
    } else if (uuid == kPasswordUuid) {
      copyValue(c, password_, sizeof(password_));
    } else if (uuid == kApplyUuid) {
      applyRequested_ = true;
    }
  }
};

void startBle() {
  NimBLEDevice::init(name_);
  // No bonding: nothing about this phone is kept after setup. MITM +
  // Secure Connections, with the passkey shown on the OLED, so someone
  // in range can't just write their own network over this one.
  NimBLEDevice::setSecurityAuth(/*bonding=*/false, /*mitm=*/true, /*sc=*/true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(passkey_);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  server->advertiseOnDisconnect(true);

  NimBLEService* service = server->createService(kServiceUuid);
  constexpr uint32_t kSecureWrite =
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
  static WriteCallbacks writeCallbacks;
  service->createCharacteristic(kSsidUuid, kSecureWrite)->setCallbacks(&writeCallbacks);
  service->createCharacteristic(kPasswordUuid, kSecureWrite)->setCallbacks(&writeCallbacks);
  service->createCharacteristic(kApplyUuid, kSecureWrite)->setCallbacks(&writeCallbacks);
  statusChar_ = service->createCharacteristic(
      kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC |
                       NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::NOTIFY);
  statusChar_->setValue(reinterpret_cast<const uint8_t*>(statusLong_), strlen(statusLong_));

  server->start();
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->setName(name_);
  advertising->addServiceUUID(kServiceUuid);
  advertising->enableScanResponse(true);
  advertising->start();
}

void stopBle() {
  uint32_t before = ESP.getFreeHeap();
  statusChar_ = nullptr;
  NimBLEDevice::deinit(true);
  Serial.printf("[setup] BLE off; free heap %u -> %u\n", before, ESP.getFreeHeap());
}

void beginSession() {
  passkey_ = esp_random() % 1000000;
  lastActivityMs_ = millis();
  phase_ = Phase::Waiting;
  setStatus("waiting", "Waiting for phone");
  startBle();
  Serial.printf("[setup] advertising as \"%s\", passkey %06u\n", name_, (unsigned)passkey_);
}

void endSession() {
  stopBle();
  phase_ = Phase::Off;
  applyRequested_ = false;
  memset(password_, 0, sizeof(password_));
  Serial.println("[setup] done");
}

void startTrial() {
  applyRequested_ = false;
  if (ssid_[0] == '\0') {
    setStatus("error: no network name", "No network name");
    return;
  }
  WiFi.disconnect();
  unsigned long waitStart = millis();
  while (WiFi.status() == WL_CONNECTED && millis() - waitStart < kDisconnectWaitMs) {
    delay(10);
  }
  if (!wifiConfigBeginTrial(ssid_, password_)) {
    setStatus("error: invalid name or password", "Invalid entry");
  } else {
    lastDisconnectReason_ = 0;
    WiFi.begin(wifiCredentials().ssid, wifiCredentials().password);
    phase_ = Phase::Connecting;
    setStatus("connecting", "Connecting...");
  }
  // The trial has its own copy; don't keep the password around here.
  memset(password_, 0, sizeof(password_));
  ssid_[0] = '\0';
}

}  // namespace

void provisioningStart() {
  if (phase_ != Phase::Off) {
    return;
  }
  if (name_[0] == '\0') {
    snprintf(name_, sizeof(name_), "Bedside-%04X", (unsigned)(ESP.getEfuseMac() >> 32) & 0xFFFF);
  }
  if (!wifiEventRegistered_) {
    WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    wifiEventRegistered_ = true;
  }
  Serial.printf("[setup] starting; free heap %u\n", ESP.getFreeHeap());
  beginSession();
}

void provisioningUpdate() {
  if (phase_ == Phase::Off) {
    return;
  }
  unsigned long now = millis();
  // The panel may be dark and the person is looking at their phone.
  dimmerWake();

  if (clientActivity_) {
    clientActivity_ = false;
    lastActivityMs_ = now;
  }

  switch (phase_) {
    case Phase::Waiting:
      if (applyRequested_) {
        startTrial();
      } else if (now - lastActivityMs_ >= kIdleTimeoutMs) {
        Serial.println("[setup] timed out");
        endSession();
        if (!wifiConfigHasCredentials()) {
          beginSession();  // nothing else to do without a network
        }
      }
      break;
    case Phase::Connecting:
      // The outcome arrives through provisioningOnTrialResult().
      lastActivityMs_ = now;
      break;
    case Phase::Linger:
      if (now - lingerStartMs_ >= kLingerMs) {
        endSession();
      }
      break;
    case Phase::Off:
      break;
  }
}

void provisioningOnTrialResult(WifiTrialResult result) {
  if (phase_ != Phase::Connecting) {
    return;
  }
  if (result == WifiTrialResult::Succeeded) {
    char text[48];
    snprintf(text, sizeof(text), "connected %s", WiFi.localIP().toString().c_str());
    setStatus(text, "WiFi connected");
    phase_ = Phase::Linger;
    lingerStartMs_ = millis();
  } else if (result == WifiTrialResult::Failed) {
    char text[48];
    snprintf(text, sizeof(text), "failed: %s", failureText(lastDisconnectReason_));
    const char* shortText = lastDisconnectReason_ == WIFI_REASON_NO_AP_FOUND ? "Network not found"
                            : strstr(text, "password")                       ? "Wrong password"
                                                                             : "Could not connect";
    setStatus(text, shortText);
    phase_ = Phase::Waiting;  // still advertising; the phone can try again
    lastActivityMs_ = millis();
  }
}

bool provisioningActive() {
  return phase_ != Phase::Off;
}

ProvisioningView provisioningView() {
  ProvisioningView view{};
  view.active = phase_ != Phase::Off;
  view.name = name_;
  view.passkey = passkey_;
  unsigned long idle = millis() - lastActivityMs_;
  view.secondsLeft = idle >= kIdleTimeoutMs ? 0 : (kIdleTimeoutMs - idle) / 1000;
  view.status = statusShort_;
  return view;
}
