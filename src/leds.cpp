#include "leds.h"

#include <FastLED.h>

namespace {

// GPIO13 (board silkscreen D13) -- see DESIGN.md's pin table. Briefly
// tested on GPIO14 to rule out a GPIO13-specific fault (damaged pin,
// pin-mux quirk); the bug reproduced identically there too, so it
// follows the wire/strip rather than the pin -- back to the documented
// D13. FastLED's addLeds<>() needs the data pin as a compile-time
// template argument, so it's a constant here rather than a parameter
// to ledsBegin().
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
  // TEMPORARY diagnostic for the pixels-render-white investigation --
  // proves whether the ESP32 computed the right CRGB (a software bug)
  // or computed it correctly and the strip is still showing white
  // anyway (a wiring/signal-integrity issue downstream of this code).
  // Remove once that's settled one way or the other.
  for (uint8_t i = 0; i < kNumPixels; i++) {
    Serial.printf("[leds] pixel %u hex=\"%s\" -> r=%u g=%u b=%u\n", i, payload.pixelHex[i],
                  pixels_[i].r, pixels_[i].g, pixels_[i].b);
  }
  FastLED.show();
}
