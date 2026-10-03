#include "input.h"

#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include <Wire.h>

#include "pins.h"

namespace {

InputWakeHandler onWake_ = nullptr;

// ---- Event queue ----------------------------------------------------------

constexpr uint8_t kQueueSize = 16;
InputEvent queue_[kQueueSize];
uint8_t queueHead_ = 0;
uint8_t queueCount_ = 0;

void push(InputKind kind, uint8_t button = 0) {
  if (queueCount_ == kQueueSize) {
    return;  // nobody's draining it; dropping the newest is fine
  }
  queue_[(queueHead_ + queueCount_) % kQueueSize] = {kind, button};
  queueCount_++;
}

bool wakeSwallows() {
  return onWake_ != nullptr && onWake_();
}

// ---- MCP23017: buttons 1-8 and the joystick press -------------------------

Adafruit_MCP23X17 mcp_;
bool expanderPresent_ = false;

constexpr uint8_t kNumSwitches = kNumButtons + 1;  // + joystick press
constexpr uint8_t kJoyPressIndex = kNumButtons;
constexpr unsigned long kDebounceMs = 25;
constexpr unsigned long kLongPressMs = 600;
// Belt and braces: the interrupt line tells us when to read, but a slow
// fallback read means a missed edge can never leave a button stuck.
constexpr unsigned long kFallbackPollMs = 200;
// While anything is held or settling the port is read every pass; this
// keeps that from turning into back-to-back I2C traffic.
constexpr unsigned long kMinReadGapMs = 5;

struct Switch {
  bool rawDown = false;
  bool down = false;  // debounced
  unsigned long rawChangedMs = 0;
  unsigned long downSinceMs = 0;
  bool longFired = false;
  bool swallowed = false;
};
Switch switches_[kNumSwitches];
unsigned long lastExpanderReadMs_ = 0;

uint8_t expanderPin(uint8_t index) {
  return index == kJoyPressIndex ? kExpanderJoystickPressPin : kExpanderFirstButtonPin + index;
}

void onSwitchDown(uint8_t index, Switch& s, unsigned long now) {
  s.downSinceMs = now;
  s.longFired = false;
  s.swallowed = wakeSwallows();
  if (!s.swallowed && index == kJoyPressIndex) {
    push(InputKind::JoyPress);
  }
}

void onSwitchUp(uint8_t index, Switch& s) {
  if (!s.swallowed && index != kJoyPressIndex && !s.longFired) {
    push(InputKind::ButtonShort, index + 1);
  }
  s.swallowed = false;
}

void updateSwitches(unsigned long now) {
  if (!expanderPresent_ || now - lastExpanderReadMs_ < kMinReadGapMs) {
    return;
  }
  bool anySettling = false;
  for (const Switch& s : switches_) {
    anySettling |= (s.rawDown != s.down) || s.down;
  }
  bool interrupted = digitalRead(kPinExpanderInt) == LOW;
  if (!interrupted && !anySettling && now - lastExpanderReadMs_ < kFallbackPollMs) {
    return;
  }
  lastExpanderReadMs_ = now;

  // Reading the port also clears the expander's interrupt.
  uint16_t bits = mcp_.readGPIOAB();
  // A failed I2C read comes back as all zeros, which would read as every
  // button and the stick held down at once. No real press looks like that
  // (the pins are pulled up), so drop the read and try again next time.
  // The same goes for a half-failed read: all 8 button pins low at once.
  if ((bits & 0x00FF) == 0) {
    return;
  }
  for (uint8_t i = 0; i < kNumSwitches; i++) {
    Switch& s = switches_[i];
    bool rawDown = !(bits & (1u << expanderPin(i)));  // pulled up; pressed = low
    if (rawDown != s.rawDown) {
      s.rawDown = rawDown;
      s.rawChangedMs = now;
    }
    if (s.rawDown != s.down && now - s.rawChangedMs >= kDebounceMs) {
      s.down = s.rawDown;
      if (s.down) {
        onSwitchDown(i, s, now);
      } else {
        onSwitchUp(i, s);
      }
    }
    if (s.down && i != kJoyPressIndex && !s.swallowed && !s.longFired &&
        now - s.downSinceMs >= kLongPressMs) {
      s.longFired = true;
      push(InputKind::ButtonLong, i + 1);
    }
  }
}

void beginExpander() {
  expanderPresent_ = mcp_.begin_I2C(kI2cAddrExpander, &Wire);
  if (!expanderPresent_) {
    Serial.printf("[input] no MCP23017 at 0x%02X -- buttons disabled\n", kI2cAddrExpander);
    return;
  }
  // Both ports on INTA (mirroring), open-drain so the ESP32's pull-up sets
  // the idle level, active low.
  mcp_.setupInterrupts(true, true, LOW);
  for (uint8_t i = 0; i < kNumSwitches; i++) {
    mcp_.pinMode(expanderPin(i), INPUT_PULLUP);
    mcp_.setupInterruptPin(expanderPin(i), CHANGE);
  }
  pinMode(kPinExpanderInt, INPUT_PULLUP);
  mcp_.readGPIOAB();  // clear anything latched during setup
}

// ---- Joystick axes ---------------------------------------------------------

bool joystickPresent_ = false;
int centreX_ = 0;
int centreY_ = 0;

// 12-bit ADC, centre around 2048. Enter a direction past ~40% of full
// deflection, leave it below ~25%: the gap stops a stick resting near the
// line from chattering. Deliberately well inside the rails, where the
// ESP32's ADC is at its least linear.
constexpr int kEnterThreshold = 800;
constexpr int kExitThreshold = 500;
constexpr unsigned long kJoyPollMs = 20;
constexpr unsigned long kRepeatDelayMs = 450;
constexpr unsigned long kRepeatIntervalMs = 150;

// Flip these if the stick ends up mounted rotated. Most KY-023 boards read
// low X at left and low Y at top with the pins facing down.
constexpr bool kSwapAxes = false;
constexpr bool kInvertX = false;
constexpr bool kInvertY = false;

enum class Dir : uint8_t { None, Up, Down, Left, Right };
Dir dir_ = Dir::None;
bool dirSwallowed_ = false;
unsigned long dirSinceMs_ = 0;
unsigned long lastRepeatMs_ = 0;
unsigned long lastJoyPollMs_ = 0;

int readAverage(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(pin);
  }
  return sum / 4;
}

