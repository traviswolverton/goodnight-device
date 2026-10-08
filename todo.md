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

## Config and security
- Flash once over USB with the `secrets.h` that `scripts/gen_secrets.py` writes, so the device has
  the real OTA password and device token and the saved WiFi no longer matters.
- MQTT broker is `192.0.2.10:1883` from `secrets.h`. It connects now; check whether the
  server's `GET /device/config` (via `SERVER_URL` and `DEVICE_TOKEN`) is supposed to supply it,
  and whether the device on `192.0.2.x` can reach the server.

## Hardware
- Verify buttons 3, 4, 5 and 8 and the joystick directions in the log (the stick now logs
  `[input] stick <direction>`); 1, 2, 6 and 7 were seen registering.
- Check the DHT11 shows temperature and humidity on the OLED.
- Wire the AM312 PIR (`OUT` to GPIO18 / `D18`, `VCC` to 3V3, `GND`) and check it: wave a hand in
  front of it 10 s after boot and watch the log for `[sensors] motion`, then `[sensors] still`.
  Also confirm `"motion"` shows up in `bedside/sensors`.
- Decide what the PIR is for on the server (night light, occupancy). Nothing on the device uses
  it yet: it doesn't wake the screen.
- Optional: BH1750 `ADDR` to GND to force 0x23 (works left open now).
- Optional: sound sensor (HW-485) planned for GPIO32 (`AO`, ADC1); not wired or in the pinout.
- Optional: lower-risk I2C bus hardening (4.7 kΩ pull-ups) if dropouts ever return; the cause
  last time was the ground bus.
