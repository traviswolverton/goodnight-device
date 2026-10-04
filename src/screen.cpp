#include "screen.h"

#include <Arduino.h>
#include <math.h>

#include "display.h"

namespace {

// 24 px bold: 4-6 characters across a 128 px panel, readable at arm's
// length. Chosen by eye from a few candidates on the real panel.
const uint8_t* const kBigFont = u8g2_font_helvB24_tf;
// 6x10: 21 characters by 3 lines; for the small corner marks and the few
// screens that need a lot of words (setup details, the fault list).
const uint8_t* const kSmallFont = u8g2_font_6x10_tf;
constexpr uint8_t kSmallBaselines[3] = {8, 18, 28};
constexpr size_t kLineLen = 22;

constexpr unsigned long kInputShownMs = 1500;
constexpr unsigned long kPageReturnMs = 8000;
// A held stick repeats; this keeps that from spinning through the pages.
constexpr unsigned long kPageStepMinMs = 300;
// In setup mode the big passkey and the small details take turns.
constexpr unsigned long kSetupAltMs = 3000;

enum class Page : uint8_t { Time, Temperature, Humidity, Light, Fault };

Page page_ = Page::Time;
unsigned long pageChangedMs_ = 0;
int pendingStep_ = 0;  // +1 / -1 from the stick, applied in screenUpdate()

bool echo_ = false;
char inputText_[kLineLen] = "";
unsigned long inputShownAtMs_ = 0;

char lastKey_[96] = "";
DisplayLevel lastLevel_ = DisplayLevel::Off;

Page nextPage(Page p, int step, bool hasFault) {
  int last = hasFault ? 4 : 3;
  int i = (int)p + step;
  if (i < 0) i = last;
  if (i > last) i = 0;
  return (Page)i;
}

// Pixel width of a string in the given font.
int widthOf(U8G2& g, const uint8_t* font, const char* text) {
  g.setFont(font);
  return g.getStrWidth(text);
}

// Draws text centred, in the big font, with its digits vertically centred.
void drawBigCentred(U8G2& g, const char* text, int offX, int offY) {
  g.setFont(kBigFont);
  int x = (kDisplayWidth - g.getStrWidth(text)) / 2 + offX;
  int y = (kDisplayHeight + g.getAscent()) / 2 + offY;
  g.drawStr(x, y, text);
}

void formatTemperature(char* out, size_t size, float c) {
  if (isnan(c)) {
    snprintf(out, size, "--\xB0" "F");
  } else {
    snprintf(out, size, "%.0f\xB0" "F", c * 9.0f / 5.0f + 32.0f);
  }
}

void formatLux(char* out, size_t size, float lux) {
  if (isnan(lux)) {
    snprintf(out, size, "-- lx");
  } else if (lux >= 10000) {
    snprintf(out, size, "%.0fk lx", lux / 1000.0f);
  } else if (lux >= 1000) {
    snprintf(out, size, "%.1fk lx", lux / 1000.0f);
  } else if (lux >= 10) {
    snprintf(out, size, "%.0f lx", lux);
  } else {
    snprintf(out, size, "%.1f lx", lux);
  }
}

}  // namespace

void screenSetInputEcho(bool on) {
  echo_ = on;
}

void screenFlash(const char* text) {
  snprintf(inputText_, kLineLen, "%s", text);
  inputShownAtMs_ = millis();
}

void screenNoteInput(const InputEvent& event) {
  // Left/right always page; everything else only shows when the echo is on.
  if (!echo_ && event.kind != InputKind::JoyLeft && event.kind != InputKind::JoyRight) {
    return;
  }
  switch (event.kind) {
    case InputKind::ButtonShort:
      snprintf(inputText_, kLineLen, "BTN %u", event.button);
      break;
    case InputKind::ButtonLong:
      snprintf(inputText_, kLineLen, "HOLD %u", event.button);
      break;
    case InputKind::JoyLeft:
      pendingStep_ = -1;
      return;
    case InputKind::JoyRight:
      pendingStep_ = 1;
      return;
    case InputKind::JoyUp:
      snprintf(inputText_, kLineLen, "UP");
      break;
    case InputKind::JoyDown:
      snprintf(inputText_, kLineLen, "DOWN");
      break;
    default:
      snprintf(inputText_, kLineLen, "PRESS");
      break;
  }
  inputShownAtMs_ = millis();
}

