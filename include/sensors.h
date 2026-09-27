#pragma once

#include <stdbool.h>

// BH1750 light level and DHT11 temperature/humidity. Both are polled from
// loop() on their own cadence; readers just take the latest values.
//
// A reading is NAN until the first good sample, and goes back to NAN when
// a sensor stops answering for long enough that the last value would be a
// lie -- callers treat NAN as "unknown", never as zero.

void sensorsBegin();
void sensorsUpdate();

// Smoothed lux. The raw reading is averaged over a few seconds so a car's
// headlights across the ceiling don't flick the screen on and off.
float sensorsLux();
float sensorsTemperatureC();
float sensorsHumidity();

bool sensorsLightPresent();
