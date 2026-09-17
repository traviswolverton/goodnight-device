// Screen state machine -- HOME / MENU / CAMERA, per DESIGN.md's "Screen
// state machine" section. All timing is millis()-based; nothing in here
// blocks with delay().

#include "screen.h"

#include <TFT_eSPI.h>
#include <WiFi.h>

#include "backlight.h"
#include "timing_config.h"

namespace {

enum class State { HOME, MENU, CAMERA };

const char* kMenuItems[] = {
    "Pin this screen",  // relabeled "Unpin screen" at draw time if pinned
    "Mute alerts",       // relabeled "Unmute alerts" at draw time if muted
    "Refresh now",
    "Network info",
    "Configs",
    "Back",
};
constexpr uint8_t kMenuItemCount = 6;
// Seconds adjusted per encoder detent while editing the backlight
// timeout in the Configs sub-panel.
constexpr int32_t kBacklightStepSeconds = 5;

TFT_eSPI tft;
State state_ = State::HOME;
bool needsRedraw_ = true;

// HOME
StatusPayload lastStatus_;
uint8_t cardIndex_ = 0;
unsigned long nextAutoRotateMs_ = 0;
unsigned long autoRotatePausedUntilMs_ = 0;
int8_t pinnedCardIndex_ = -1;  // -1 = not pinned
bool muted_ = false;

// MENU
uint8_t menuSelection_ = 0;
unsigned long menuIdleDeadlineMs_ = 0;
bool menuShowingNetworkInfo_ = false;
bool menuShowingConfigs_ = false;

// CAMERA
CameraAlert cameraAlert_;
unsigned long cameraDeadlineMs_ = 0;

// -- drawing --

uint16_t colorForLevel(const char* level) {
  if (strcmp(level, "warn") == 0) return TFT_ORANGE;
  if (strcmp(level, "down") == 0) return TFT_RED;
  return TFT_GREEN;
}

void drawBanner() {
  uint16_t bg = muted_ ? TFT_DARKGREY : colorForLevel(lastStatus_.overallLevel);
  tft.fillRect(0, 0, tft.width(), 30, bg);
  tft.setTextColor(TFT_BLACK, bg);
  tft.setTextDatum(ML_DATUM);
  // Fixed buffer, not String -- this runs on every HOME redraw
  // (auto-rotation, encoder input), not just once per message.
  char text[kTextLen + 8];
  snprintf(text, sizeof(text), "%s%s", lastStatus_.overallText, muted_ ? " (muted)" : "");
  tft.drawString(text, 8, 15);
}

void drawDots() {
  if (lastStatus_.serviceCount == 0) return;
  int totalWidth = lastStatus_.serviceCount * 16;
  int startX = (tft.width() - totalWidth) / 2 + 8;
  int y = tft.height() - 10;
  for (uint8_t i = 0; i < lastStatus_.serviceCount; i++) {
    uint16_t color = (i == cardIndex_) ? TFT_WHITE : TFT_DARKGREY;
    tft.fillCircle(startX + i * 16, y, 4, color);
  }
}

void drawCard() {
  int top = 34;
  int bottom = tft.height() - 20;
  tft.fillRect(0, top, tft.width(), bottom - top, TFT_BLACK);

  if (lastStatus_.serviceCount == 0) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Waiting for data...", tft.width() / 2, (top + bottom) / 2);
    return;
  }

  const ServiceReading& card = lastStatus_.services[cardIndex_];
  int midY = (top + bottom) / 2;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(colorForLevel(card.status), TFT_BLACK);
  tft.drawString(card.name, tft.width() / 2, midY - 14);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(card.line, tft.width() / 2, midY + 14);

  if ((int8_t)cardIndex_ == pinnedCardIndex_) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("PINNED", tft.width() - 6, top + 4);
  }
}

void drawHome() {
  drawBanner();
  drawCard();
  drawDots();
}

