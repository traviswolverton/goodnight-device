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
