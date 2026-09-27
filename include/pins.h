#pragma once

#include <stdint.h>

// Every GPIO this firmware touches, in one place. DESIGN.md's "Pin
// assignments" table is the source of truth for the wiring; keep the two
// in step.
//
// Board: ELEGOO ESP32-WROOM-32 (38-pin DEVKIT V1), same as the desk unit.

// Shared I2C bus: SSD1306 OLED (0x3C), BH1750 (0x23), MCP23017 (0x20).
// The ESP32's default Wire pins, printed D21/D22 on the board.
constexpr uint8_t kPinI2cSda = 21;
constexpr uint8_t kPinI2cScl = 22;
constexpr uint32_t kI2cClockHz = 400000;

constexpr uint8_t kI2cAddrOled = 0x3C;
constexpr uint8_t kI2cAddrLight = 0x23;  // BH1750 ADDR pin low or floating
constexpr uint8_t kI2cAddrExpander = 0x20;  // MCP23017 A0-A2 all low

// MCP23017 INTA. The expander mirrors both ports onto INTA and drives it
// open-drain, active-low, so this pin needs a pull-up (the ESP32's own).
constexpr uint8_t kPinExpanderInt = 27;

// DHT11 (HW-507 board, which carries its own data-line pull-up).
constexpr uint8_t kPinDht = 4;

// KY-023 joystick axes. These have to be ADC1 pins (GPIO32-39): ADC2 is
// unusable while WiFi is on. 34/35 are input-only, which is all an
// analog read needs. Power the joystick from 3.3V, never its "5V" pin.
constexpr uint8_t kPinJoystickX = 34;
constexpr uint8_t kPinJoystickY = 35;

// Expander pins (Adafruit_MCP23X17 numbering: GPA0-7 = 0-7, GPB0-7 = 8-15).
constexpr uint8_t kNumButtons = 8;
constexpr uint8_t kExpanderFirstButtonPin = 0;  // buttons 1-8 on GPA0-GPA7
constexpr uint8_t kExpanderJoystickPressPin = 8;  // joystick SW on GPB0
