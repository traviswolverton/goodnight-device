# To do

## Screen
The one-line screen (clock, temperature, humidity, light; stick left/right) is built. A flicker
every 10 s turned out to be the display's periodic panel ping (an address-only I2C transaction
to the OLED); it is gone now that the panel is no longer probed while running.

## Diagnostics for the server page
Firmware side is done (`src/diagnostics.cpp`, `bedside/diagnostics`, `bedside/stick`,
`bedside/diagnostics/request`; contract in DESIGN.md "Diagnostics"). Left:
- Give the server repo the brief (topics and JSON shapes are in DESIGN.md).
- Confirm the broker ACL for `bedside-esp32` covers the new topics: publish `bedside/diagnostics`
  and `bedside/stick`, subscribe `bedside/diagnostics/request`.
- Check the whole path with the server: ask for `{"fast_seconds": 30}` and watch it publish 1/s.

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
- Optional: BH1750 `ADDR` to GND to force 0x23 (works left open now).
- Optional: sound sensor (HW-485) planned for GPIO32 (`AO`, ADC1); not wired or in the pinout.
- Optional: 4.7 kΩ pull-ups if I2C errors ever return with every device grounded and on 3V3. The
  BH1750 board already carries pull-ups. The two bus faults so far were a BH1750 with no ground
  and the expander fed 5V (see [docs/troubleshooting.md](docs/troubleshooting.md)).
