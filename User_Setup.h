// TFT_eSPI configuration for the Desk Dashboard's Waveshare 2" ST7789VW
// display (240x320, no MISO/SDO pin, backlight wired directly to 3.3V).
// Pin assignments verified against real hardware — see the "Pin
// assignments" section in DESIGN.md before changing anything here.
//
// Usage: TFT_eSPI reads its config from whichever User_Setup.h its build
// is pointed at. Either:
//   (a) replace <TFT_eSPI library>/User_Setup.h with this file, or
//   (b) in a PlatformIO project, keep this file under include/ and add to
//       platformio.ini:
//         build_flags = -D USER_SETUP_LOADED=1 -include include/User_Setup.h
// Don't edit User_Setup_Select.h to point elsewhere AND replace the stock
// file — pick one method, not both.

#define USER_SETUP_INFO "DeskDashboard"

// --- Driver ---
#define ST7789_DRIVER

// --- Panel resolution (native portrait; runtime tft.setRotation(1) does
//     the portrait->landscape flip called for in DESIGN.md, not this file) ---
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Common requirement for ST7789 panels — try commenting this out first
// if colors look inverted/wrong on first boot.
#define TFT_INVERSION_ON

// --- Pins (VSPI hardware SPI — matches TFT_eSPI's ESP32 default bus) ---
#define TFT_MOSI 23   // board silkscreen: D23  (display pin: DIN)
#define TFT_SCLK 18   // board silkscreen: D18  (display pin: CLK)
#define TFT_CS    5   // board silkscreen: D5   (display pin: CS)
#define TFT_DC    4   // board silkscreen: D4   (display pin: DC)
#define TFT_RST  16   // board silkscreen: RX2 — NOT D16, see DESIGN.md note
// No TFT_MISO — this display has no MISO/SDO pin (write-only).
// No TFT_BL — backlight is wired straight to 3.3V, not GPIO-driven. If a
// dimmable backlight is added later, define TFT_BL + TFT_BACKLIGHT_ON here.

// --- Fonts (trim this list later if flash size becomes a concern) ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// --- SPI clock ---
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
