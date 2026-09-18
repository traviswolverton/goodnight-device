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

// Configs is itself a small sub-menu now (more than one adjustable
// value), nested one level inside MENU the same way MENU nests inside
// HOME: a list to pick a field from, then an edit view for whichever
// field was picked.
enum class ConfigsField : uint8_t { kNone, kBacklightTimeout, kCardSensitivity };
const char* kConfigsItems[] = {"Backlight timeout", "Card sensitivity", "Back"};
constexpr uint8_t kConfigsItemCount = 3;
ConfigsField configsEditing_ = ConfigsField::kNone;
uint8_t configsSelection_ = 0;

// Raw encoder counts per HOME card change -- higher = less sensitive.
// Added after real-world use: a small physical turn was jumping through
// 4-5 cards, which matches a well-known characteristic of these
// encoders (many produce 4 raw quadrature transitions per detent).
// Rather than chase exact per-board hardware calibration, this is just
// user-adjustable. accumulator_ carries a partial turn across calls
// rather than discarding it, so a series of small nudges still adds up
// correctly instead of each one being rounded away.
int32_t cardStepAccumulator_ = 0;
int32_t cardStepDivisor_ = 4;
constexpr int32_t kCardStepDivisorMin = 1;
constexpr int32_t kCardStepDivisorMax = 10;

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

// Per-line horizontal scroll state for the currently shown HOME card.
// Generic across any card: index 0 is always the name/title line,
// indices 1.. are detail lines, matching ServiceReading's layout below.
// Sized for the richest template (name + kMaxDetailLines).
unsigned long nextScrollTickMs_ = 0;
int scrollOffsetPx_[1 + kMaxDetailLines] = {0};
constexpr unsigned long kScrollTickMs = 120;
constexpr int kScrollStepPx = 3;

void resetCardScroll() {
  for (int i = 0; i < 1 + kMaxDetailLines; i++) scrollOffsetPx_[i] = 0;
}

// -- drawing --

uint16_t colorForLevel(const char* level) {
  if (strcmp(level, "warn") == 0) return TFT_ORANGE;
  if (strcmp(level, "down") == 0) return TFT_RED;
  return TFT_GREEN;
}

void drawBanner() {
  tft.setTextFont(2);  // explicit, not assumed -- card rendering now
                        // toggles fonts (font 4) between calls
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

// Width reserved on the title line (line 0 only) for the status icon
// drawn by drawTitleIcon() -- the title's own text viewport starts
// after this, so the marquee scroll never runs underneath the icon.
constexpr int kTitleIconInset = 20;

// Generic multi-line card renderer. Line 0 is always the name/title
// line (colored by status); indices 1.. are detail lines (white). A
// card's "template" is nothing more than its total line count (1 +
// detailLineCount) -- there's no separate concept to keep in sync with
// this, no per-service special-casing anywhere in this file. See
// DESIGN.md's "Card templates" note.
//
// If onlyIfScrolling is true, a line that already fits is skipped
// entirely rather than redrawn -- this is what lets the scroll ticker
// touch only the lines that actually animate, instead of repainting
// the whole card every tick (which was visibly flickering the lines
// that never even change).
//
// leftInset shifts the line's viewport right by that many pixels --
// used only for the title line, to leave room for the status icon.
void drawCardLine(const char* text, int lineIndex, int top, int lineHeight, uint16_t color,
                   bool onlyIfScrolling, int leftInset = 0) {
  int lineTop = top + lineIndex * lineHeight;
  int midY = lineHeight / 2;
  int textW = tft.textWidth(text);
  int availW = tft.width() - 12 - leftInset;
  bool scrolls = textW > availW;

  if (onlyIfScrolling && !scrolls) return;

  tft.setViewport(6 + leftInset, lineTop, availW, lineHeight);
  tft.setTextColor(color, TFT_BLACK);
  if (!scrolls) {
    tft.drawString(text, 0, midY);
  } else {
    int cycle = textW + 40;  // 40px gap before the marquee repeats
    int x = -(scrollOffsetPx_[lineIndex] % cycle);
    tft.drawString(text, x, midY);
    tft.drawString(text, x + cycle, midY);  // trailing copy, seamless wrap
  }
  tft.resetViewport();
}

// Small shape indicating a card's status, drawn to the left of the
// title text -- the same information already carried by the title's
// text color, just readable at a glance without reading the color.
// Distinct shapes rather than a checkmark/warning glyph: our fonts
// (see include/User_Setup.h) are monochrome ASCII/Latin bitmap sets
// with no emoji or symbol glyphs, so this is drawn with plain TFT
// primitives instead of any font at all. Only called from the
// full-card redraw path -- the shape never changes without a new
// status message triggering that path anyway, so the scroll ticker
// has no reason to touch it.
void drawTitleIcon(const char* status, int top, int lineHeight) {
  int cx = 6 + kTitleIconInset / 2;
  int cy = top + lineHeight / 2;
  uint16_t color = colorForLevel(status);
  if (strcmp(status, "warn") == 0) {
    tft.fillTriangle(cx, cy - 6, cx - 6, cy + 6, cx + 6, cy + 6, color);
  } else if (strcmp(status, "down") == 0) {
    tft.fillRect(cx - 6, cy - 6, 12, 12, color);
  } else {
    tft.fillCircle(cx, cy, 6, color);
  }
}

// Draws every line of the current card fresh (full redraw path).
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
  int totalLines = 1 + card.detailLineCount;
  int lineHeight = (bottom - top) / totalLines;

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  drawCardLine(card.title, 0, top, lineHeight, colorForLevel(card.status), false, kTitleIconInset);
  drawTitleIcon(card.status, top, lineHeight);
  for (uint8_t i = 0; i < card.detailLineCount; i++) {
    drawCardLine(card.detailLines[i], 1 + i, top, lineHeight, TFT_WHITE, false);
  }
  tft.setTextFont(2);  // restore the font the rest of the UI expects

  if ((int8_t)cardIndex_ == pinnedCardIndex_) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("PINNED", tft.width() - 6, top + 4);
  }
}

