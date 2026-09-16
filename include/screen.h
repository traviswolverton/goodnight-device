#pragma once

#include "status_payload.h"

void screenBegin();

// Called every loop(). `isNewStatus` should be true only on the loop
// iteration where a homelab/status message actually just parsed
// successfully -- HOME only redraws when its content actually changed,
// not every loop.
void screenUpdate(const StatusPayload& status, bool isNewStatus);

void screenOnEncoderDelta(int32_t delta);
void screenOnEncoderPress();

// Switches straight to CAMERA (per DESIGN.md: "auto-triggered ... on
// receipt"), unless alerts are currently muted.
void screenOnCameraAlert(const CameraAlert& alert);
