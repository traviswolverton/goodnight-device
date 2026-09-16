#pragma once

// Idle window shared across MENU's return-to-HOME timeout and the
// backlight's off-delay after the PIR stops seeing motion -- one
// constant, not two separately tuned ones (see DESIGN.md's "Backlight /
// presence" section). Also used for CAMERA's auto-dismiss, which
// DESIGN.md didn't originally specify a timeout for at all -- without
// one, an alert with nobody there to dismiss it would stay on CAMERA
// forever.
constexpr unsigned long kIdleTimeoutMs = 18000;

// How often HOME auto-rotates to the next card.
constexpr unsigned long kCardRotationMs = 4500;

// How long manual encoder rotation on HOME pauses auto-rotation for.
constexpr unsigned long kCardRotationPauseMs = 10000;
