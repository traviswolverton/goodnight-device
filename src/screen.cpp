// Screen state machine -- HOME / MENU / CAMERA, per DESIGN.md's "Screen
// state machine" section. All timing is millis()-based; nothing in here
// blocks with delay().

#include "screen.h"

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <strings.h>  // strncasecmp, for the manual HTTP header parse below

#include "backlight.h"
#include "icons.h"
#include "settings_store.h"
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
constexpr int32_t kCardStepDivisorDefault = 4;
// Overwritten in screenBegin() with whatever's persisted in NVS (falls
// back to kCardStepDivisorDefault on first boot) -- can't load from NVS
// here, this initializer runs before setup()/NVS is available.
int32_t cardStepDivisor_ = kCardStepDivisorDefault;
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
// Set by enterCamera(), cleared by screenCameraFetchIfPending() the
// moment it starts a fetch attempt -- one-shot per alert, but a fresh
// alert (screenOnCameraAlert() -> enterCamera() again) re-arms it, which
// is how a second alert arriving mid-display cleanly overwrites/retries
// rather than needing separate "already showing" handling.
bool cameraFetchPending_ = false;
// True once fetchAndDecodeSnapshot() has successfully decoded and drawn
// an image for the current alert -- drawCamera() (the placeholder/error
// chrome) is never invoked again after this until the next enterCamera().
bool cameraHasImage_ = false;
// True if the most recent fetch attempt for the current alert failed --
// drives drawCamera()'s "Snapshot unavailable" vs "Loading snapshot..."
// text.
bool cameraFetchFailed_ = false;

// ~64KB, sized for "a typical response is about 13 KB, worst case seen
// ~45 KB" (see DESIGN.md).
//
// This was originally written as a function-static buffer (allocated
// once, reused across every alert -- the same shape as
// status_payload.cpp's static JsonDocument), matching this codebase's
// general preference for avoiding per-message malloc/free churn after
// Bug 2. That doesn't fit here, though, and the build itself proves it:
// this board's actual DRAM (320KB total, per `pio run`'s own board
// report -- not the 520KB some ESP32-WROOM-32 variants have) can't fit
// a permanent extra 64KB BSS reservation on top of TFT_eSPI's own
// buffers, WiFi/MQTT/TLS state, and everything else already static --
// linking fails with "DRAM segment ... overflowed" if you try. So this
// is a scoped heap allocation instead (malloc'd in
// fetchAndDecodeSnapshot(), freed before every return), which is fine
// specifically because camera alerts are rare and one-shot (a 60s
// per-camera cooldown server-side) -- nothing like the every-15-60s
// homelab/status cadence that made per-message churn a real problem the
// first time. See DESIGN.md's "Camera snapshot fetch" note.
constexpr size_t kSnapshotBufferSize = 65536;

// Hard ceiling on the whole fetch (connect + headers + body) -- the
// CAMERA screen must never hang the main loop waiting on a slow/dead
// server. Enforced with an explicit millis() deadline in
// httpGetToBuffer() below, not just a per-call socket timeout, so a
// connection that dribbles in a few bytes at a time can't keep this
// running indefinitely.
constexpr uint32_t kSnapshotFetchTimeoutMs = 5000;

// Floor on free heap before even attempting the fetch, checked with
// ESP.getFreeHeap() at the point of use rather than just assumed safe
// -- covers the kSnapshotBufferSize malloc() itself plus headroom for
// TJpg_Decoder's own working memory and other allocations during the
// fetch. If free heap is already too low, skip the fetch (and the
// malloc) entirely and degrade to "Snapshot unavailable" rather than
// risk an allocation failure mid-decode.
constexpr uint32_t kMinFreeHeapForFetchBytes = kSnapshotBufferSize + 20000;

// Used by the manual HTTP GET below -- see its own comment for why this
// isn't ESP32's HTTPClient class.
constexpr size_t kHttpLineBufSize = 128;
constexpr size_t kHttpMaxHeaderLines = 40;

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

// True once SPIFFS mounted successfully in screenBegin() -- guards every
// drawIcon() call so a mount failure just means icons silently don't draw
// instead of loadFont() reading garbage off an unmounted filesystem.
bool iconFontReady_ = false;

