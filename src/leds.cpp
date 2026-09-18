#include "leds.h"

#include <FastLED.h>

namespace {

// GPIO13 (board silkscreen D13) -- see DESIGN.md's pin table. FastLED's
// addLeds<>() needs the data pin as a compile-time template argument, so
// it's a constant here rather than a parameter to ledsBegin().
constexpr uint8_t kDataPin = 13;

CRGB pixels_[kNumPixels];

// "#RRGGBB" -> CRGB. Anything else (empty, malformed) -> black/off, per
// DESIGN.md: null in the pixels map means off/unassigned, not "no
// change".
CRGB hexToColor(const String& hex) {
  if (hex.length() != 7 || hex[0] != '#') {
    return CRGB::Black;
  }
  long value = strtol(hex.c_str() + 1, nullptr, 16);
  return CRGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

}  // namespace

void ledsBegin() {
  FastLED.addLeds<WS2812B, kDataPin, GRB>(pixels_, kNumPixels);
  // Full 255 is uncomfortably bright for a strip sitting this close on a
  // desk -- a modest default, not a DESIGN.md-specified value.
  FastLED.setBrightness(80);
  FastLED.clear();
  FastLED.show();
}

void ledsUpdate(const StatusPayload& payload) {
  for (uint8_t i = 0; i < kNumPixels; i++) {
    pixels_[i] = hexToColor(payload.pixelHex[i]);
  }
  FastLED.show();
}
