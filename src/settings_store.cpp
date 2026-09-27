#include "settings_store.h"

#include <Preferences.h>

namespace {

// One shared NVS namespace for every persisted Configs value, rather
// than a namespace per field -- this is a handful of small settings,
// not a growing schema. (15-char limit on namespace/key names is an NVS
// constraint, not a style choice, hence the abbreviations.)
constexpr char kNamespace[] = "dashcfg";
constexpr char kKeyBacklightTimeout[] = "bl_secs";
constexpr char kKeyCardStepDivisor[] = "card_div";
constexpr char kKeyCardRotationSecs[] = "card_secs";

}  // namespace

// Preferences::begin(..., /*readOnly=*/true) on a namespace that hasn't
// been written to yet simply fails to open -- it doesn't crash or throw,
// and every getter on an unopened Preferences instance falls through to
// the caller-supplied default. So first boot (nothing saved) and a normal
// "value not present" both take the same path here with no special-casing.

uint16_t settingsLoadBacklightTimeoutSeconds(uint16_t defaultValue) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);
  uint16_t value = prefs.getUShort(kKeyBacklightTimeout, defaultValue);
  prefs.end();
  return value;
}

void settingsSaveBacklightTimeoutSeconds(uint16_t seconds) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putUShort(kKeyBacklightTimeout, seconds);
  prefs.end();
}

int32_t settingsLoadCardStepDivisor(int32_t defaultValue) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);
  int32_t value = prefs.getInt(kKeyCardStepDivisor, defaultValue);
  prefs.end();
  return value;
}

void settingsSaveCardStepDivisor(int32_t divisor) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putInt(kKeyCardStepDivisor, divisor);
  prefs.end();
}

uint16_t settingsLoadCardRotationSeconds(uint16_t defaultValue) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);
  uint16_t value = prefs.getUShort(kKeyCardRotationSecs, defaultValue);
  prefs.end();
  return value;
}

void settingsSaveCardRotationSeconds(uint16_t seconds) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putUShort(kKeyCardRotationSecs, seconds);
  prefs.end();
}
