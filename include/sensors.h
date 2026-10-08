#pragma once

#include <stdbool.h>

// BH1750 light level, DHT11 temperature/humidity and the AM312 PIR. All are
// polled from loop() on their own cadence; readers just take the latest
// values.
//
// A reading is NAN until the first good sample, and goes back to NAN when
// a sensor stops answering for long enough that the last value would be a
// lie -- callers treat NAN as "unknown", never as zero. The PIR has no
// "unknown": it's one wire, so a missing sensor just never reports motion.

void sensorsBegin();
void sensorsUpdate();

// Smoothed lux. The raw reading is averaged over a few seconds so a car's
// headlights across the ceiling don't flick the screen on and off.
float sensorsLux();
float sensorsTemperatureC();
float sensorsHumidity();

bool sensorsLightPresent();

// True while the PIR's output is high (debounced), and for a couple of
// seconds after movement stops: the AM312 holds its output on by itself.
// Always false for the first few seconds after boot, while the sensor
// settles. How long "someone's there" lasts is the server's call.
bool sensorsMotion();