void drawMenu() {
  tft.fillScreen(TFT_BLACK);

  if (menuShowingNetworkInfo_) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Network info", 10, 10);
    tft.drawString("SSID: " + WiFi.SSID(), 10, 40);
    tft.drawString("IP: " + WiFi.localIP().toString(), 10, 60);
    tft.drawString(
        String("WiFi: ") + (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected"),
        10, 80);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Press to go back", 10, 120);
    return;
  }

  if (menuShowingConfigs_) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Configs", 10, 10);
    char line[40];
    snprintf(line, sizeof(line), "Backlight timeout: %u s", backlightGetTimeoutSeconds());
    tft.drawString(line, 10, 40);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Rotate to adjust (5s steps)", 10, 70);
    tft.drawString("Press to save and go back", 10, 90);
    return;
  }

  // Row height/box tightened from the original 5-item spacing (40px,
  // 34px box) to fit 6 items within the 240px landscape height with
  // real margin left at the bottom, rather than landing right on the
  // edge.
  tft.setTextDatum(TL_DATUM);
  for (uint8_t i = 0; i < kMenuItemCount; i++) {
    int y = 8 + i * 36;
    bool selected = (i == menuSelection_);
    uint16_t bg = selected ? TFT_WHITE : TFT_BLACK;
    uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
    tft.fillRect(0, y - 3, tft.width(), 30, bg);
    tft.setTextColor(fg, bg);

    String label = kMenuItems[i];
    if (i == 0) label = (pinnedCardIndex_ >= 0) ? "Unpin screen" : "Pin this screen";
    if (i == 1) label = muted_ ? "Unmute alerts" : "Mute alerts";
    tft.drawString(label, 10, y + 8);
  }
}

void drawCamera() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CAMERA ALERT", tft.width() / 2, 20);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(cameraAlert_.camera, tft.width() / 2, 60);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(cameraAlert_.text, tft.width() / 2, 90);
  // JPEG-over-HTTP decode (TJpg_Decoder + fetching snapshotUrl) isn't
  // built yet -- this is a placeholder, not the final CAMERA screen.
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("(snapshot decode not implemented yet)", tft.width() / 2, 140);
  tft.drawString("Press to dismiss", tft.width() / 2, 170);
}

// -- state transitions --

void enterHome() {
  state_ = State::HOME;
  needsRedraw_ = true;
}

void enterMenu() {
  state_ = State::MENU;
  menuSelection_ = 0;
  menuShowingNetworkInfo_ = false;
  menuShowingConfigs_ = false;
  menuIdleDeadlineMs_ = millis() + kIdleTimeoutMs;
  needsRedraw_ = true;
}

void enterCamera(const CameraAlert& alert) {
  cameraAlert_ = alert;
  state_ = State::CAMERA;
  cameraDeadlineMs_ = millis() + kIdleTimeoutMs;
  needsRedraw_ = true;
}

}  // namespace

void screenBegin() {
  tft.init();
  tft.setRotation(1);  // portrait panel -> landscape 320x240, per DESIGN.md
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  nextAutoRotateMs_ = millis() + kCardRotationMs;
}

void screenUpdate(const StatusPayload& status, bool isNewStatus) {
  if (isNewStatus) {
    lastStatus_ = status;
    if (state_ == State::HOME) needsRedraw_ = true;
  }

  unsigned long now = millis();

  if (state_ == State::HOME) {
    bool paused = now < autoRotatePausedUntilMs_;
    if (!paused && lastStatus_.serviceCount > 0 && now >= nextAutoRotateMs_) {
      if (pinnedCardIndex_ < 0) {
        cardIndex_ = (cardIndex_ + 1) % lastStatus_.serviceCount;
      }
      nextAutoRotateMs_ = now + kCardRotationMs;
      needsRedraw_ = true;
    }
  } else if (state_ == State::MENU) {
    if (now >= menuIdleDeadlineMs_) {
      enterHome();
    }
  } else if (state_ == State::CAMERA) {
    if (now >= cameraDeadlineMs_) {
      enterHome();
    }
  }

  if (needsRedraw_) {
    switch (state_) {
      case State::HOME:
        drawHome();
        break;
      case State::MENU:
        drawMenu();
        break;
      case State::CAMERA:
        drawCamera();
        break;
    }
    needsRedraw_ = false;
  }
}

