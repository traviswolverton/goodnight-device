#include "settings_store.h"

#include <Preferences.h>

namespace {

// 15-char limit on namespace/key names is an NVS constraint, hence the
// abbreviations.
constexpr char kNamespace[] = "bedcfg";
constexpr char kKeyBrightLux[] = "bright_lux";
constexpr char kKeyDarkLux[] = "dark_lux";
constexpr char kKeyDimContrast[] = "dim_contrast";
constexpr char kKeyWakeSeconds[] = "wake_secs";
constexpr char kKeySoundOn[] = "snd_on";
constexpr char kKeySoundVolume[] = "snd_vol";
constexpr char kKeySoundDark[] = "snd_dark";

}  // namespace

// Preferences::begin(..., /*readOnly=*/true) on a namespace that hasn't
// been written yet simply fails to open, and every getter then returns
// the caller's default -- so "never saved" needs no special case.

DimmerSettings settingsLoadDimmer(const DimmerSettings& defaults) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);
  DimmerSettings s;
  s.brightLux = prefs.getFloat(kKeyBrightLux, defaults.brightLux);
  s.darkLux = prefs.getFloat(kKeyDarkLux, defaults.darkLux);
  s.dimContrast = prefs.getUChar(kKeyDimContrast, defaults.dimContrast);
  s.wakeSeconds = prefs.getUShort(kKeyWakeSeconds, defaults.wakeSeconds);
  prefs.end();
  return s;
}

void settingsSaveDimmer(const DimmerSettings& settings) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  if (prefs.isKey(kKeyBrightLux) && prefs.getFloat(kKeyBrightLux) == settings.brightLux &&
      prefs.getFloat(kKeyDarkLux) == settings.darkLux &&
      prefs.getUChar(kKeyDimContrast) == settings.dimContrast &&
      prefs.getUShort(kKeyWakeSeconds) == settings.wakeSeconds) {
    prefs.end();
    return;
  }
  prefs.putFloat(kKeyBrightLux, settings.brightLux);
  prefs.putFloat(kKeyDarkLux, settings.darkLux);
  prefs.putUChar(kKeyDimContrast, settings.dimContrast);
  prefs.putUShort(kKeyWakeSeconds, settings.wakeSeconds);
  prefs.end();
}

SoundSettings settingsLoadSound(const SoundSettings& defaults) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);
  SoundSettings s;
  s.enabled = prefs.getBool(kKeySoundOn, defaults.enabled);
  s.volume = prefs.getUChar(kKeySoundVolume, defaults.volume);
  s.darkVolume = prefs.getUChar(kKeySoundDark, defaults.darkVolume);
  prefs.end();
  return s;
}

void settingsSaveSound(const SoundSettings& settings) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  if (prefs.isKey(kKeySoundOn) && prefs.getBool(kKeySoundOn) == settings.enabled &&
      prefs.getUChar(kKeySoundVolume) == settings.volume &&
      prefs.getUChar(kKeySoundDark) == settings.darkVolume) {
    prefs.end();
    return;
  }
  prefs.putBool(kKeySoundOn, settings.enabled);
  prefs.putUChar(kKeySoundVolume, settings.volume);
  prefs.putUChar(kKeySoundDark, settings.darkVolume);
  prefs.end();
}
