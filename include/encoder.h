#pragma once

#include <Arduino.h>

// Non-blocking rotary encoder + button reader for the HW-040 board
// (silkscreened HW-040, same pinout as KY-040: CLK/DT/SW). Rotation
// decode runs in interrupt handlers -- polling from loop() alone can
// miss fast pulses, and loop() is also busy with MQTT/display work per
// DESIGN.md's non-blocking rule. The board has no onboard pull-ups (see
// DESIGN.md), so begin() sets INPUT_PULLUP on all three pins itself.
void encoderBegin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin);

// Accumulated rotation steps since the last call; resets to 0 each call.
// Positive/negative direction depends on which pin is wired to clkPin vs
// dtPin -- if it reads backwards on the real hardware, swap those two
// arguments to encoderBegin() rather than negating this return value.
int32_t encoderReadDelta();

// True at most once per physical press, debounced. Polled from loop(),
// not interrupt-driven -- a button doesn't need ISR-level timing the way
// the rotation signal does.
bool encoderPressed();
