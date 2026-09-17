#include "status_payload.h"

#include <ArduinoJson.h>

namespace {

// Copies src into a fixed dest buffer, truncating rather than
// overflowing. src may be null (treated as empty). No heap allocation --
// see DESIGN.md's heap fragmentation note for why that matters here.
void copyTruncated(char* dest, size_t destSize, const char* src) {
  if (!src) {
    src = "";
  }
  size_t len = strlen(src);
  if (len >= destSize) {
    len = destSize - 1;
  }
  memcpy(dest, src, len);
  dest[len] = '\0';
}

const char* asCString(JsonVariantConst v) {
  return v.is<const char*>() ? v.as<const char*>() : nullptr;
}

// Folds services[].value/unit/detail into the one display line the rest
// of the firmware actually needs, written directly into a fixed buffer
// (snprintf, not String concatenation).
void buildServiceLine(char* dest, size_t destSize, JsonVariantConst service) {
  const char* detail = asCString(service["detail"]);
  if (detail && detail[0] != '\0') {
    copyTruncated(dest, destSize, detail);
    return;
  }

  if (service["value"].isNull()) {
    dest[0] = '\0';
    return;
  }
  double value = service["value"] | 0.0;
  const char* unit = asCString(service["unit"]);
  if (!unit) unit = "";

  // The real server writes every number as a float (Python's json
  // module does this), so "1784409.0" needs to render as "1784409", not
  // with a needless trailing ".0" -- only show decimals when the value
  // actually has a fractional part.
  if (value == (long)value) {
    snprintf(dest, destSize, "%ld%s%s", (long)value, unit[0] ? " " : "", unit);
  } else {
    snprintf(dest, destSize, "%.1f%s%s", value, unit[0] ? " " : "", unit);
  }
}

// TEMPORARY: overrides uptime_kuma's real (single) detail line with
// placeholder multi-line content, so the generalized multi-line card
// renderer (screen.cpp) can be exercised live on real hardware before
// the server can actually send more than one fact per service. Remove
// this once the schema grows a real `lines` array (see DESIGN.md's
// "Card templates" note) and screen.cpp will keep working unchanged --
// it only ever looks at detailLines/detailLineCount, never a name.
void injectPreviewLines(ServiceReading& reading) {
  if (strcmp(reading.name, "uptime_kuma") != 0) {
    return;
  }
  static const char* kPreviewLines[3] = {
      "71 / 71 monitors up",
      "All monitors healthy, last checked 12s ago",
      "Longest-running monitor: Plex, 41 days",
  };
  reading.detailLineCount = 3;
  for (uint8_t i = 0; i < 3; i++) {
    copyTruncated(reading.detailLines[i], kLineLen, kPreviewLines[i]);
  }
}

}  // namespace

bool parseStatusPayload(const uint8_t* data, size_t length, StatusPayload& out) {
  // A function-static JsonDocument, reused (via clear()) rather than a
  // fresh one constructed and destroyed on every call. See DESIGN.md's
  // heap fragmentation note: a message arrives every 15-60s, forever --
  // allocating and freeing this document's whole memory pool on every
  // single one of those, indefinitely, is a real long-uptime cost that a
  // short test run won't show.
  static JsonDocument doc;
  doc.clear();

  DeserializationError err = deserializeJson(doc, data, length);
  if (err) {
    Serial.printf("[status] JSON parse failed: %s\n", err.c_str());
    return false;
  }

  JsonVariantConst overall = doc["overall"];
  if (overall.isNull()) {
    Serial.println("[status] missing \"overall\" -- ignoring payload");
    return false;
  }
  copyTruncated(out.overallLevel, kLevelLen, asCString(overall["level"]) ?: "ok");
  copyTruncated(out.overallText, kTextLen, asCString(overall["text"]));

  JsonArrayConst services = doc["services"];
  out.serviceCount = 0;
  for (JsonVariantConst service : services) {
    if (out.serviceCount >= kMaxServices) {
      break;  // more adapters than we have card slots for -- drop the rest
    }
    ServiceReading& reading = out.services[out.serviceCount];
    copyTruncated(reading.name, kNameLen, asCString(service["name"]) ?: "?");
    copyTruncated(reading.status, kStatusLen, asCString(service["status"]) ?: "ok");
    buildServiceLine(reading.detailLines[0], kLineLen, service);
    reading.detailLineCount = 1;
    injectPreviewLines(reading);  // TEMPORARY, see comment above
    out.serviceCount++;
  }

  JsonVariantConst pixels = doc["pixels"];
  for (uint8_t i = 0; i < kNumPixels; i++) {
    char key[2] = {char('0' + i), '\0'};
    copyTruncated(out.pixelHex[i], kPixelHexLen, asCString(pixels[key]));
  }

  out.valid = true;
  return true;
}

bool parseCameraAlert(const uint8_t* data, size_t length, CameraAlert& out) {
  static JsonDocument doc;  // reused for the same reason as above
  doc.clear();

  DeserializationError err = deserializeJson(doc, data, length);
  if (err) {
    Serial.printf("[camera] JSON parse failed: %s\n", err.c_str());
    return false;
  }
  copyTruncated(out.camera, kCameraNameLen, asCString(doc["camera"]));
  copyTruncated(out.snapshotUrl, kUrlLen, asCString(doc["snapshot_url"]));
  copyTruncated(out.text, kAlertTextLen, asCString(doc["text"]));
  return true;
}
