#include "backlight.h"

#include <Arduino.h>

#include "timing_config.h"

namespace {

// See DESIGN.md's pin table.
constexpr uint8_t kPirPin = 27;        // D27
constexpr uint8_t kBacklightPin = 26;  // D26 -- digital on/off, not PWM

unsigned long onUntilMs_ = 0;
bool backlightOn_ = true;

}  // namespace

void backlightBegin() {
  pinMode(kPirPin, INPUT);
  pinMode(kBacklightPin, OUTPUT);
  // Default to on at boot -- matches the "fail toward showing, not
  // hiding" reasoning behind resetting mute/pin on every boot too.
  digitalWrite(kBacklightPin, HIGH);
  onUntilMs_ = millis() + kIdleTimeoutMs;
}

void backlightUpdate() {
  unsigned long now = millis();
  if (digitalRead(kPirPin) == HIGH) {
    onUntilMs_ = now + kIdleTimeoutMs;
    if (!backlightOn_) {
      digitalWrite(kBacklightPin, HIGH);
      backlightOn_ = true;
    }
    return;
  }
  if (backlightOn_ && now >= onUntilMs_) {
    digitalWrite(kBacklightPin, LOW);
    backlightOn_ = false;
  }
}
