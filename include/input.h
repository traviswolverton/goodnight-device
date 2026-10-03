#pragma once

#include <stdint.h>

// The 8 buttons and the joystick press (both on the MCP23017), and the
// joystick's two axes (ESP32 ADC). Everything is turned into discrete
// events here; nothing else reads a pin.
//
// Buttons 1-8 report Short (released before the long-press time) or Long
// (fired once while still held). What a button *does* is the server's
// business -- they're published as raw presses. The joystick drives the
// on-device UI: four directions, repeating while held, plus a press.

enum class InputKind : uint8_t {
  ButtonShort,
  ButtonLong,
  JoyUp,
  JoyDown,
  JoyLeft,
  JoyRight,
  JoyPress,
};

struct InputEvent {
  InputKind kind;
  uint8_t button;  // 1-8 for ButtonShort/ButtonLong, 0 otherwise
};

// Called the moment any control is first touched -- a button going down,
// the stick leaving centre, the stick being pressed -- before any event
// for it exists. Return true to swallow that whole gesture: no event is
// produced for it, and a held stick doesn't repeat, until it's released.
// This is how a press that wakes a dark screen avoids also doing
// something the sleeper can't see.
using InputWakeHandler = bool (*)();

void inputBegin(InputWakeHandler onWake);
void inputUpdate();

// Pops the oldest pending event. False when there are none.
bool inputNextEvent(InputEvent& out);

// True while the joystick is pressed in, read straight from the expander
// (no debounce, no events). For checks at power-up, before inputUpdate()
// is running. False when the expander isn't present, and for a read that
// has every pin low at once: that's what a failed I2C read looks like,
// not eight buttons and a stick all held down.
bool inputJoystickPressDown();

// What the controls are doing right now, for diagnostics. Buttons are
// debounced; bit 0 is button 1, bit 7 is button 8.
uint8_t inputButtonsDown();
bool inputJoystickPressed();
// Current raw ADC reading (0-4095). False when the joystick isn't present.
bool inputJoystickRaw(int& x, int& y);
// "none", "up", "down", "left" or "right".
const char* inputJoystickDirection();

bool inputExpanderPresent();
bool inputJoystickPresent();
