#pragma once

#include "status_payload.h"

void ledsBegin();

// Sets all pixels from payload.pixelHex ("#RRGGBB", or empty = off) and
// pushes them out. Call this only when a new homelab/status message has
// actually arrived, not every loop() -- FastLED.show() briefly disables
// interrupts for its timing-sensitive bitstream, and there's no reason
// to pay that cost when nothing changed.
void ledsUpdate(const StatusPayload& payload);