// Called from the scroll ticker only -- redraws just the lines that are
// actually scrolling on whichever card currently shows, nothing else.
void updateCardScroll(int top, int bottom) {
  if (lastStatus_.serviceCount == 0) return;
  const ServiceReading& card = lastStatus_.services[cardIndex_];
  int totalLines = 1 + card.detailLineCount;
  int lineHeight = (bottom - top) / totalLines;

  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  drawCardLine(card.title, 0, top, lineHeight, colorForLevel(card.status), true, kTitleIconInset);
  for (uint8_t i = 0; i < card.detailLineCount; i++) {
    drawCardLine(card.detailLines[i], 1 + i, top, lineHeight, TFT_WHITE, true);
  }
  tft.setTextFont(2);
}

void drawHome() {
  drawBanner();
  drawCard();
  drawDots();
}

void drawMenu() {
  tft.setTextFont(2);  // explicit, not assumed -- see drawBanner()
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

    if (configsEditing_ == ConfigsField::kNone) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Configs", 10, 10);
      for (uint8_t i = 0; i < kConfigsItemCount; i++) {
        int y = 40 + i * 30;
        bool selected = (i == configsSelection_);
        uint16_t bg = selected ? TFT_WHITE : TFT_BLACK;
        uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
        tft.fillRect(0, y - 3, tft.width(), 26, bg);
        tft.setTextColor(fg, bg);
        tft.drawString(kConfigsItems[i], 10, y + 5);
      }
      return;
    }

    if (configsEditing_ == ConfigsField::kBacklightTimeout) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Backlight timeout", 10, 10);
      char line[40];
      snprintf(line, sizeof(line), "%u s", backlightGetTimeoutSeconds());
      tft.drawString(line, 10, 40);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.drawString("Rotate to adjust (5s steps)", 10, 70);
      tft.drawString("Press to save and go back", 10, 90);
      return;
    }

    // ConfigsField::kCardSensitivity
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Card sensitivity", 10, 10);
    char line[40];
    snprintf(line, sizeof(line), "%ld raw steps / card", (long)cardStepDivisor_);
    tft.drawString(line, 10, 40);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Rotate to adjust (lower = faster)", 10, 70);
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
  tft.setTextFont(2);  // explicit, not assumed -- see drawBanner()
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
  cardStepAccumulator_ = 0;  // don't let a stale partial turn from before
                             // this MENU visit suddenly complete later
  needsRedraw_ = true;
}