// Draws one glyph from data/icons.vlw centred at (x, y). Caller must
// already have the font loaded (tft.loadFont("icons")) and is
// responsible for unloading it again -- this is the shared primitive
// both a single standalone icon (drawIcon(), below) and a detail row's
// batched icon pass (drawCardLine()) use, so a row with several icons
// pays for one load/unload total instead of one per icon. Restores the
// text datum/colour the caller had set, since drawCardLine's sibling
// drawString() calls right after don't set their own datum.
void drawIconGlyph(uint16_t codepoint, int x, int y, uint16_t color) {
  uint8_t savedDatum = tft.getTextDatum();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  // Every codepoint here is >= 0xE000 (see tools/icons/build_icons.py),
  // always a 3-byte UTF-8 sequence.
  char utf8[4] = {
      (char)(0xE0 | ((codepoint >> 12) & 0x0F)),
      (char)(0x80 | ((codepoint >> 6) & 0x3F)),
      (char)(0x80 | (codepoint & 0x3F)),
      0,
  };
  tft.drawString(utf8, x, y);
  tft.setTextDatum(savedDatum);
}

// Draws a single standalone icon: loads the font, draws it, unloads
// again. Used only for the once-per-status-change title glyph
// (drawTitleIcon()) -- a detail row with icon codes in its text uses
// drawIconGlyph() directly, batched under one load/unload for the whole
// row (see drawCardLine()), since loading per icon on every 120ms
// scroll tick would be a real, measurable cost otherwise.
void drawIcon(uint16_t codepoint, int x, int y, uint16_t color) {
  if (!iconFontReady_) return;

  // loadFont("icons", SPIFFS) looks equivalent but isn't: that overload
  // (Smooth_font.cpp) always forwards flash=false internally, which sets
  // TFT_eSPI's spiffs flag to false and switches drawGlyph() onto its
  // SD-card path (an endWrite()/startWrite() SPI-transaction release
  // around every glyph row, meant to let an SD card use the bus in
  // between). On this display that breaks the addressing window and every
  // glyph silently draws nothing -- confirmed on real hardware: font
  // metrics loaded fine (gCount matched, no error), but not a single
  // pixel changed. The plain string+bool overload with its bool
  // defaulted to true is the one that actually sets spiffs=true and
  // fontFS=SPIFFS together, taking the direct (correct) read path.
  tft.loadFont("icons");
  drawIconGlyph(codepoint, x, y, color);
  tft.unloadFont();
}

// One run of a detail line's text: either a stretch of plain bytes
// (isIcon false) or a single icon glyph (isIcon true) -- see
// splitLineRuns() below. byteStart/byteLen index into the *original*
// already-icon-substituted line buffer, not a copy, so a run only costs
// a copy into a scratch buffer at the point it's actually measured or
// drawn.
struct LineRun {
  bool isIcon;
  uint16_t codepoint;
  uint16_t byteStart;
  uint16_t byteLen;
  int widthPx;  // filled by computeRunWidths(); 0 until then
};

// Generous relative to any realistic template -- the hard ceiling is a
// line's own byte budget (up to kDetailLineLens[3]-1 = 224), and each
// icon costs 3 of those bytes, so this is nowhere near a real limit;
// it's just insurance against a pathological line silently overflowing
// a fixed-size local array. Extra runs beyond this are simply not
// added (splitLineRuns), same truncate-don't-crash rule as everywhere
// else fixed buffers are used in this file.
constexpr int kMaxLineRuns = 48;

