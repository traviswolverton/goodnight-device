#include "display.h"

#include <Wire.h>

#include "pins.h"

namespace {

// 0.91" 128x32 modules are the "UNIVISION" SSD1306 variant in U8g2's
// naming. Reset isn't wired (these boards reset themselves at power-up).
// Swap U8G2_R0 for U8G2_R2 if the panel ends up mounted upside down.
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2_(U8G2_R0, U8X8_PIN_NONE, kPinI2cScl, kPinI2cSda);

bool present_ = false;
DisplayLevel level_ = DisplayLevel::Full;
uint8_t dimContrast_ = 1;

constexpr unsigned long kDriftIntervalMs = 3UL * 60UL * 1000UL;
// A small square walk, so nothing ever moves more than a pixel from
// where it's meant to be.
constexpr uint8_t kDrift[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
uint8_t driftStep_ = 0;
unsigned long lastDriftMs_ = 0;

// SSD1306 commands. The Full values are what U8g2's own init sequence
// for this panel sets, so returning to Full restores the stock panel.
constexpr uint8_t kCmdPrecharge = 0xD9;
constexpr uint8_t kCmdVcomh = 0xDB;
constexpr uint8_t kPrechargeFull = 0xF1;
constexpr uint8_t kPrechargeDim = 0x11;
constexpr uint8_t kVcomhFull = 0x40;
constexpr uint8_t kVcomhDim = 0x00;

// The panel can't be read back, and a brief power dip or a bus fault can
// reset it to "display off, memory blank" without the firmware knowing. A
// re-init fixes that but blinks the panel, so it's only done when the panel
// has stopped answering for a check and then answered again. The check
// itself (an address ping) is invisible.
constexpr unsigned long kRefreshIntervalMs = 10UL * 1000UL;
unsigned long lastRefreshMs_ = 0;
bool needReinit_ = false;

bool probePanel() {
  Wire.beginTransmission(kI2cAddrOled);
  return Wire.endTransmission() == 0;
}

void applyLevel() {
  if (!present_) {
    return;
  }
  switch (level_) {
    case DisplayLevel::Off:
      u8g2_.setPowerSave(1);
      break;
    case DisplayLevel::Dim:
      u8g2_.sendF("ca", kCmdPrecharge, kPrechargeDim);
      u8g2_.sendF("ca", kCmdVcomh, kVcomhDim);
      u8g2_.setContrast(dimContrast_);
      u8g2_.setPowerSave(0);
      break;
    case DisplayLevel::Full:
      u8g2_.sendF("ca", kCmdPrecharge, kPrechargeFull);
      u8g2_.sendF("ca", kCmdVcomh, kVcomhFull);
      u8g2_.setContrast(255);
      u8g2_.setPowerSave(0);
      break;
  }
}

}  // namespace

bool displayBegin() {
  present_ = probePanel();
  if (!present_) {
    Serial.println("[display] no SSD1306 at 0x3C");
    return false;
  }
  u8g2_.setBusClock(kI2cClockHz);
  u8g2_.begin();
  applyLevel();
  lastDriftMs_ = millis();
  return true;
}

void displayRefresh() {
  unsigned long now = millis();
  if (now - lastRefreshMs_ < kRefreshIntervalMs) {
    return;
  }
  lastRefreshMs_ = now;
  if (!present_) {
    // Missing at boot: the panel may just have answered late.
    if (!probePanel()) {
      return;
    }
    present_ = true;
    Serial.println("[display] SSD1306 found late; starting it");
    u8g2_.setBusClock(kI2cClockHz);
    u8g2_.begin();
    applyLevel();
    return;
  }
  if (!probePanel()) {
    needReinit_ = true;
    return;
  }
  if (needReinit_) {
    needReinit_ = false;
    Serial.println("[display] panel answered again after missing; re-initialising it");
    u8g2_.initDisplay();
    applyLevel();
    displaySend();  // the frame buffer still holds what should be showing
  }
}

bool displayPresent() {
  return present_;
}

U8G2& displayCanvas() {
  return u8g2_;
}

uint8_t displayOffsetX() {
  return kDrift[driftStep_][0];
}

uint8_t displayOffsetY() {
  return kDrift[driftStep_][1];
}

void displayUpdateDrift() {
  unsigned long now = millis();
  if (now - lastDriftMs_ >= kDriftIntervalMs) {
    lastDriftMs_ = now;
    driftStep_ = (driftStep_ + 1) % (sizeof(kDrift) / sizeof(kDrift[0]));
  }
}

void displayStartFrame() {
  u8g2_.clearBuffer();
}

void displaySend() {
  // Nothing is lit while the panel is off, so there's no reason to spend
  // I2C time pushing a frame nobody can see.
  if (present_ && level_ != DisplayLevel::Off) {
    u8g2_.sendBuffer();
  }
}

void displaySetLevel(DisplayLevel level) {
  if (level == level_) {
    return;
  }
  level_ = level;
  applyLevel();
}

DisplayLevel displayLevel() {
  return level_;
}

void displaySetDimContrast(uint8_t contrast) {
  if (contrast == dimContrast_) {
    return;
  }
  dimContrast_ = contrast;
  if (level_ == DisplayLevel::Dim) {
    applyLevel();
  }
}
