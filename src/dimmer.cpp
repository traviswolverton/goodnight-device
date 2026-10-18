#include "dimmer.h"

#include <Arduino.h>
#include <math.h>

namespace {

enum class Zone : uint8_t { Bright, Dim, Dark };

DimmerSettings settings_;
Zone zone_ = Zone::Bright;
unsigned long awakeUntilMs_ = 0;
bool awake_ = false;
unsigned long lastActivityMs_ = 0;
bool idle_ = false;

// A threshold has to be crossed by this fraction (or at least
// kMinHysteresisLux) before the zone changes.
constexpr float kHysteresis = 0.2f;
constexpr float kMinHysteresisLux = 0.5f;

float margin(float threshold) {
  float m = threshold * kHysteresis;
  return m < kMinHysteresisLux ? kMinHysteresisLux : m;
}

Zone zoneFor(float lux) {
  if (isnan(lux)) {
    return Zone::Bright;
  }
  float bright = settings_.brightLux;
  float dark = settings_.darkLux;
  switch (zone_) {
    case Zone::Bright:
      if (lux < dark - margin(dark)) return Zone::Dark;
      if (lux < bright - margin(bright)) return Zone::Dim;
      return Zone::Bright;
    case Zone::Dim:
      if (lux >= bright + margin(bright)) return Zone::Bright;
      if (lux < dark - margin(dark)) return Zone::Dark;
      return Zone::Dim;
    case Zone::Dark:
      if (lux >= bright + margin(bright)) return Zone::Bright;
      // While woken, the screen's own glow can reach the sensor. If that
      // alone could lift the room out of Dark, one wake would keep the
      // screen on all night -- so only a real light (past the bright
      // threshold) ends Dark early; otherwise wait until the wake ends.
      if (!awake_ && lux >= dark + margin(dark)) return Zone::Dim;
      return Zone::Dark;
  }
  return Zone::Bright;
}

DisplayLevel levelNow() {
  if (idle_) {
    return DisplayLevel::Off;
  }
  switch (zone_) {
    case Zone::Bright: return DisplayLevel::Full;
    case Zone::Dim: return DisplayLevel::Dim;
    case Zone::Dark: return awake_ ? DisplayLevel::Dim : DisplayLevel::Off;
  }
  return DisplayLevel::Full;
}

void noteActivity() {
  lastActivityMs_ = millis();
  if (idle_) {
    idle_ = false;
    Serial.println("[dim] idle ended");
  }
}

void extendWake() {
  awake_ = true;
  awakeUntilMs_ = millis() + (unsigned long)settings_.wakeSeconds * 1000UL;
}

}  // namespace

void dimmerBegin(const DimmerSettings& settings) {
  lastActivityMs_ = millis();
  dimmerApply(settings);
  displaySetLevel(levelNow());
}

void dimmerApply(const DimmerSettings& settings) {
  settings_ = settings;
  // A dark threshold above the bright one would make Dim unreachable and
  // the zone logic ambiguous; treat it as "no dim band".
  if (settings_.darkLux > settings_.brightLux) {
    settings_.darkLux = settings_.brightLux;
  }
  displaySetDimContrast(settings_.dimContrast);
}

const DimmerSettings& dimmerSettings() {
  return settings_;
}

void dimmerUpdate(float lux) {
  zone_ = zoneFor(lux);
  if (zone_ != Zone::Dark) {
    awake_ = false;
  } else if (awake_ && (long)(millis() - awakeUntilMs_) >= 0) {
    awake_ = false;
  }
  if (!idle_ && settings_.idleOffMinutes > 0 &&
      millis() - lastActivityMs_ >= (unsigned long)settings_.idleOffMinutes * 60000UL) {
    idle_ = true;
    Serial.printf("[dim] idle for %u min, screen off\n", settings_.idleOffMinutes);
  } else if (idle_ && settings_.idleOffMinutes == 0) {
    idle_ = false;
  }
  displaySetLevel(levelNow());
}

bool dimmerWake() {
  bool wasIdle = idle_;
  noteActivity();
  if (zone_ != Zone::Dark) {
    displaySetLevel(levelNow());
    return wasIdle;
  }
  bool wasOff = !awake_;
  extendWake();
  displaySetLevel(levelNow());
  return wasOff || wasIdle;
}

void dimmerMotion() {
  noteActivity();
}

bool dimmerIdle() {
  return idle_;
}

DisplayLevel dimmerLevel() {
  return levelNow();
}

bool dimmerRoomIsDark() {
  return zone_ == Zone::Dark;
}

const char* dimmerLevelName() {
  switch (levelNow()) {
    case DisplayLevel::Off: return "off";
    case DisplayLevel::Dim: return "dim";
    case DisplayLevel::Full: return "full";
  }
  return "full";
}