InputKind kindFor(Dir d) {
  switch (d) {
    case Dir::Up: return InputKind::JoyUp;
    case Dir::Down: return InputKind::JoyDown;
    case Dir::Left: return InputKind::JoyLeft;
    default: return InputKind::JoyRight;
  }
}

Dir readDirection() {
  int dx = readAverage(kPinJoystickX) - centreX_;
  int dy = readAverage(kPinJoystickY) - centreY_;
  if (kSwapAxes) {
    int t = dx;
    dx = dy;
    dy = t;
  }
  if (kInvertX) dx = -dx;
  if (kInvertY) dy = -dy;

  // Stay in the current direction until it drops below the exit threshold.
  switch (dir_) {
    case Dir::Left: if (dx < -kExitThreshold) return dir_; break;
    case Dir::Right: if (dx > kExitThreshold) return dir_; break;
    case Dir::Up: if (dy < -kExitThreshold) return dir_; break;
    case Dir::Down: if (dy > kExitThreshold) return dir_; break;
    case Dir::None: break;
  }
  // Otherwise pick the dominant axis, if it's past the entry threshold.
  if (abs(dx) >= abs(dy)) {
    if (dx < -kEnterThreshold) return Dir::Left;
    if (dx > kEnterThreshold) return Dir::Right;
  } else {
    if (dy < -kEnterThreshold) return Dir::Up;
    if (dy > kEnterThreshold) return Dir::Down;
  }
  return Dir::None;
}

