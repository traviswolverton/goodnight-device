#include "sound.h"

#include <Arduino.h>
#include <string.h>

#include "dimmer.h"
#include "pins.h"

namespace {

// Nothing else here uses LEDC; any free channel would do.
constexpr uint8_t kChannel = 4;
constexpr uint8_t kResolutionBits = 10;
// A passive buzzer is loudest at 50% duty; less duty is quieter.
constexpr uint32_t kMaxDuty = 1u << (kResolutionBits - 1);

struct Note {
  uint16_t hz;  // 0 = a rest
  uint16_t ms;
};

// Short and distinct by pitch direction as well as rhythm, so they can be
// told apart half asleep: rising = good, falling = bad. A passive buzzer is
// loudest around 2-4 kHz, so the "good" tunes sit up there and the
// failure deliberately low and long.
constexpr Note kTick[] = {{4000, 8}};
constexpr Note kArmed[] = {{1760, 120}};
constexpr Note kRunning[] = {{1319, 70}, {0, 40}, {1760, 70}};
constexpr Note kSuccess[] = {{1319, 90}, {0, 30}, {1760, 90}, {0, 30}, {2637, 180}};
constexpr Note kFailure[] = {{440, 260}, {0, 60}, {330, 260}, {0, 60}, {220, 500}};
constexpr Note kCancel[] = {{880, 40}};

// Easter egg: the opening of the Super Mario Bros. overworld theme. One
// "slot" is a sixteenth, 105 ms: a note sounds for 80 ms and leaves 25 ms
// of air so repeated notes stay separate; a rest is whole slots.
constexpr uint16_t kC5 = 523, kD5 = 587, kE5 = 659, kF5 = 698, kG5 = 784, kA5 = 880;
constexpr uint16_t kE4 = 330, kG4 = 392, kA4 = 440, kBb4 = 466, kB4 = 494;
constexpr Note kMario[] = {
    {kE5, 80}, {0, 25}, {kE5, 80}, {0, 130}, {kE5, 80}, {0, 130}, {kC5, 80}, {0, 25},
    {kE5, 80}, {0, 130}, {kG5, 80}, {0, 340}, {kG4, 80}, {0, 340},
    {kC5, 80}, {0, 340}, {kG4, 80}, {0, 340}, {kE4, 80}, {0, 340},
    {kA4, 80}, {0, 130}, {kB4, 80}, {0, 130}, {kBb4, 80}, {0, 25}, {kA4, 80}, {0, 130},
    {kG4, 70}, {0, 25}, {kE5, 70}, {0, 25}, {kG5, 70}, {0, 25}, {kA5, 80}, {0, 130},
    {kF5, 80}, {0, 25}, {kG5, 80}, {0, 130}, {kE5, 80}, {0, 130},
    {kC5, 80}, {0, 25}, {kD5, 80}, {0, 25}, {kB4, 80}, {0, 25},
};

struct Tune {
  const Note* notes;
  uint8_t count;
};

template <size_t N>
constexpr Tune tune(const Note (&notes)[N]) {
  return Tune{notes, static_cast<uint8_t>(N)};
}

Tune tuneFor(Sound sound) {
  switch (sound) {
    case Sound::Tick: return tune(kTick);
    case Sound::Armed: return tune(kArmed);
    case Sound::Running: return tune(kRunning);
    case Sound::Success: return tune(kSuccess);
    case Sound::Failure: return tune(kFailure);
    case Sound::Cancel: return tune(kCancel);
    case Sound::Mario: return tune(kMario);
  }
  return tune(kTick);
}

SoundSettings settings_;
Tune playing_ = {nullptr, 0};
uint8_t index_ = 0;
uint32_t duty_ = 0;
unsigned long noteEndsMs_ = 0;

void silence() {
  ledcWrite(kChannel, 0);
}

void startNote() {
  const Note& note = playing_.notes[index_];
  if (note.hz == 0 || duty_ == 0) {
    silence();
  } else {
    ledcChangeFrequency(kChannel, note.hz, kResolutionBits);
    ledcWrite(kChannel, duty_);
  }
  noteEndsMs_ = millis() + note.ms;
}

// 0-100 -> duty. Squared, so the low end of the scale has usable quiet
// steps instead of jumping straight to loud.
uint32_t dutyFor(uint8_t volume) {
  if (volume == 0) {
    return 0;
  }
  uint32_t duty = kMaxDuty * volume * volume / 10000;
  return duty == 0 ? 1 : duty;
}

}  // namespace

void soundBegin(const SoundSettings& settings) {
  ledcSetup(kChannel, 2000, kResolutionBits);
  ledcAttachPin(kPinBuzzer, kChannel);
  silence();
  soundApply(settings);
}

void soundApply(const SoundSettings& settings) {
  settings_ = settings;
  if (settings_.volume > 100) settings_.volume = 100;
  if (settings_.darkVolume > 100) settings_.darkVolume = 100;
}

const SoundSettings& soundSettings() {
  return settings_;
}

bool soundFromName(const char* name, Sound& out) {
  if (!name) return false;
  if (strcmp(name, "armed") == 0) out = Sound::Armed;
  else if (strcmp(name, "running") == 0) out = Sound::Running;
  else if (strcmp(name, "success") == 0) out = Sound::Success;
  else if (strcmp(name, "failure") == 0) out = Sound::Failure;
  else if (strcmp(name, "cancel") == 0) out = Sound::Cancel;
  else return false;
  return true;
}

void soundPlay(Sound sound, bool urgent) {
  if (!settings_.enabled) {
    return;
  }
  uint8_t volume = (!urgent && dimmerRoomIsDark()) ? settings_.darkVolume : settings_.volume;
  duty_ = dutyFor(volume);
  playing_ = tuneFor(sound);
  index_ = 0;
  startNote();
}

void soundUpdate() {
  if (playing_.notes == nullptr || (long)(millis() - noteEndsMs_) < 0) {
    return;
  }
  if (++index_ >= playing_.count) {
    playing_ = {nullptr, 0};
    silence();
    return;
  }
  startNote();
}