// Splits `text` (already icon-substituted, see status_payload.cpp) into
// text/icon runs, in order. A 3-byte UTF-8 sequence decoding into our
// icon codepoint range (Icon::kCodepointBase..+kCount) is an icon run;
// everything else -- including any other multi-byte UTF-8, which
// shouldn't occur since the client's other fonts are ASCII-only anyway
// -- is folded into the surrounding text run untouched.
int splitLineRuns(const char* text, LineRun* runs, int maxRuns) {
  int count = 0;
  size_t len = strlen(text);
  size_t i = 0;
  size_t runStart = 0;
  while (i < len) {
    uint8_t b0 = (uint8_t)text[i];
    uint16_t decoded = 0;
    bool isIconHere = false;
    if (b0 >= 0xE0 && b0 <= 0xEF && i + 2 < len) {
      uint8_t b1 = (uint8_t)text[i + 1];
      uint8_t b2 = (uint8_t)text[i + 2];
      if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
        decoded = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        if (decoded >= Icon::kCodepointBase && decoded < Icon::kCodepointBase + Icon::kCount) {
          isIconHere = true;
        }
      }
    }
    if (isIconHere) {
      if (runStart < i && count < maxRuns) {
        runs[count++] = {false, 0, (uint16_t)runStart, (uint16_t)(i - runStart), 0};
      }
      if (count < maxRuns) {
        runs[count++] = {true, decoded, (uint16_t)i, 3, 0};
      }
      i += 3;
      runStart = i;
      continue;
    }
    i++;
  }
  if (runStart < len && count < maxRuns) {
    runs[count++] = {false, 0, (uint16_t)runStart, (uint16_t)(len - runStart), 0};
  }
  return count;
}

// Fills widthPx for every run -- an icon's own advance width for an
// icon run (independent of whether data/icons.vlw is actually mounted,
// so layout/scroll math stays identical either way and doesn't jitter
// if that ever changes at runtime), or its rendered width in font 4
// for a text run, matching exactly what the server predicts
// (app.aggregator.client_text_width_px) when it decides whether a line
// needs padding.
void computeRunWidths(const char* text, LineRun* runs, int count) {
  char scratch[kDetailLineLens[kMaxDetailLines - 1]];
  for (int i = 0; i < count; i++) {
    if (runs[i].isIcon) {
      runs[i].widthPx = Icon::advanceForCodepoint(runs[i].codepoint);
    } else {
      size_t n = runs[i].byteLen < sizeof(scratch) - 1 ? runs[i].byteLen : sizeof(scratch) - 1;
      memcpy(scratch, text + runs[i].byteStart, n);
      scratch[n] = '\0';
      runs[i].widthPx = tft.textWidth(scratch);
    }
  }
}

int totalRunsWidth(const LineRun* runs, int count) {
  int total = 0;
  for (int i = 0; i < count; i++) total += runs[i].widthPx;
  return total;
}

// Draws just the text runs at the given baseline x, in font 4 (whatever
// the caller already set) -- the smooth icon font must NOT be loaded
// while this runs, since TFT_eSPI's drawString() routes through the
// smooth-font glyph path for *everything*, including plain ASCII, the
// moment one is loaded (confirmed the hard way: with the icon font
// loaded, ordinary text hit its "glyph not found" fallback instead of
// drawing as text at all). See drawCardLine() for why text and icons
// are drawn in two passes instead of interleaved font swaps per run.
void drawRunsText(const char* text, const LineRun* runs, int count, int baseX, int midY) {
  char scratch[kDetailLineLens[kMaxDetailLines - 1]];
  int x = baseX;
  for (int i = 0; i < count; i++) {
    if (!runs[i].isIcon) {
      size_t n = runs[i].byteLen < sizeof(scratch) - 1 ? runs[i].byteLen : sizeof(scratch) - 1;
      memcpy(scratch, text + runs[i].byteStart, n);
      scratch[n] = '\0';
      tft.drawString(scratch, x, midY);
    }
    x += runs[i].widthPx;
  }
}

