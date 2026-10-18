# To do

## Screen
The one-line screen (clock, temperature, humidity, light; stick left/right) is built. A flicker
every 10 s turned out to be the display's periodic panel ping (an address-only I2C transaction
to the OLED); it is gone now that the panel is no longer probed while running.

## Diagnostics for the server page
Done on both sides: the firmware (`src/diagnostics.cpp`, `bedside/diagnostics`, `bedside/stick`,
`bedside/diagnostics/request`; contract in DESIGN.md "Diagnostics") and the server's `/diagnostics`
page. Fast mode was checked end to end on the live broker on 2026-10-18. Left:
- Confirm the central broker's ACL lets `bedside-esp32` publish `bedside/stick`: it's the one topic
  not yet seen arriving. Move the stick and press a button while the server listens.
- Ask the server to stop offering button 8 on its Buttons page (the device never publishes it).

## Cleanup
- The first diagnostics message is printed to serial once per boot (`[diag] {...}`); drop it if
  it's no longer useful.
- `main.cpp` prints a `[dim] lux=... screen=... motion=...` line every 5 s, left in from bring-up.
  Keep it while tuning the dimmer, then drop it or make it less frequent.

## Config and security
- MQTT broker comes from `secrets.h` and connects. Check whether the server's `GET /device/config`
  (via `SERVER_URL` and `DEVICE_TOKEN`) is supposed to supply it, and whether the device can reach
  the server from its subnet.

## Hardware
All of it is built, wired and working: the OLED, the BH1750, the eight buttons, the joystick (both
axes inverted in `input.cpp`, as mounted), the DHT11 and the AM312 PIR. What's left:
- Decide what else the PIR is for on the server (night light, occupancy). On the device it only
  counts as activity for the idle screen-off timer.
- Add `idle_off_minutes` (default 15, 0 = never) to the server's device config and settings page;
  the firmware side is done. Brief: [docs/idle-off-server-brief.md](docs/idle-off-server-brief.md).
- Wire the BH1750's `ADDR` pin to a GND screw on the bus board. The docs now assume it is.