void screenOnEncoderDelta(int32_t delta) {
  if (delta == 0) return;
  unsigned long now = millis();

  if (state_ == State::HOME) {
    if (lastStatus_.serviceCount > 0) {
      int32_t idx = ((int32_t)cardIndex_ + delta) % (int32_t)lastStatus_.serviceCount;
      if (idx < 0) idx += lastStatus_.serviceCount;
      cardIndex_ = (uint8_t)idx;
    }
    autoRotatePausedUntilMs_ = now + kCardRotationPauseMs;
    needsRedraw_ = true;
  } else if (state_ == State::MENU && menuShowingConfigs_) {
    // Editing the backlight timeout instead of scrolling the list.
    // Clamp in int32_t before narrowing to uint16_t -- doing the
    // clamp only inside backlightSetTimeoutSeconds would let a
    // negative result wrap around to a huge value on the implicit
    // uint16_t cast, before that function ever got a chance to clamp it.
    int32_t next = (int32_t)backlightGetTimeoutSeconds() + delta * kBacklightStepSeconds;
    if (next < kBacklightMinTimeoutSeconds) next = kBacklightMinTimeoutSeconds;
    if (next > kBacklightMaxTimeoutSeconds) next = kBacklightMaxTimeoutSeconds;
    backlightSetTimeoutSeconds((uint16_t)next);
    menuIdleDeadlineMs_ = now + kIdleTimeoutMs;
    needsRedraw_ = true;
  } else if (state_ == State::MENU && !menuShowingNetworkInfo_) {
    int32_t idx = ((int32_t)menuSelection_ + delta) % (int32_t)kMenuItemCount;
    if (idx < 0) idx += kMenuItemCount;
    menuSelection_ = (uint8_t)idx;
    menuIdleDeadlineMs_ = now + kIdleTimeoutMs;
    needsRedraw_ = true;
  }
  // CAMERA ignores rotation -- there's nothing to navigate there.
}

void screenOnEncoderPress() {
  unsigned long now = millis();

  switch (state_) {
    case State::HOME:
      enterMenu();
      break;

    case State::MENU:
      if (menuShowingNetworkInfo_) {
        menuShowingNetworkInfo_ = false;
        menuIdleDeadlineMs_ = now + kIdleTimeoutMs;
        needsRedraw_ = true;
        break;
      }
      if (menuShowingConfigs_) {
        menuShowingConfigs_ = false;
        menuIdleDeadlineMs_ = now + kIdleTimeoutMs;
        needsRedraw_ = true;
        break;
      }
      menuIdleDeadlineMs_ = now + kIdleTimeoutMs;
      switch (menuSelection_) {
        case 0:  // Pin / unpin this screen
          pinnedCardIndex_ = (pinnedCardIndex_ >= 0) ? -1 : (int8_t)cardIndex_;
          enterHome();
          break;
        case 1:  // Mute / unmute alerts
          muted_ = !muted_;
          enterHome();
          break;
        case 2:
          // Refresh now -- there's no request/reply model here (the
          // server only pushes), so this just redraws HOME immediately
          // from the latest cached payload rather than triggering any
          // network activity.
          enterHome();
          break;
        case 3:  // Network info
          menuShowingNetworkInfo_ = true;
          needsRedraw_ = true;
          break;
        case 4:  // Configs
          menuShowingConfigs_ = true;
          needsRedraw_ = true;
          break;
        case 5:  // Back
          enterHome();
          break;
      }
      break;

    case State::CAMERA:
      enterHome();
      break;
  }
}

void screenOnCameraAlert(const CameraAlert& alert) {
  if (muted_) {
    // Muted means no auto-interrupt -- same "don't surprise the user"
    // principle as the backlight note in DESIGN.md, just applied to the
    // screen instead of the light.
    return;
  }
  enterCamera(alert);
}