void updateJoystick(unsigned long now) {
  if (!joystickPresent_ || now - lastJoyPollMs_ < kJoyPollMs) {
    return;
  }
  lastJoyPollMs_ = now;

  Dir d = readDirection();
  if (d != dir_) {
    Dir previous = dir_;
    dir_ = d;
    dirSinceMs_ = now;
    lastRepeatMs_ = now;
    if (d == Dir::None) {
      dirSwallowed_ = false;
      return;
    }
    // Sliding from one direction straight into another without passing
    // centre is still the same gesture: stay swallowed if it was.
    if (previous == Dir::None) {
      dirSwallowed_ = wakeSwallows();
    }
    if (!dirSwallowed_) {
      push(kindFor(d));
    }
    return;
  }
  if (d == Dir::None || dirSwallowed_) {
    return;
  }
  if (now - dirSinceMs_ >= kRepeatDelayMs && now - lastRepeatMs_ >= kRepeatIntervalMs) {
    lastRepeatMs_ = now;
    push(kindFor(d));
  }
}

void beginJoystick() {
  analogReadResolution(12);
  // Calibrate centre at boot: nobody's touching it yet, and cheap sticks
  // rarely rest at exactly mid-scale.
  long sx = 0;
  long sy = 0;
  for (int i = 0; i < 32; i++) {
    sx += analogRead(kPinJoystickX);
    sy += analogRead(kPinJoystickY);
    delay(2);
  }
  centreX_ = sx / 32;
  centreY_ = sy / 32;
  // A connected, resting stick sits near mid-scale. A reading pinned to a
  // rail means it's unplugged or miswired; don't turn noise into
  // navigation. (GPIO34/35 have no internal pulls, so a floating pin can
  // read anything -- this catches the common cases, not all of them.)
  joystickPresent_ = centreX_ > 1000 && centreX_ < 3100 && centreY_ > 1000 && centreY_ < 3100;
  Serial.printf("[input] joystick centre x=%d y=%d%s\n", centreX_, centreY_,
                joystickPresent_ ? "" : " -- out of range, joystick disabled");
}

}  // namespace

void inputBegin(InputWakeHandler onWake) {
  onWake_ = onWake;
  beginExpander();
  beginJoystick();
}

void inputUpdate() {
  unsigned long now = millis();
  updateSwitches(now);
  updateJoystick(now);
}

bool inputNextEvent(InputEvent& out) {
  if (queueCount_ == 0) {
    return false;
  }
  out = queue_[queueHead_];
  queueHead_ = (queueHead_ + 1) % kQueueSize;
  queueCount_--;
  return true;
}

uint8_t inputButtonsDown() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < kNumButtons; i++) {
    if (switches_[i].down) {
      mask |= (1u << i);
    }
  }
  return mask;
}

bool inputJoystickPressed() {
  return switches_[kJoyPressIndex].down;
}

bool inputJoystickRaw(int& x, int& y) {
  if (!joystickPresent_) {
    return false;
  }
  x = readAverage(kPinJoystickX);
  y = readAverage(kPinJoystickY);
  return true;
}

const char* inputJoystickDirection() {
  switch (dir_) {
    case Dir::Up: return "up";
    case Dir::Down: return "down";
    case Dir::Left: return "left";
    case Dir::Right: return "right";
    default: return "none";
  }
}

bool inputJoystickPressDown() {
  if (!expanderPresent_) {
    return false;
  }
  uint16_t bits = mcp_.readGPIOAB();
  if ((bits & 0x00FF) == 0) {
    return false;  // see updateSwitches(): a failed read, not eight presses
  }
  return (bits & (1u << kExpanderJoystickPressPin)) == 0;
}

bool inputExpanderPresent() {
  return expanderPresent_;
}

bool inputJoystickPresent() {
  return joystickPresent_;
}
