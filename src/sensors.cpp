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

}  // namespace

void sensorsBegin() {
  // High-resolution mode 2: 0.5 lx steps, which is what a dark bedroom
  // needs -- the interesting thresholds sit in the 0-20 lx range.
  lightPresent_ = light_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, kI2cAddrLight, &Wire);
  if (!lightPresent_) {
    Serial.println("[sensors] no BH1750 at 0x23 -- screen stays at full brightness");
  }
  dht_.begin();
  // First DHT read right away instead of 10s after boot.
  lastDhtMs_ = millis() - kDhtIntervalMs;
}

void sensorsUpdate() {
  unsigned long now = millis();
  updateLight(now);
  updateClimate(now);
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
