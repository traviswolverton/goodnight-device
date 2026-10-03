#pragma once

#include <stddef.h>
#include <stdint.h>

// What the device says about itself, for a diagnostics page on the server:
// which hardware answered, what the controls are doing, WiFi and memory.
// Published to bedside/diagnostics (retained). Normally every 30 s; the
// server asks for a faster rate by publishing to
// bedside/diagnostics/request, so the device isn't chatty when nobody is
// looking at the page. The JSON shape is documented in DESIGN.md
// ("Diagnostics").

// Parses a request payload: {"fast_seconds": N}. Publishes once a second
// (diagnostics and sensors) for N seconds, 1-600; 0 goes back to the
// normal rate at once. Ignores anything it can't parse.
void diagnosticsApplyRequest(const uint8_t* payload, unsigned int length);

// True while a fast period is running.
bool diagnosticsFast();

// True when it's time to publish: first call after boot or reconnect (see
// diagnosticsPublishSoon()), then at the current rate. Calling it again
// right after a true answer gives false.
bool diagnosticsDue();
void diagnosticsPublishSoon();

// Writes the JSON into out (NUL-terminated); returns its length.
size_t diagnosticsBuild(char* out, size_t size);
