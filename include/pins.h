#pragma once

#include <stdint.h>

// Every GPIO this firmware touches, in one place. DESIGN.md's "Pin
// assignments" table is the source of truth for the wiring; keep the two
// in step.
//
// Board: ELEGOO ESP32-WROOM-32 (30-pin DEVKIT V1), same as the desk unit.

// Shared I2C bus: SSD1306 OLED (0x3C), BH1750 (0x23), MCP23017 (0x27).
// The ESP32's default Wire pins, printed D21/D22 on the board.
constexpr uint8_t kPinI2cSda = 21;
constexpr uint8_t kPinI2cScl = 22;
constexpr uint32_t kI2cClockHz = 400000;

constexpr uint8_t kI2cAddrOled = 0x3C;
constexpr uint8_t kI2cAddrLight = 0x23;  // BH1750 ADDR pin wired to GND (low or floating also gives 0x23)
constexpr uint8_t kI2cAddrExpander = 0x27;  // MCP23017 A0-A2 all high: how the Waveshare board ships

// MCP23017 INTA. The expander mirrors both ports onto INTA and drives it
// open-drain, active-low, so this pin needs a pull-up (the ESP32's own).
constexpr uint8_t kPinExpanderInt = 27;

// DHT11 (HW-507 board, which carries its own data-line pull-up).
constexpr uint8_t kPinDht = 4;

// AM312 mini PIR motion sensor: one output, 3.3V, high while it sees motion.
// 18 is a plain input-capable GPIO with no boot-strap role. The firmware
// leaves it a plain input: the module's output is weak, and the internal
// pull-down held it at about 1.6V, so motion never registered. Power the
// module from 3.3V, like everything else.
constexpr uint8_t kPinPir = 18;

// KY-023 joystick axes. These have to be ADC1 pins (GPIO32-39): ADC2 is
// unusable while WiFi is on. 34/35 are input-only, which is all an
// analog read needs. Power the joystick from 3.3V, never its "5V" pin.
constexpr uint8_t kPinJoystickX = 34;
constexpr uint8_t kPinJoystickY = 35;

// Expander pins (Adafruit_MCP23X17 numbering: GPA0-7 = 0-7, GPB0-7 = 8-15).
constexpr uint8_t kNumButtons = 8;
constexpr uint8_t kExpanderFirstButtonPin = 0;  // buttons 1-8 on GPA0-GPA7
constexpr uint8_t kExpanderJoystickPressPin = 8;  // joystick SW on GPB0

// Passive buzzer module (sound.h), driven by LEDC PWM: I/O to this pin, VCC
// to 3V3, GND to GND. 25 is a plain output-capable GPIO with no boot-strap role.
constexpr uint8_t kPinBuzzer = 25;
