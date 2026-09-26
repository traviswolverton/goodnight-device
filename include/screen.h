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
// receipt"), unless alerts are currently muted. Safe to call from the
// MQTT callback -- only records the alert and flips a pending-fetch
// flag, no network/SPI work. See screenCameraFetchIfPending() below.
void screenOnCameraAlert(const CameraAlert& alert);

// Services a pending CAMERA snapshot fetch, if any (set by
// screenOnCameraAlert()). Must be called from loop(), never from the
// MQTT callback -- see DESIGN.md's "Camera snapshot fetch" note. Cheap
// no-op when nothing is pending, so it's safe to call unconditionally
// every loop() iteration, the same as screenUpdate(). Blocks for up to
// ~5s on a real fetch; that gap is a deliberate, documented tradeoff
// (see DESIGN.md), not an oversight.
void screenCameraFetchIfPending();

// Actions menu (DESIGN.md "Actions"). screen.cpp owns the menu and decides
// when to send; main.cpp owns MQTT and supplies the sender. The sender
// returns false when it couldn't send at all (e.g. offline).
using ActionSender = bool (*)(const char* actionId, uint16_t requestId);
void screenSetActionSender(ActionSender sender);

// The server's reply to an action request. Safe to call from the MQTT
// callback -- it only records the result and flags a redraw.
void screenOnActionResult(const ActionResult& result);