// Draws just the icon runs at the given baseline x. Caller must already
// have data/icons.vlw loaded and unloads it afterwards -- see
// drawCardLine(), the only caller, for why this is batched one
// load/unload per row rather than per icon.
void drawRunsIcons(const LineRun* runs, int count, int baseX, int midY, uint16_t color) {
  int x = baseX;
  for (int i = 0; i < count; i++) {
    if (runs[i].isIcon) {
      drawIconGlyph(runs[i].codepoint, x + runs[i].widthPx / 2, midY, color);
    }
    x += runs[i].widthPx;
  }
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

  // text may carry `:name:` icon codes already expanded to their glyph
  // by status_payload.cpp -- split into text/icon runs once, and use
  // their combined width for the scroll decision (matching what the
  // server predicted, app.aggregator.client_text_width_px) instead of
  // tft.textWidth(text) directly, which would measure an icon's raw
  // UTF-8 bytes as three made-up font-4 characters.
  LineRun runs[kMaxLineRuns];
  int runCount = splitLineRuns(text, runs, kMaxLineRuns);
  computeRunWidths(text, runs, runCount);
  int textW = totalRunsWidth(runs, runCount);
  int availW = tft.width() - 12 - leftInset;
  bool scrolls = textW > availW;

  if (onlyIfScrolling && !scrolls) return;

  bool hasIcons = false;
  for (int i = 0; i < runCount; i++) {
    if (runs[i].isIcon) {
      hasIcons = true;
      break;
    }
  }

  tft.setViewport(6 + leftInset, lineTop, availW, lineHeight);
  tft.setTextColor(color, TFT_BLACK);

  int cycle = textW + 40;  // 40px gap before the marquee repeats
  int x = scrolls ? -(scrollOffsetPx_[lineIndex] % cycle) : 0;

  // Two passes, not one interleaved pass, and for a reason that bit us
  // once already: TFT_eSPI's drawString() routes through the smooth
  // icon font's glyph lookup for *everything* the moment it's loaded,
  // even plain ASCII -- so text and icons can't share a loadFont() call
  // while text is drawn through the normal font-4 path. Drawing all
  // text first (icon font definitely unloaded), then loading once and
  // drawing all icons, keeps that one load/unload pair per row-draw
  // regardless of how many icons the row has, rather than one pair per
  // icon -- the cost that mattered here, since a scrolling row redraws
  // every 120ms (kScrollTickMs).
  drawRunsText(text, runs, runCount, x, midY);
  if (scrolls) drawRunsText(text, runs, runCount, x + cycle, midY);  // trailing copy, seamless wrap

  if (hasIcons && iconFontReady_) {
    tft.loadFont("icons");
    drawRunsIcons(runs, runCount, x, midY, color);
    if (scrolls) drawRunsIcons(runs, runCount, x + cycle, midY, color);
    tft.unloadFont();
  }

  tft.resetViewport();
}

// Icon indicating a card's status, drawn to the left of the title text --
// the same information already carried by the title's text color, just
// readable at a glance without reading the color. Used to be a plain
// TFT-primitive shape (circle/triangle/square) because our built-in fonts
// (see include/User_Setup.h) are monochrome ASCII/Latin bitmap sets with
// no symbol glyphs; data/icons.vlw (see tools/icons/) adds exactly that,
// so this normally draws a real check/alert/close glyph instead. Falls
// back to the old shapes if the icon font isn't mounted (e.g. firmware
// updated over OTA but data/icons.vlw was never uploadfs'd to match) --
// a status glyph missing entirely would be a silent downgrade nobody
// asked for; the old shapes carried exactly the same information on
// their own for as long as this UI existed without an icon font at all.
// Only called from the full-card redraw path -- the icon never changes
// without a new status message triggering that path anyway, so the
// scroll ticker has no reason to touch it.
void drawTitleIcon(const char* status, int top, int lineHeight) {
  int cx = 6 + kTitleIconInset / 2;
  int cy = top + lineHeight / 2;
  uint16_t color = colorForLevel(status);

  if (!iconFontReady_) {
    if (strcmp(status, "warn") == 0) {
      tft.fillTriangle(cx, cy - 6, cx - 6, cy + 6, cx + 6, cy + 6, color);
    } else if (strcmp(status, "down") == 0) {
      tft.fillRect(cx - 6, cy - 6, 12, 12, color);
    } else {
      tft.fillCircle(cx, cy, 6, color);
    }
    return;
  }

  uint16_t icon = Icon::kCheckCircle;
  if (strcmp(status, "warn") == 0) {
    icon = Icon::kAlert;
  } else if (strcmp(status, "down") == 0) {
    icon = Icon::kCloseCircle;
  }
  drawIcon(icon, cx, cy, color);
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
    drawCardLine(card.detailLine(i), 1 + i, top, lineHeight, TFT_WHITE, false);
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
    drawCardLine(card.detailLine(i), 1 + i, top, lineHeight, TFT_WHITE, true);
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

// Placeholder/error chrome only -- drawn on entry (before a fetch has
// even started) and again if a fetch fails. Never called again once
// fetchAndDecodeSnapshot() has successfully drawn an image; there's no
// cached pixel data to redraw it from, so a later needsRedraw_ while an
// image is showing would just be wrong. Nothing in this codebase
// currently sets needsRedraw_ while state_ == CAMERA other than
// enterCamera() and the fetch-failure path below, so that's not a
// concern in practice, but worth noting for the next person extending
// this.
void drawCamera() {
  tft.setTextFont(2);  // explicit, not assumed -- see drawBanner()
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CAMERA ALERT", tft.width() / 2, 20);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(cameraAlert_.camera, tft.width() / 2, 50);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(cameraAlert_.text, tft.width() / 2, 76);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(cameraFetchFailed_ ? "Snapshot unavailable" : "Loading snapshot...",
                  tft.width() / 2, 140);
  tft.drawString("Press to dismiss", tft.width() / 2, 170);
}

