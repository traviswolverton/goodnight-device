#include "status_payload.h"

#include <ArduinoJson.h>

namespace {

// Folds services[].value/unit/detail into the one display line the rest
// of the firmware actually needs.
String buildServiceLine(JsonVariantConst service) {
  const char* detail = service["detail"] | (const char*)nullptr;
  if (detail && detail[0] != '\0') {
    return String(detail);
  }

  String line;
  if (!service["value"].isNull()) {
    double value = service["value"] | 0.0;
    // The real server writes every number as a float (Python's json
    // module does this), so "1784409.0" needs to render as "1784409",
    // not with a needless trailing ".0" -- only show decimals when the
    // value actually has a fractional part.
    if (value == (long)value) {
      line = String((long)value);
    } else {
      line = String(value, 1);
    }
  }
  const char* unit = service["unit"] | (const char*)nullptr;
  if (unit && unit[0] != '\0') {
    if (line.length()) line += ' ';
    line += unit;
  }
  return line;
}

}  // namespace

bool parseStatusPayload(const uint8_t* data, size_t length, StatusPayload& out) {
  JsonDocument doc;
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
  out.overallLevel = overall["level"] | "ok";
  out.overallText = overall["text"] | "";

  JsonArrayConst services = doc["services"];
  out.serviceCount = 0;
  for (JsonVariantConst service : services) {
    if (out.serviceCount >= kMaxServices) {
      break;  // more adapters than we have card slots for -- drop the rest
    }
    ServiceReading& reading = out.services[out.serviceCount];
    reading.name = service["name"] | "?";
    reading.status = service["status"] | "ok";
    reading.line = buildServiceLine(service);
    out.serviceCount++;
  }

  JsonVariantConst pixels = doc["pixels"];
  for (uint8_t i = 0; i < kNumPixels; i++) {
    char key[2] = {char('0' + i), '\0'};
    JsonVariantConst px = pixels[key];
    out.pixelHex[i] = (px.is<const char*>()) ? String((const char*)px) : String();
  }

  out.valid = true;
  return true;
}

bool parseCameraAlert(const uint8_t* data, size_t length, CameraAlert& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, length);
  if (err) {
    Serial.printf("[camera] JSON parse failed: %s\n", err.c_str());
    return false;
  }
  out.camera = doc["camera"] | "";
  out.snapshotUrl = doc["snapshot_url"] | "";
  out.text = doc["text"] | "";
  return true;
}