void enterMenu() {
  state_ = State::MENU;
  menuSelection_ = 0;
  menuShowingNetworkInfo_ = false;
  menuShowingConfigs_ = false;
  configsEditing_ = ConfigsField::kNone;
  configsSelection_ = 0;
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
        resetCardScroll();  // fresh marquee start for the newly-shown card
      }
      nextAutoRotateMs_ = now + kCardRotationMs;
      needsRedraw_ = true;
    }
    // Ticks scroll offsets for whichever card is currently shown,
    // redrawing only the lines that actually need it -- generic across
    // any card, not tied to a particular one. Ticks its own timer
    // rather than riding the auto-rotate one, since scrolling needs to
    // keep moving even while rotation itself is paused by manual input.
    // Calls the lightweight per-line update directly instead of setting
    // needsRedraw_ -- going through the normal full-card redraw path
    // 8 times a second was flickering lines that never even change.
    if (lastStatus_.serviceCount > 0 && now >= nextScrollTickMs_) {
      nextScrollTickMs_ = now + kScrollTickMs;
      for (int i = 0; i < 1 + kMaxDetailLines; i++) scrollOffsetPx_[i] += kScrollStepPx;
      updateCardScroll(34, tft.height() - 20);
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
    // cardStepDivisor_ raw encoder counts are required per card change;
    // the accumulator carries a partial turn across calls so a series of
    // small nudges below the threshold still adds up instead of each one
    // being rounded away. autoRotatePausedUntilMs_ still updates on every
    // call (any input means "the user is doing something right now"),
    // but needsRedraw_ only fires once a full step actually lands.
    cardStepAccumulator_ += delta;
    int32_t steps = cardStepAccumulator_ / cardStepDivisor_;
    cardStepAccumulator_ -= steps * cardStepDivisor_;
    if (steps != 0 && lastStatus_.serviceCount > 0) {
      int32_t idx = ((int32_t)cardIndex_ + steps) % (int32_t)lastStatus_.serviceCount;
      if (idx < 0) idx += lastStatus_.serviceCount;
      cardIndex_ = (uint8_t)idx;
      resetCardScroll();  // fresh marquee start for the newly-shown card
      needsRedraw_ = true;
    }
    autoRotatePausedUntilMs_ = now + kCardRotationPauseMs;
  } else if (state_ == State::MENU && menuShowingConfigs_ &&
             configsEditing_ == ConfigsField::kNone) {
    // Scrolling the Configs list itself (Backlight timeout / Card
    // sensitivity / Back), not editing a value yet.
    int32_t idx = ((int32_t)configsSelection_ + delta) % (int32_t)kConfigsItemCount;
    if (idx < 0) idx += kConfigsItemCount;
    configsSelection_ = (uint8_t)idx;
    menuIdleDeadlineMs_ = now + kIdleTimeoutMs;
    needsRedraw_ = true;
  } else if (state_ == State::MENU && menuShowingConfigs_ &&
             configsEditing_ == ConfigsField::kBacklightTimeout) {
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
  } else if (state_ == State::MENU && menuShowingConfigs_) {
    // configsEditing_ == kCardSensitivity
    int32_t next = cardStepDivisor_ + delta;
    if (next < kCardStepDivisorMin) next = kCardStepDivisorMin;
    if (next > kCardStepDivisorMax) next = kCardStepDivisorMax;
    cardStepDivisor_ = next;
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
        if (configsEditing_ != ConfigsField::kNone) {
          // Save (already applied live as it was rotated) and return to
          // the Configs list, not all the way out to the top-level menu.
          configsEditing_ = ConfigsField::kNone;
        } else {
          switch (configsSelection_) {
            case 0:
              configsEditing_ = ConfigsField::kBacklightTimeout;
              break;
            case 1:
              configsEditing_ = ConfigsField::kCardSensitivity;
              break;
            case 2:  // Back
              menuShowingConfigs_ = false;
              break;
          }
        }
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