// Caption bar drawn over the bottom of the decoded image -- kept as its
// own small function since it's the only part of the successful-fetch
// path that isn't inside fetchAndDecodeSnapshot() itself (called once
// right after TJpgDec finishes).
void drawCameraCaption() {
  constexpr int kBarHeight = 24;
  int y = tft.height() - kBarHeight;
  tft.setTextFont(2);
  tft.fillRect(0, y, tft.width(), kBarHeight, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(cameraAlert_.text, tft.width() / 2, y + kBarHeight / 2);
}

// TJpg_Decoder's render callback -- called once per decoded tile
// (16x16 MCU blocks, typically) as the JPEG streams off the decoder.
// Pushed straight to the TFT via tft.pushImage() rather than composited
// into a full-frame buffer first: a 320x240 16bpp framebuffer would be
// ~150KB, on top of the 64KB JPEG receive buffer -- not worth it for a
// screen that's redrawn once per alert. Bails out once past the bottom
// of the screen (defensive -- shouldn't happen for a correctly-sized
// snapshot, but costs nothing to guard).
bool cameraTileOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// Minimal hand-rolled HTTP/1.1 GET, deliberately not ESP32's HTTPClient
// class. HTTPClient.h unconditionally pulls in WiFiClientSecure (full
// mbedTLS plus the root cert bundle) whether or not it's ever used for
// https:// -- measured at ~45KB of flash here, more than this board's
// existing OTA partition had headroom for (linking with HTTPClient in
// the mix overflowed it by ~26KB). The snapshot endpoint is plain HTTP,
// no TLS, no auth (per DESIGN.md and the server contract), so none of
// that is needed. This parses exactly what that endpoint returns -- a
// status line and a Content-Length header, then exactly that many body
// bytes -- and nothing more (no chunked encoding, no redirects). Bounds
// the whole operation (connect + headers + body) with an explicit
// millis() deadline, not just per-call socket timeouts, so a
// connection that dribbles in a byte at a time can't run past it.
bool httpGetToBuffer(const char* url, uint8_t* buffer, size_t bufferSize,
                      unsigned long deadlineMs, size_t* outLen) {
  const char* kPrefix = "http://";
  if (strncmp(url, kPrefix, strlen(kPrefix)) != 0) {
    Serial.println("[camera] snapshot URL is not http://");
    return false;
  }
  const char* hostStart = url + strlen(kPrefix);
  const char* pathStart = strchr(hostStart, '/');
  const char* path = pathStart ? pathStart : "/";

  char host[64];
  size_t hostLen = pathStart ? (size_t)(pathStart - hostStart) : strlen(hostStart);
  if (hostLen >= sizeof(host)) hostLen = sizeof(host) - 1;
  memcpy(host, hostStart, hostLen);
  host[hostLen] = '\0';

  uint16_t port = 80;
  char* colon = strchr(host, ':');
  if (colon) {
    port = (uint16_t)atoi(colon + 1);
    *colon = '\0';
  }

  int32_t connectTimeoutMs = (int32_t)(deadlineMs - millis());
  WiFiClient client;
  if (connectTimeoutMs <= 0 || !client.connect(host, port, connectTimeoutMs)) {
    Serial.printf("[camera] connect to %s:%u failed\n", host, port);
    return false;
  }
  // WiFiClient::setTimeout() takes *seconds*, unlike the base Stream
  // class it overrides (confirmed against this core's WiFiClient.cpp,
  // not assumed) -- rounds the remaining budget up so a 1-999ms
  // remainder doesn't round down to 0.
  uint32_t remainingMs = (millis() < deadlineMs) ? (deadlineMs - millis()) : 0;
  client.setTimeout((remainingMs + 999) / 1000);

  client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);

  char line[kHttpLineBufSize];

  // Status line, e.g. "HTTP/1.1 200 OK".
  size_t n = client.readBytesUntil('\n', line, sizeof(line) - 1);
  if (n == 0 || millis() > deadlineMs) {
    Serial.println("[camera] snapshot: no response");
    client.stop();
    return false;
  }
  line[n] = '\0';
  char* statusSpace = strchr(line, ' ');
  int statusCode = statusSpace ? atoi(statusSpace + 1) : -1;
  if (statusCode != 200) {
    Serial.printf("[camera] snapshot GET failed: %s\n", line);
    client.stop();
    return false;
  }

  long contentLength = -1;
  for (size_t i = 0; i < kHttpMaxHeaderLines; i++) {
    n = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (millis() > deadlineMs) {
      Serial.println("[camera] snapshot: header read timed out");
      client.stop();
      return false;
    }
    line[n] = '\0';
    if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';  // readBytesUntil
                                                            // stops at '\n',
                                                            // leaving '\r'
    if (line[0] == '\0') break;  // blank line -- end of headers
    if (strncasecmp(line, "content-length:", 15) == 0) {
      contentLength = atol(line + 15);
    }
  }

  if (contentLength <= 0 || (size_t)contentLength > bufferSize) {
    Serial.printf("[camera] snapshot Content-Length out of range: %ld\n", contentLength);
    client.stop();
    return false;
  }

  size_t received = 0;
  while (received < (size_t)contentLength) {
    if (millis() > deadlineMs) {
      Serial.printf("[camera] snapshot body read timed out at %u/%ld\n", (unsigned)received,
                    contentLength);
      client.stop();
      return false;
    }
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      delay(1);  // brief yield while waiting for more bytes -- this
                 // whole function is already a documented blocking call
                 // bounded by deadlineMs above, not the main loop
      continue;
    }
    int n2 = client.read(buffer + received, (size_t)contentLength - received);
    if (n2 <= 0) break;
    received += (size_t)n2;
  }
  client.stop();

  if (received != (size_t)contentLength) {
    Serial.printf("[camera] snapshot incomplete: got %u of %ld bytes\n", (unsigned)received,
                  contentLength);
    return false;
  }

  *outLen = received;
  return true;
}

