#include "status_payload.h"

#include <ArduinoJson.h>

#include "icons.h"

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

// Same contract as copyTruncated, but expands a `:name:` icon code
// (DESIGN.md "Card icons", app/icons.py on the server) into its 3-byte
// UTF-8 glyph as it copies -- this is what makes an icon cost exactly 3
// bytes against destSize regardless of how long its name is, and what
// keeps a code from being split in half if it would straddle the end of
// a full buffer (an icon that wouldn't fit whole is dropped, not
// partially copied). The scan rule matches the server's exactly: a ':'
// followed by a real icon name and another ':' is an icon and scanning
// resumes after its closing ':'; any other ':' is plain text and
// scanning resumes at the very next character (so "12:30:05" stays
// text, and "a:b:wifi:" is "a:b" then the wifi icon). Only used for
// detail lines -- the title row and every other field still go through
// plain copyTruncated() above, which never interprets ':'.
void copyTruncatedWithIcons(char* dest, size_t destSize, const char* src) {
  if (!src) {
    src = "";
  }
  size_t si = 0;
  size_t di = 0;
  while (src[si] != '\0' && di + 1 < destSize) {
    if (src[si] == ':') {
      size_t close = si + 1;
      while (src[close] != '\0' && src[close] != ':') close++;
      if (src[close] == ':') {
        const Icon::Entry* found = Icon::lookupByName(src + si + 1, close - si - 1);
        if (found) {
          if (di + 3 >= destSize) break;  // glyph wouldn't fit -- stop, don't split it
          uint16_t cp = found->codepoint;
          dest[di++] = (char)(0xE0 | ((cp >> 12) & 0x0F));
          dest[di++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          dest[di++] = (char)(0x80 | (cp & 0x3F));
          si = close + 1;
          continue;
        }
      }
    }
    dest[di++] = src[si++];
  }
  dest[di] = '\0';
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

// Populates reading.detailLine(i)/detailLineCount for one service. If the
// schema's optional `lines` array (1-4 strings) is present, uses those
// directly -- pre-formatted exactly as the server wants them shown,
// zero formatting logic here. If absent (an adapter that hasn't been
// given multi-line content, or hasn't been updated at all), falls back
// to folding value/unit/detail into one line, exactly as before this
// field existed. No per-service-name special-casing anywhere -- a
// service gets however many lines its own message actually carries.
// (Per-line capacity IS asymmetric -- kDetailLineLens[i] -- but that's
// handled once inside detailLine()/kDetailLineLens, not here.)
//
// The `lines` array goes through copyTruncatedWithIcons(), not plain
// copyTruncated() -- any of its four entries may carry `:name:` icon
// codes (DESIGN.md "Card icons"), which get expanded to their glyph
// right here, once, rather than every time a line is drawn/measured.
void populateDetailLines(ServiceReading& reading, JsonVariantConst service) {
  JsonVariantConst linesField = service["lines"];
  if (linesField.is<JsonArrayConst>()) {
    JsonArrayConst linesArray = linesField.as<JsonArrayConst>();
    reading.detailLineCount = 0;
    for (JsonVariantConst lineVal : linesArray) {
      if (reading.detailLineCount >= kMaxDetailLines) {
        break;  // schema caps at 4; this is just defense in depth
      }
      copyTruncatedWithIcons(reading.detailLine(reading.detailLineCount),
                             kDetailLineLens[reading.detailLineCount], asCString(lineVal));
      reading.detailLineCount++;
    }
    if (reading.detailLineCount > 0) {
      return;
    }
    // Schema says minItems 1, so an empty array shouldn't happen -- but
    // don't leave a card with zero lines if it somehow does.
  }
  buildServiceLine(reading.detailLine(0), kDetailLineLens[0], service);
  reading.detailLineCount = 1;
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
    const char* title = asCString(service["title"]);
    copyTruncated(reading.title, kTitleLen, (title && title[0] != '\0') ? title : reading.name);
    copyTruncated(reading.status, kStatusLen, asCString(service["status"]) ?: "ok");
    populateDetailLines(reading, service);
    out.serviceCount++;
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
