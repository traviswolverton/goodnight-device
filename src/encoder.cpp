#include "encoder.h"

namespace {

uint8_t clkPin_ = 0;
uint8_t dtPin_ = 0;
uint8_t swPin_ = 0;

volatile uint8_t quadState_ = 0;
volatile int32_t accum_ = 0;

// Full-step quadrature decode table, indexed by (previous 2 bits << 2 |
// current 2 bits) of (CLK, DT). Yields +-1 only at an actual detent
// transition and 0 for the intermediate, contact-bounce-prone states in
// between -- this is what makes it robust against a noisy encoder
// without needing a software debounce timer on the rotation signal
// itself (unlike the button, which does use one below).
const int8_t kQuadTable[16] = {
    0, -1, 1,  0,
    1, 0,  0,  -1,
    -1, 0, 0,  1,
    0,  1, -1, 0,
};

void IRAM_ATTR onEncoderEdge() {
  uint8_t clk = digitalRead(clkPin_);
  uint8_t dt = digitalRead(dtPin_);
  quadState_ = ((quadState_ << 2) | (clk << 1) | dt) & 0x0F;
  accum_ += kQuadTable[quadState_];
}

bool lastSwState_ = HIGH;
unsigned long lastSwChangeMs_ = 0;
constexpr unsigned long kDebounceMs = 30;

}  // namespace

void encoderBegin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin) {
  clkPin_ = clkPin;
  dtPin_ = dtPin;
  swPin_ = swPin;

  pinMode(clkPin_, INPUT_PULLUP);
  pinMode(dtPin_, INPUT_PULLUP);
  pinMode(swPin_, INPUT_PULLUP);

  lastSwState_ = digitalRead(swPin_);

  attachInterrupt(digitalPinToInterrupt(clkPin_), onEncoderEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(dtPin_), onEncoderEdge, CHANGE);
}

int32_t encoderReadDelta() {
  // Read-then-clear needs to be atomic with respect to the ISR, or a
  // rotation edge landing between the two steps loses a count.
  noInterrupts();
  int32_t delta = accum_;
  accum_ = 0;
  interrupts();
  return delta;
}

bool encoderPressed() {
  bool current = digitalRead(swPin_);
  unsigned long now = millis();
  bool firedPress = false;
  if (current != lastSwState_ && (now - lastSwChangeMs_) > kDebounceMs) {
    lastSwChangeMs_ = now;
    lastSwState_ = current;
    // Active-low: the button shorts SW to GND on press, and
    // INPUT_PULLUP idles it HIGH otherwise.
    if (current == LOW) {
      firedPress = true;
    }
  }
  return firedPress;
}
