# To do

## BLE WiFi provisioning: test it (on hold)
The code is built and flashed (`src/provisioning.cpp`, `src/wifi_config.cpp`) but has never
run end to end. Setup mode itself was seen starting (`[setup] advertising as "Bedside-XXXX"`),
so the part left is pairing and writing credentials from the iPhone.

1. Hold the **joystick centre press** while the board resets (or power it on holding it) and
   keep holding about 3 s, until the OLED shows `Setup: Bedside-XXXX` and a 6-digit code.
   Setup mode times out after 5 min with no phone activity.
2. nRF Connect on the iPhone: scan, connect to `Bedside-XXXX`, type the OLED's code to pair.
3. Characteristics (`6e5b000N-2f5c-4c0e-9b7d-5a8f0c3e1d00`; N=1 is the service):
   - `…0002` write the network name (text)
   - `…0003` write the password (text)
   - `…0004` write any byte (e.g. `01`) to apply
   - `…0005` subscribe to notifications: `connecting`, then `connected <ip>` or `failed: <reason>`
4. Check: wrong password reports failure and keeps the old network; a good one is saved to
   NVS and survives a power cycle; BLE shuts down afterwards (log shows `BLE off; free heap`).
5. Then remove `WIFI_SSID` / `WIFI_PASSWORD` from `secrets.h` and confirm the device still joins
   from NVS (saved network wins over `secrets.h`).

Known: holding the stick pressed at power-up can push it off centre, so the joystick may read
"out of range, joystick disabled" for that boot only.

## Screen
The one-line screen (clock, temperature, humidity, light; stick left/right) is built and was
flashed over the air. Open: a flicker about every 10 s was reported and not yet explained. Suspects:
the brightness flipping between Full and Dim near the 20 lx threshold, or MQTT dropping and
reconnecting (the `M` mark redraws). Needs the serial log (USB) or a broker login that can read
`bedside/sensors` and `bedside/availability`.

## Diagnostics for the server page
Firmware side is done (`src/diagnostics.cpp`, `bedside/diagnostics`, `bedside/stick`,
`bedside/diagnostics/request`; contract in DESIGN.md "Diagnostics"). Left:
- Give the server repo the brief (topics and JSON shapes are in DESIGN.md).
- Confirm the broker ACL for `bedside-esp32` covers the new topics: publish `bedside/diagnostics`
  and `bedside/stick`, subscribe `bedside/diagnostics/request`.
- Check the whole path with the server: ask for `{"fast_seconds": 30}` and watch it publish 1/s.

## Cleanup
- Fix the stale flash figure in the `platformio.ini` comment (build is about 59% of the slot,
  not 67%; BLE added about 283 KB).
- The first diagnostics message is printed to serial once per boot (`[diag] {...}`); drop it if
  it's no longer useful.

## Config and security
- `OTA_PASSWORD` in `secrets.h` is still the example placeholder; set a real one.
- MQTT broker is `192.0.2.10:1883` from `secrets.h`. It connects now; check whether the
  server's `GET /device/config` (via `SERVER_URL` and `DEVICE_TOKEN`) is supposed to supply it,
  and whether the device on `192.0.2.x` can reach the server.
- Decide whether to log the BLE passkey to serial (it is, for now).

## Hardware
- Verify buttons 3, 4, 5 and 8 and the joystick directions in the log (the stick now logs
  `[input] stick <direction>`); 1, 2, 6 and 7 were seen registering.
- Check the DHT11 shows temperature and humidity on the OLED.
- Optional: BH1750 `ADDR` to GND to force 0x23 (works left open now).
- Optional: sound sensor (HW-485) planned for GPIO32 (`AO`, ADC1); not wired or in the pinout.
- Optional: lower-risk I2C bus hardening (4.7 kΩ pull-ups) if dropouts ever return; the cause
  last time was the ground bus.
