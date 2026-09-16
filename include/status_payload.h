#pragma once

#include <Arduino.h>

// Fixed capacity so this stays a plain stack-friendly struct rather than
// needing dynamic allocation -- see DESIGN.md's schema note about adapter
// count growing past the initial 2-3. Raise this if it ever does.
constexpr uint8_t kMaxServices = 8;
constexpr uint8_t kNumPixels = 5;

struct ServiceReading {
  String name;
  String status;  // "ok" | "warn" | "down"
  // Precomputed at parse time: `detail` if present, otherwise "<value>
  // <unit>" -- the rest of the firmware only ever needs one ready-to-draw
  // line, not the raw value/unit/detail split from the schema.
  String line;
};

struct StatusPayload {
  String overallLevel = "ok";  // "ok" | "warn" | "down"
  String overallText = "Waiting for data...";
  ServiceReading services[kMaxServices];
  uint8_t serviceCount = 0;
  String pixelHex[kNumPixels];  // "#RRGGBB", or empty = off/unassigned
  bool valid = false;           // true once at least one message has parsed
};

struct CameraAlert {
  String camera;
  String snapshotUrl;
  String text;
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