// Fetches cameraAlert_.snapshotUrl and decodes it straight to the TFT.
// Blocking, on purpose -- see DESIGN.md's "Camera snapshot fetch" note.
// Must only ever be called from screenCameraFetchIfPending() (i.e. from
// loop()), never from the MQTT callback: this can take up to
// kSnapshotFetchTimeoutMs, which would stall mqttClient.loop() far
// longer than the microseconds-long FastLED.show() call that caused
// Bug 1.
bool fetchAndDecodeSnapshot(const char* url) {
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < kMinFreeHeapForFetchBytes) {
    Serial.printf("[camera] skipping snapshot fetch, low heap: %u\n", freeHeap);
    return false;
  }

  // Scoped allocation, freed before every return below -- see
  // kSnapshotBufferSize's comment for why this isn't a function-static
  // buffer (doesn't fit in this board's DRAM alongside everything else
  // already static).
  uint8_t* buffer = (uint8_t*)malloc(kSnapshotBufferSize);
  if (!buffer) {
    Serial.println("[camera] snapshot buffer malloc failed");
    return false;
  }

  unsigned long deadlineMs = millis() + kSnapshotFetchTimeoutMs;
  size_t received = 0;
  if (!httpGetToBuffer(url, buffer, kSnapshotBufferSize, deadlineMs, &received)) {
    free(buffer);
    return false;
  }

  tft.fillScreen(TFT_BLACK);
  JRESULT decodeResult = TJpgDec.drawJpg(0, 0, buffer, received);
  free(buffer);  // done with the compressed data either way; decoded
                  // tiles already went straight to the TFT via the
                  // callback, nothing else references this buffer
  if (decodeResult != JDR_OK) {
    Serial.println("[camera] JPEG decode failed");
    return false;
  }

  drawCameraCaption();
  return true;
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
  // Re-arms the fetch every time, including when a second alert arrives
  // while CAMERA is already showing -- overwrites cameraAlert_ above and
  // starts a fresh fetch/decode cycle for it, same "newer thing cleanly
  // replaces the old one" shape as gLatestStatus/gHasNewStatus in
  // main.cpp.
  cameraFetchPending_ = true;
  cameraHasImage_ = false;
  cameraFetchFailed_ = false;
  needsRedraw_ = true;
}

}  // namespace

