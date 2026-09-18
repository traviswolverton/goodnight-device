#pragma once

#include <Arduino.h>

// Fixed capacity so this stays a plain stack-friendly struct rather than
// needing dynamic allocation. Raised 8 -> 10 to match the server repo's
// full candidate-adapter list (only 2 of 10 built as of this writing,
// but the cap needed to cover all 10 before that becomes a silently
// missing card rather than a design choice).
constexpr uint8_t kMaxServices = 10;
constexpr uint8_t kNumPixels = 5;

// All fields below are fixed-size char buffers, not String -- see
// DESIGN.md's heap fragmentation note. A message arrives roughly every
// 15-60s, every hour of every day this thing is plugged in; String's
// per-assignment heap churn (this struct alone reassigns ~26 of them per
// message) is exactly the kind of thing that's fine for a five-minute
// test and a real problem for a fifth day of uptime. Truncation on an
// overlong field is a silently-clipped display value, never a crash --
// the trade this makes on purpose.
constexpr size_t kNameLen = 24;
constexpr size_t kTitleLen = 24;
constexpr size_t kStatusLen = 8;
constexpr size_t kLineLen = 100;
constexpr size_t kLevelLen = 8;
constexpr size_t kTextLen = 48;
constexpr size_t kPixelHexLen = 8;  // "#RRGGBB" + '\0'
constexpr size_t kCameraNameLen = 32;
constexpr size_t kUrlLen = 160;
constexpr size_t kAlertTextLen = 64;

// A card's total line count is 1 (name) + detailLineCount. Matches the
// server schema's `lines` array exactly (1-4 entries, optional) --
// see DESIGN.md's "Card templates" note. When a service's `lines` is
// present, detailLines/detailLineCount come from it directly; when
// absent, they fall back to folding value/unit/detail into one line,
// same as before this field existed.
constexpr uint8_t kMaxDetailLines = 4;

struct ServiceReading {
  char name[kNameLen] = "";  // stable identifier -- display code should
                              // prefer `title` below; `name` itself is
                              // never shown
  char title[kTitleLen] = "";  // display label; always populated
                                // (falls back to `name` when the
                                // server payload omits or empties it)
  char status[kStatusLen] = "ok";  // "ok" | "warn" | "down" -- also
                                    // colors the name line, same as
                                    // before this became multi-line
  char detailLines[kMaxDetailLines][kLineLen] = {};
  uint8_t detailLineCount = 0;  // 0..kMaxDetailLines
};

struct StatusPayload {
  char overallLevel[kLevelLen] = "ok";  // "ok" | "warn" | "down"
  char overallText[kTextLen] = "Waiting for data...";
  ServiceReading services[kMaxServices];
  uint8_t serviceCount = 0;
  char pixelHex[kNumPixels][kPixelHexLen] = {};  // "#RRGGBB", or "" = off
  bool valid = false;  // true once at least one message has parsed
};

struct CameraAlert {
  char camera[kCameraNameLen] = "";
  char snapshotUrl[kUrlLen] = "";
  char text[kAlertTextLen] = "";
};

// Parses a homelab/status payload (see DESIGN.md's locked "MQTT JSON
// payload schema") into `out`. Unknown/extra fields are ignored, not an
// error -- the real server payload already carries a field ("error")
// beyond the originally-locked example, and that's expected to keep
// happening as adapters evolve. Returns false only on a JSON parse
// failure or a missing "overall" object.
bool parseStatusPayload(const uint8_t* data, size_t length, StatusPayload& out);

// Parses a homelab/alert/camera payload into `out`.
bool parseCameraAlert(const uint8_t* data, size_t length, CameraAlert& out);
