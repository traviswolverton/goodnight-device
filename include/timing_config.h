#pragma once

// Idle window shared across MENU's return-to-HOME timeout and CAMERA's
// auto-dismiss (which DESIGN.md didn't originally specify a timeout for
// at all -- without one, an alert with nobody there to dismiss it would
// stay on CAMERA forever). No longer covers the backlight -- that timer
// split off into its own runtime-adjustable value in backlight.cpp,
// since it's driven by PIR-or-user-activity rather than just menu idle
// time, and is user-configurable via the Configs menu. See DESIGN.md's
// "Backlight / presence" section.
constexpr unsigned long kIdleTimeoutMs = 18000;

// How often HOME auto-rotates to the next card.
constexpr unsigned long kCardRotationMs = 4500;

// How long manual encoder rotation on HOME pauses auto-rotation for.
constexpr unsigned long kCardRotationPauseMs = 10000;
