#pragma once

// Presence-driven backlight, independent of screen state -- runs the
// same in HOME, MENU, and CAMERA. See DESIGN.md's "Backlight /
// presence" section.
void backlightBegin();

// Call every loop(). Reads the PIR pin directly and drives the
// backlight pin; doesn't touch screen state at all.
void backlightUpdate();