void screenUpdate(const ScreenModel& m) {
  if (!displayPresent()) {
    return;
  }
  unsigned long now = millis();
  bool hasFault = m.hardwareNote != nullptr;

  // Stick left/right, then back to the clock after a while.
  if (pendingStep_ != 0) {
    if (now - pageChangedMs_ >= kPageStepMinMs) {
      page_ = nextPage(page_, pendingStep_, hasFault);
      pageChangedMs_ = now;
    }
    pendingStep_ = 0;
  }
  if (page_ == Page::Fault && !hasFault) {
    page_ = Page::Time;
  }
  if (page_ != Page::Time && now - pageChangedMs_ >= kPageReturnMs) {
    page_ = Page::Time;
  }

  bool showInput = inputText_[0] && now - inputShownAtMs_ < kInputShownMs;
  if (!showInput) {
    inputText_[0] = '\0';
  }

  // What goes on the glass, as text, so "did anything change" is a string
  // compare and the panel is only re-sent when it would look different.
  char big[24] = "";        // the big line (empty when a small layout is used)
  char ampm[3] = "";        // small AM/PM beside a time
  char small[3][kLineLen] = {"", "", ""};  // small layout, when big is empty
  bool marks = true;        // the W / M corner marks and the fault "!"

  if (m.setup.active) {
    marks = false;
    if ((now / kSetupAltMs) % 2 == 0) {
      snprintf(big, sizeof(big), "%06u", (unsigned)m.setup.passkey);
    } else {
      snprintf(small[0], kLineLen, "Setup: %s", m.setup.name);
      snprintf(small[1], kLineLen, "Left %u:%02u", m.setup.secondsLeft / 60,
               m.setup.secondsLeft % 60);
      snprintf(small[2], kLineLen, "%s", m.setup.status);
    }
  } else if (showInput) {
    snprintf(big, sizeof(big), "%s", inputText_);
  } else {
    switch (page_) {
      case Page::Time:
        if (m.timeValid) {
          snprintf(big, sizeof(big), "%u:%02u", m.hour12, m.minute);
          snprintf(ampm, sizeof(ampm), "%s", m.pm ? "PM" : "AM");
        } else {
          snprintf(big, sizeof(big), "--:--");
        }
        break;
      case Page::Temperature:
        formatTemperature(big, sizeof(big), m.temperatureC);
        break;
      case Page::Humidity:
        if (isnan(m.humidity)) {
          snprintf(big, sizeof(big), "--%%");
        } else {
          snprintf(big, sizeof(big), "%.0f%%", m.humidity);
        }
        break;
      case Page::Light:
        formatLux(big, sizeof(big), m.lux);
        break;
      case Page::Fault:
        snprintf(small[0], kLineLen, "Missing:");
        snprintf(small[1], kLineLen, "%s", m.hardwareNote);
        break;
    }
  }

  displayUpdateDrift();
  uint8_t offX = displayOffsetX();
  uint8_t offY = displayOffsetY();
  DisplayLevel level = displayLevel();

  char key[96];
  // Problem marks only: nothing shows while everything is fine. WiFi down
  // implies MQTT down, so the M is only for "WiFi up but no broker".
  bool wifiDown = marks && !m.wifi;
  bool mqttDown = marks && m.wifi && !m.mqtt;
  bool faultMark = marks && hasFault;
  snprintf(key, sizeof(key), "%s|%s|%s|%s|%s|%d%d%d|%u%u", big, ampm, small[0], small[1], small[2],
           wifiDown, mqttDown, faultMark, offX, offY);
  // Re-send after the panel comes back on too: while it was off, frames
  // weren't sent, so its memory still holds whatever was there before.
  bool changed = strcmp(key, lastKey_) != 0 || (level != lastLevel_ && level != DisplayLevel::Off);
  lastLevel_ = level;
  if (!changed) {
    return;
  }
  strcpy(lastKey_, key);

  displayStartFrame();
  U8G2& g = displayCanvas();
  if (big[0]) {
    if (ampm[0]) {
      // Time with a small AM/PM sitting on its baseline, centred together.
      int bigW = widthOf(g, kBigFont, big);
      int ampmW = widthOf(g, kSmallFont, ampm);
      int total = bigW + 3 + ampmW;
      int x = (kDisplayWidth - total) / 2 + offX;
      int y = (kDisplayHeight + (g.setFont(kBigFont), g.getAscent())) / 2 + offY;
      g.setFont(kBigFont);
      g.drawStr(x, y, big);
      g.setFont(kSmallFont);
      g.drawStr(x + bigW + 3, y, ampm);
    } else {
      drawBigCentred(g, big, offX, offY);
    }
  } else {
    g.setFont(kSmallFont);
    for (int i = 0; i < 3; i++) {
      g.drawStr(offX, kSmallBaselines[i] + offY, small[i]);
    }
  }
  // A problem is a white box with a dark letter in a top corner.
  g.setFont(kSmallFont);
  auto problemMark = [&](int x, const char* letter) {
    g.setDrawColor(1);
    g.drawBox(x, offY, 7, 9);
    g.setDrawColor(0);
    g.drawStr(x + 1, kSmallBaselines[0] + offY, letter);
    g.setDrawColor(1);
  };
  if (wifiDown) {
    problemMark(kDisplayWidth - 9 + offX, "W");
  } else if (mqttDown) {
    problemMark(kDisplayWidth - 9 + offX, "M");
  }
  if (faultMark) {
    problemMark(offX, "!");
  }
  displaySend();
}
