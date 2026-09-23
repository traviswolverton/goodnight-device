#include "backlight.h"

#include <Arduino.h>

#include "settings_store.h"

namespace {

// See DESIGN.md's pin table.
constexpr uint8_t kPirPin = 27;        // D27
constexpr uint8_t kBacklightPin = 26;  // D26 -- digital on/off, not PWM

// Deliberately this module's own constant now, not DESIGN.md's shared
// kIdleTimeoutMs (still used by MENU/CAMERA) -- this timeout is driven
// by PIR-or-user-activity and is runtime-adjustable via the Configs
// menu, neither of which applies to MENU/CAMERA's idle timeout. See
// DESIGN.md's "Backlight / presence" section for why it split off.
constexpr uint16_t kDefaultTimeoutSeconds = 18;

unsigned long onUntilMs_ = 0;
bool backlightOn_ = true;
uint16_t timeoutSeconds_ = kDefaultTimeoutSeconds;

void extendTimer(unsigned long now) {
  onUntilMs_ = now + (unsigned long)timeoutSeconds_ * 1000UL;
  if (!backlightOn_) {
    digitalWrite(kBacklightPin, HIGH);
    backlightOn_ = true;
  }
}

}  // namespace

void backlightBegin() {
  pinMode(kPirPin, INPUT);
  pinMode(kBacklightPin, OUTPUT);
  // Default to on at boot -- matches the "fail toward showing, not
  // hiding" reasoning behind resetting mute/pin on every boot too. (The
  // timeout *length* itself is persisted below; whether the backlight
  // starts lit is not the same question and stays unconditional.)
  digitalWrite(kBacklightPin, HIGH);
  timeoutSeconds_ = settingsLoadBacklightTimeoutSeconds(kDefaultTimeoutSeconds);
  onUntilMs_ = millis() + (unsigned long)timeoutSeconds_ * 1000UL;
}

void backlightUpdate() {
  unsigned long now = millis();
  if (digitalRead(kPirPin) == HIGH) {
    extendTimer(now);
    return;
  }
  if (backlightOn_ && now >= onUntilMs_) {
    digitalWrite(kBacklightPin, LOW);
    backlightOn_ = false;
  }
}

void backlightNotifyActivity() {
  extendTimer(millis());
}

uint16_t backlightGetTimeoutSeconds() {
  return timeoutSeconds_;
}

void backlightSetTimeoutSeconds(uint16_t seconds) {
  if (seconds < kBacklightMinTimeoutSeconds) seconds = kBacklightMinTimeoutSeconds;
  if (seconds > kBacklightMaxTimeoutSeconds) seconds = kBacklightMaxTimeoutSeconds;
  timeoutSeconds_ = seconds;
  // Applies immediately, extending from now -- so dialing the timeout
  // down doesn't instantly flip the backlight off mid-edit.
  extendTimer(millis());
}

void backlightPersistTimeoutSeconds() {
  settingsSaveBacklightTimeoutSeconds(timeoutSeconds_);
}
