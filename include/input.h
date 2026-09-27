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

bool inputExpanderPresent();
bool inputJoystickPresent();
