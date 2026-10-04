#pragma once

#include <U8g2lib.h>

// The 0.91" SSD1306 OLED: 128x32, monochrome, I2C. Full-frame buffer
// (512 bytes) so each screen is drawn whole and sent once.
//
// Burn-in: an OLED showing the same thing all night wears unevenly, so
// the whole frame drifts by a pixel every few minutes. Draw through
// displayOffsetX()/displayOffsetY() and keep one pixel of margin on the
// right and bottom edges so the drift never clips anything.

constexpr uint8_t kDisplayWidth = 128;
constexpr uint8_t kDisplayHeight = 32;

enum class DisplayLevel : uint8_t { Off, Dim, Full };

// Returns false when the panel doesn't answer on I2C -- the firmware keeps
// running (sensors and buttons still publish), it just has nothing to show.
bool displayBegin();
bool displayPresent();

// Call every loop(). Retries a panel that was missing at boot, every few
// seconds, and does nothing once the panel is running.
void displayRefresh();

U8G2& displayCanvas();
uint8_t displayOffsetX();
uint8_t displayOffsetY();

// Moves the burn-in drift on a step when it's due. Call every loop()
// (before comparing what's on screen), so a screen whose content never
// changes still drifts.
void displayUpdateDrift();

// Clears the buffer. Call before drawing a frame; displaySend() after.
void displayStartFrame();
void displaySend();

void displaySetLevel(DisplayLevel level);
DisplayLevel displayLevel();

// Contrast used at DisplayLevel::Dim, 0-255. Dim also lowers the panel's
// pre-charge and VCOMH voltages, which dims well below what the contrast
// register alone can reach -- at contrast 0 alone an SSD1306 is still
// bright in a dark room.
void displaySetDimContrast(uint8_t contrast);
