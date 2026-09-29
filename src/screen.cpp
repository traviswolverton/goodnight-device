#include "screen.h"

#include <Arduino.h>
#include <math.h>

#include "display.h"

namespace {

// 6x10: 21 characters by 3 lines on a 128x32 panel. Baselines leave a
// pixel below the lowest descender for the burn-in drift.
const uint8_t* const kFont = u8g2_font_6x10_tf;
constexpr uint8_t kBaselines[3] = {8, 18, 28};
constexpr size_t kLineLen = 22;

constexpr unsigned long kInputShownMs = 3000;
char inputLine_[kLineLen] = "";
unsigned long inputShownAtMs_ = 0;

char lastLines_[3][kLineLen] = {};
uint8_t lastOffset_ = 0xFF;
DisplayLevel lastLevel_ = DisplayLevel::Off;

const char* joyName(InputKind kind) {
  switch (kind) {
    case InputKind::JoyUp: return "up";
    case InputKind::JoyDown: return "down";
    case InputKind::JoyLeft: return "left";
    case InputKind::JoyRight: return "right";
    default: return "press";
  }
}

void formatReadings(char* out, const ScreenModel& m) {
  char t[8] = "--";
  char h[6] = "--";
  char l[9] = "--";
  if (!isnan(m.temperatureC)) snprintf(t, sizeof(t), "%.0f", m.temperatureC);
  if (!isnan(m.humidity)) snprintf(h, sizeof(h), "%.0f", m.humidity);
  if (!isnan(m.lux)) snprintf(l, sizeof(l), m.lux < 10 ? "%.1f" : "%.0f", m.lux);
  // Leaves the last 3 columns for the connection marks.
  snprintf(out, kLineLen, "%sC %s%% %slx", t, h, l);
}

}  // namespace

void screenNoteInput(const InputEvent& event) {
  switch (event.kind) {
    case InputKind::ButtonShort:
      snprintf(inputLine_, kLineLen, "Button %u", event.button);
      break;
    case InputKind::ButtonLong:
      snprintf(inputLine_, kLineLen, "Button %u (long)", event.button);
      break;
    default:
      snprintf(inputLine_, kLineLen, "Stick %s", joyName(event.kind));
      break;
  }
  inputShownAtMs_ = millis();
}

void screenUpdate(const ScreenModel& m) {
  if (!displayPresent()) {
    return;
  }
  char lines[3][kLineLen];
  formatReadings(lines[0], m);

  if (!m.wifi) {
    snprintf(lines[1], kLineLen, "Joining WiFi...");
  } else if (!m.mqtt) {
    snprintf(lines[1], kLineLen, "Connecting MQTT...");
  } else {
    snprintf(lines[1], kLineLen, "Connected");
  }

  if (inputLine_[0] && millis() - inputShownAtMs_ < kInputShownMs) {
    snprintf(lines[2], kLineLen, "%s", inputLine_);
  } else {
    inputLine_[0] = '\0';
    snprintf(lines[2], kLineLen, "%s", m.hardwareNote ? m.hardwareNote : "");
  }

  displayUpdateDrift();
  uint8_t offset = displayOffsetX() | (displayOffsetY() << 1);
  DisplayLevel level = displayLevel();
  // Re-send after the panel comes back on too: while it was off, frames
  // weren't sent, so its memory still holds whatever was there before.
  bool changed = offset != lastOffset_ || (level != lastLevel_ && level != DisplayLevel::Off);
  for (int i = 0; i < 3 && !changed; i++) {
    changed = strcmp(lines[i], lastLines_[i]) != 0;
  }
  lastLevel_ = level;
  if (!changed) {
    return;
  }
  memcpy(lastLines_, lines, sizeof(lines));
  lastOffset_ = offset;

  displayStartFrame();
  U8G2& g = displayCanvas();
  uint8_t x = displayOffsetX();
  uint8_t y = displayOffsetY();
  g.setFont(kFont);
  for (int i = 0; i < 3; i++) {
    g.drawStr(x, kBaselines[i] + y, lines[i]);
  }
  // Connection marks, top right: W = WiFi, M = MQTT, blank when down.
  g.drawStr(x + kDisplayWidth - 1 - 12, kBaselines[0] + y, m.wifi ? "W" : " ");
  g.drawStr(x + kDisplayWidth - 1 - 6, kBaselines[0] + y, m.mqtt ? "M" : " ");
  displaySend();
}
