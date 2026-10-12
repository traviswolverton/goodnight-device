#include "sensors.h"

#include <Arduino.h>
#include <BH1750.h>
#include <DHT.h>
#include <Wire.h>
#include <math.h>

#include "pins.h"

namespace {

BH1750 light_(kI2cAddrLight);
DHT dht_(kPinDht, DHT11);

bool lightPresent_ = false;
float lux_ = NAN;
unsigned long lastLuxMs_ = 0;
unsigned long lastGoodLuxMs_ = 0;
constexpr unsigned long kLuxIntervalMs = 250;
// Exponential moving average weight per 250ms sample: ~2s to settle.
constexpr float kLuxSmoothing = 0.15f;

float temperatureC_ = NAN;
float humidity_ = NAN;
unsigned long lastDhtMs_ = 0;
unsigned long lastGoodDhtMs_ = 0;
// The DHT11 can't be read faster than once every 2s, and each read holds
// interrupts off for a few milliseconds. A bedroom doesn't change faster
// than this anyway.
constexpr unsigned long kDhtIntervalMs = 10000;

constexpr unsigned long kStaleAfterMs = 60000;

// The PIR's output is ignored for this long after boot: these modules can
// give a false "motion" while they settle after power-up. Then a change has
// to hold for kPirDebounceMs before it counts, so noise on the wire isn't
// reported as movement.
constexpr unsigned long kPirSettleMs = 10000;
constexpr unsigned long kPirDebounceMs = 50;
bool motion_ = false;
bool motionRaw_ = false;
unsigned long motionRawSinceMs_ = 0;

void updateLight(unsigned long now) {
  if (!lightPresent_ || now - lastLuxMs_ < kLuxIntervalMs) {
    return;
  }
  lastLuxMs_ = now;
  if (!light_.measurementReady()) {
    return;
  }
  float raw = light_.readLightLevel();
  if (raw < 0) {  // the library's error codes are negative
    if (now - lastGoodLuxMs_ > kStaleAfterMs) {
      lux_ = NAN;
    }
    return;
  }
  lastGoodLuxMs_ = now;
  // Snap straight down to real darkness rather than easing toward it: the
  // average of "lamp on" and "lamp off" never quite reaches 0, and a
  // near-zero threshold would otherwise take much longer than the
  // settling time to cross.
  if (isnan(lux_) || raw < 0.5f) {
    lux_ = raw;
  } else {
    lux_ += (raw - lux_) * kLuxSmoothing;
  }
}

void updateClimate(unsigned long now) {
  if (now - lastDhtMs_ < kDhtIntervalMs) {
    return;
  }
  lastDhtMs_ = now;
  float t = dht_.readTemperature();
  float h = dht_.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperatureC_ = t;
    humidity_ = h;
    lastGoodDhtMs_ = now;
  } else if (now - lastGoodDhtMs_ > kStaleAfterMs) {
    temperatureC_ = NAN;
    humidity_ = NAN;
  }
}

void updateMotion(unsigned long now) {
  if (now < kPirSettleMs) {
    return;
  }
  bool raw = digitalRead(kPinPir) == HIGH;
  if (raw != motionRaw_) {
    motionRaw_ = raw;
    motionRawSinceMs_ = now;
    return;
  }
  if (raw != motion_ && now - motionRawSinceMs_ >= kPirDebounceMs) {
    motion_ = raw;
    Serial.println(motion_ ? "[sensors] motion" : "[sensors] still");
  }
}

}  // namespace

void sensorsBegin() {
  // High-resolution mode 2: 0.5 lx steps, which is what a dark bedroom
  // needs -- the interesting thresholds sit in the 0-20 lx range.
  lightPresent_ = light_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, kI2cAddrLight, &Wire);
  if (!lightPresent_) {
    Serial.println("[sensors] no BH1750 at 0x23 -- screen stays at full brightness");
  }
  // A plain input, with no pull-down: the AM312's output is weak, and the
  // ESP32's internal pull-down (about 45k) held it at about 1.6 V, which is
  // neither a high nor a low, so motion was never seen. Measured on the bench
  // with a meter and by watching the pin. A PIR that isn't connected would
  // float, but it is always fitted.
  pinMode(kPinPir, INPUT);
  dht_.begin();
  // First DHT read right away instead of 10s after boot.
  lastDhtMs_ = millis() - kDhtIntervalMs;
}

void sensorsUpdate() {
  unsigned long now = millis();
  updateLight(now);
  updateClimate(now);
  updateMotion(now);
}

float sensorsLux() {
  return lux_;
}

float sensorsTemperatureC() {
  return temperatureC_;
}

float sensorsHumidity() {
  return humidity_;
}

bool sensorsLightPresent() {
  return lightPresent_;
}

bool sensorsMotion() {
  return motion_;
}
