#pragma once

#include <stdbool.h>
#include <stdint.h>

// Wall-clock time in US Central, from NTP over WiFi. The ESP32's own clock
// keeps running between syncs and the SNTP service re-syncs about hourly,
// so this only reads "unknown" from power-up until WiFi first comes up.

// Starts the NTP client (non-blocking). Call once from setup().
void localTimeBegin();

// Current local hour (1-12), minute and AM/PM. False until the clock has
// been set.
bool localTimeNow12h(uint8_t& hour12, uint8_t& minute, bool& pm);