void screenBegin() {
  tft.init();
  tft.setRotation(1);  // portrait panel -> landscape 320x240, per DESIGN.md
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  nextAutoRotateMs_ = millis() + kCardRotationMs;

  // Mounts the same SPIFFS partition data/icons.vlw was uploaded to via
  // `pio run -t uploadfs` (separate from the normal -t upload/OTA flash
  // step, see platformio.ini's partition-table comment). false = don't
  // format on mount failure -- an unmounted/missing filesystem should just
  // mean drawIcon() no-ops, not silently wipe whatever's actually there.
  iconFontReady_ = SPIFFS.begin(false);

  // Card sensitivity persists across a reboot (see settings_store.h for
  // why this one and backlight timeout, but not mute/pin). Backlight
  // timeout's own load happens inside backlightBegin() instead, since
  // that's the module that owns timeoutSeconds_.
  cardStepDivisor_ = settingsLoadCardStepDivisor(kCardStepDivisorDefault);

  // TJpg_Decoder setup -- doesn't change per-alert, so this is done once
  // here rather than inside fetchAndDecodeSnapshot(). setSwapBytes(true)
  // matches TFT_eSPI's expected 16-bit colour byte order (the standard
  // pairing for these two libraries).
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(cameraTileOutput);
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
          // Value's already applied live as it was rotated -- this is
          // the one point where it's also written to NVS (see
          // settings_store.h), so a reboot picks up where the user left
          // it instead of dialing it back in from scratch. Then return
          // to the Configs list, not all the way out to the top-level
          // menu.
          if (configsEditing_ == ConfigsField::kBacklightTimeout) {
            backlightPersistTimeoutSeconds();
          } else if (configsEditing_ == ConfigsField::kCardSensitivity) {
            settingsSaveCardStepDivisor(cardStepDivisor_);
          }
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

void screenCameraFetchIfPending() {
  if (!cameraFetchPending_) return;
  // Cleared immediately, before the blocking call below -- a fresh
  // alert arriving later (screenOnCameraAlert() -> enterCamera()) sets
  // it again, which is the only way a retry/overwrite happens. There's
  // no retry-on-failure of the *same* alert; a failed fetch just shows
  // "Snapshot unavailable" until something newer replaces it or the
  // screen times out.
  cameraFetchPending_ = false;

  bool ok = fetchAndDecodeSnapshot(cameraAlert_.snapshotUrl);
  cameraHasImage_ = ok;
  cameraFetchFailed_ = !ok;
  if (!ok) {
    // Let the normal drawCamera() chrome show the failure text. On
    // success, fetchAndDecodeSnapshot() already drew the final image +
    // caption directly -- nothing else to redraw.
    needsRedraw_ = true;
  }
}
