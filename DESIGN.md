# Bedside Dashboard — ESP32 client

A bedside sibling of [desk-dashboard-esp32](https://github.com/traviswolverton/desk-dashboard-esp32),
forked from it with full history. Connectivity, OTA and the NVS settings
pattern carry over; the display, controls and sensors are all new. For the
reasoning behind the inherited pieces (non-blocking MQTT, heap-churn-free
payload parsing, OTA partition layout), see the desk repo's `DESIGN.md` at
the fork point — `git log` here reaches it too.

## Role in the system
Same split as the desk unit: the ESP32 is the dumb end. It shows what
`bedside-dashboard-server` sends, reports its sensors, and publishes raw
button presses. What a button *does*, and every threshold worth tuning,
lives on the server.

## Hardware
- **MCU:** ELEGOO ESP32-WROOM-32 (38-pin DEVKIT V1), same board as the desk unit.
- **Display:** 0.91" SSD1306 OLED, 128×32, monochrome, I2C (0x3C). Not
  AMOLED, despite the listings — a small passive OLED. Draws nothing on
  black pixels, can be powered off by command, and has a contrast register.
- **Buttons:** 8 momentary push buttons on a Waveshare MCP23017 I/O
  expander (I2C, 0x27), GPA0–GPA7, each switching to GND. The expander's
  internal pull-ups are enabled; no resistors needed.
- **Joystick:** KY-023 (two potentiometers + a push switch). Axes on
  ESP32 ADC1; the switch on the expander's GPB0.
- **Light:** BH1750 on a GY-302 board (I2C, 0x23 with ADDR low/floating).
- **Temperature/humidity:** DHT11 on an HW-507 board (single-wire, own GPIO;
  the board carries the data pull-up).
- **Buzzer:** a passive piezo, driven directly from a GPIO by LEDC PWM —
  distinct pitches, volume by duty cycle. (Passive, not active: an active
  buzzer only has its one built-in tone, so sounds could differ only by
  rhythm.)
- **Power:** 5V USB into the ESP32. Everything else runs from its 3.3V pin.

**Everything is 3.3V.** Two boards invite 5V — the KY-023's pin is
literally labelled `5V`, and the HW-507 accepts 3.3–5V. Don't: the joystick
outputs a fraction of its supply, so at 5V it would put up to 5V into an
ESP32 pin rated for 3.3V; the DHT11's data line likewise swings to its
supply.

## Pin assignments

| Function | Signal | GPIO | Board silkscreen |
|---|---|---|---|
| I2C bus (OLED, BH1750, MCP23017) | SDA | 21 | `D21` |
| I2C bus | SCL | 22 | `D22` |
| MCP23017 | `INTA` | 27 | `D27` |
| DHT11 | data (`S`/`DAT`) | 4 | `D4` |
| Joystick | `VRx` | 34 | `D34` |
| Joystick | `VRy` | 35 | `D35` |
| Piezo buzzer | `+` | 25 | `D25` (its `−` to `GND`) |
| MCP23017 | GPA0–GPA7 | — | buttons 1–8, other leg to GND |
| MCP23017 | GPB0 | — | joystick `SW`, other side is its `GND` |
| MCP23017 | A0–A2 | — | leave at default (address 0x27: the Waveshare board ships with all three high) |
| All modules | VCC / GND | — | `3V3` / `GND` |

`include/pins.h` holds the same assignments in code. If the two ever
disagree, this table is what the hardware was built from.

See [`docs/pinout.html`](docs/pinout.html) for a visual wiring diagram of
the same pins (open it in a browser). It's drawn from this table and
`pins.h`, not from a built unit, so keep all three in step.

Why these pins:
- **21/22** are the ESP32's default I2C pins. The three I2C devices have
  distinct fixed addresses, so one bus serves them all.
- **34/35 for the joystick:** analog reads must use ADC1 (GPIO32–39) —
  ADC2 stops working whenever WiFi is on. 34/35 are input-only, which is
  all an analog read needs. They have no internal pulls, so an *unplugged*
  joystick floats; the firmware checks the boot-time centre reading and
  disables the stick if it's pinned near a rail, but a floating pin can
  land anywhere, so treat odd drift with nothing connected as expected.
- **27 for `INTA`:** the expander drives it open-drain and active-low
  (configured that way on purpose, so the ESP32's pull-up sets the idle
  level). Both expander ports are mirrored onto `INTA`, so `INTB` is unused.
- Still avoid: GPIO0/2/12/15 (boot straps), 1/3 (serial), 6–11 (flash).
- **25 for the buzzer:** a plain output with no boot-strap role (it's
  also DAC1, unused here).
- Free for later: 13, 14, 16–19, 23, 26, 32, 33.

```mermaid
graph LR
  subgraph ESP32["ESP32 · DEVKIT V1"]
    D21["D21 (SDA)"]
    D22["D22 (SCL)"]
    D27["D27"]
    D4["D4"]
    D34["D34"]
    D35["D35"]
    V3["3V3"]
  end
  subgraph I2C["I2C bus"]
    OLED["SSD1306 0x3C"]
    LUX["BH1750 0x23"]
    MCP["MCP23017 0x27"]
  end
  subgraph Controls
    BTN["Buttons 1-8 → GPA0-7"]
    JSW["Stick SW → GPB0"]
    JXY["Stick VRx / VRy"]
  end
  DHT["DHT11 (HW-507)"]
  D21 --- I2C
  D22 --- I2C
  MCP -- "INTA" --- D27
  BTN --- MCP
  JSW --- MCP
  JXY -- "VRx" --- D34
  JXY -- "VRy" --- D35
  DHT -- "data" --- D4
  V3 -. "3.3V to every module" .- I2C
```

**Mounting the light sensor:** at night the interesting light levels are
0–5 lux, and a lit OLED a few centimetres away is enough to move that. Put
the BH1750 where it sees the room, not the screen — facing up or out,
behind the panel's edge, never looking at the display. The firmware guards
against the obvious feedback loop (see Dimming) but can't undo a sensor
that mostly measures the screen.

## Firmware layout

| File | What it owns |
|---|---|
| `main.cpp` | Glue: WiFi/MQTT, topics, publishing, the loop |
| `display.*` | The SSD1306: U8g2 full-frame buffer, levels, burn-in drift |
| `dimmer.*` | Lux → Off / Dim / Full, and wake-on-touch |
| `input.*` | Buttons, joystick press and axes → debounced events |
| `sensors.*` | BH1750 (smoothed) and DHT11 readings |
| `screen.*` | What's drawn: one big line, the clock first, stick left/right for other pages |
| `local_time.*` | Central time from NTP (`CST6CDT`), 12-hour, for the clock page |
| `settings_store.*` | Dimmer and buzzer settings in NVS, surviving reboots |
| `sound.*` | The piezo: named tunes, non-blocking, quieter in the dark |
| `ota.*` | ArduinoOTA, hostname `bedside-dashboard-esp32` |
| `device_config.*` | Which broker to use, fetched from the server (below) |
| `http_get.*` | The small HTTP client that fetch uses (from the desk unit) |
| `wifi_config.*` | Which WiFi network to join: NVS, else `secrets.h`; new ones are a trial |
| `provisioning.*` | Setting the WiFi network from a phone over BLE (below) |
| `diagnostics.*` | The device's self-report for a server diagnostics page (below) |

Every module is non-blocking and polled from `loop()`, as on the desk
unit. The one deliberate exception is the DHT11 read (a few milliseconds
with interrupts held off, once every 10s).

A missing I2C device never stops the firmware: each module checks for its
chip at boot, logs what it didn't find, and the bench screen names it on
its bottom line (`No BH1750 MCP stick`). Serial also prints a full I2C
scan at boot.

## Dimming

Three levels, chosen from smoothed room light (lux):

| Room | Screen |
|---|---|
| `lux ≥ bright_lux` (default 20) | Full |
| `dark_lux ≤ lux < bright_lux` | Dim |
| `lux < dark_lux` (default 2) | Off — a touch wakes it at Dim for `wake_seconds` (default 15) |

- **Smoothing:** readings every 250ms, averaged over ~2s, so headlights
  across the ceiling don't flick the screen. A drop to real darkness is
  taken immediately rather than eased toward.
- **Hysteresis:** each threshold must be crossed by 20% (at least 0.5 lx)
  before the level changes.
- **The wake press does nothing else.** The touch that wakes a dark
  screen is swallowed — a button's press, the stick's movement until it
  returns to centre, the stick's click — so reaching over in the dark to
  see the time can't also toggle a light. Touches while already awake
  restart the timer and act normally.
- **The screen can't keep itself awake.** While woken in the dark, only
  a real light (past `bright_lux`) ends dark mode early; the screen's own
  glow on the sensor can't nudge the room into Dim and hold the screen on.
- **Dim is dimmer than contrast alone.** Contrast 0 on an SSD1306 is
  still bright in a dark room, so Dim also lowers the panel's pre-charge
  and VCOMH voltages (`display.cpp`). If a particular panel misbehaves at
  those values (flicker, uneven rows), that's the place to tune.
- **No sensor, no reading:** Full. Fail toward a readable screen.
- **Burn-in:** the whole frame drifts by one pixel every 3 minutes around
  a small square, so nothing sits on the same pixels all night.

Settings come from the server (`bedside/config`, below) and are saved to
NVS, so a power cut at 3am brings the screen back with the right
thresholds before WiFi is up.

## Controls

- **Buttons 1–8:** 25ms debounce. A press released before 600ms is
  `short`; held to 600ms fires `long` once (and nothing on release) and
  plays a tiny tick, so you know you can let go. Published as raw presses —
  what each does is configured on the server.
- **Joystick:** centre calibrated at boot (so don't hold it while
  powering on). Enters a direction past ~40% deflection, leaves below
  ~25%. Holding repeats after 450ms, then every 150ms. Its press is a
  plain event. The stick is for on-device navigation and isn't published.
  If it reads rotated once mounted, flip `kSwapAxes` / `kInvertX` /
  `kInvertY` in `input.cpp`.

## MQTT

Client ID `bedside-dashboard-esp32`, login `bedside-esp32`, on the
homelab's central broker (192.0.2.10:1883) — shared with Home Assistant,
which is how the sensors and button triggers reach it. The account's ACL
limits it to the topics below (bedside-dashboard-server's
`docs/mqtt-broker.md`).

| Topic | Direction | Retained | Payload |
|---|---|---|---|
| `bedside/config` | server → device | yes | `{"dimmer":{"bright_lux":20,"dark_lux":2,"dim_contrast":1,"wake_seconds":15},"sounds":{"enabled":true,"volume":80,"dark_volume":10}}` — any field optional |
| `bedside/sound` | server → device | no | `{"sound":"armed","urgent":false}` — see "Sounds" |
| `bedside/sensors` | device → server | yes | `{"temperature_c":21.5,"humidity":44,"lux":3.5,"screen":"off"}` — `null` for a sensor with no reading |
| `bedside/button` | device → server | no | `{"button":3,"press":"short"}` or `"long"` |
| `bedside/stick` | device → server | no | `{"stick":"up"}`: `up`, `down`, `left`, `right` (repeats while held) or `press` |
| `bedside/diagnostics` | device → server | yes | see "Diagnostics" |
| `bedside/diagnostics/request` | server → device | no | `{"fast_seconds":120}` — see "Diagnostics" |
| `bedside/availability` | device → server | yes | `online`, or `offline` (the connection's last will) |

`bedside/sensors` goes out every 30s, on every (re)connect, and whenever
the screen level changes. Presses aren't retained or queued while offline:
replaying one later would trigger whatever it's bound to after the fact.

Config values are clamped on arrival (`dark_lux` ≤ `bright_lux`, contrast
0–255, wake 3–600s), so a bad server value can't produce a screen that
never turns on.

### Broker settings from the server
Since 2026-09-28, the broker to use (host, port, `bedside-esp32` login)
comes from the server's `GET /device/config` instead of being fixed at
compile time (`device_config.cpp`, the same code as the desk unit). Moving
the device to another broker is then a change on the server's Connections
page, not a reflash. `secrets.h` needs `SERVER_URL` (the server's
plain-http LAN address) and `DEVICE_TOKEN` (made on that page, sent as
`Authorization: Bearer`). Without them, fetching compiles out and
`MQTT_*` are used as before.

- **When it asks:** at boot, before the first MQTT connect; every 15 min;
  and after 3 failed connects in a row, at most once a minute. Each ask
  is one blocking request, capped at 2 s. That's normally a few ms on the
  LAN, and the 2 s cap only applies while the server is down.
- **Trial, then keep:** an answer that differs from the settings in use
  is tried at once. It's written to NVS (`brokercfg`, separate from
  `settings_store`'s `bedcfg`) only after an MQTT connect succeeds with
  it. After 5 failed connects it's rolled back to the previous settings
  and ignored for an hour, so a mistake on the server can't strand the
  device.
- **Precedence at boot:** settings saved in NVS beat `MQTT_*` in
  `secrets.h`. The compiled-in values only matter until the server first
  gives a working answer.

## WiFi provisioning
The WiFi network is set from a phone over Bluetooth LE, so moving the
device to another network needs no reflash and no password in a file.
Code: `provisioning.*` (NimBLE) and `wifi_config.*` (storage).

- **Where the network comes from, in order:** saved in NVS (`wificfg`),
  else `WIFI_SSID`/`WIFI_PASSWORD` from `secrets.h` (both optional now),
  else none. A saved network wins over `secrets.h`.
- **Trial, then keep:** credentials from the phone are only a trial. They
  are saved after a connection succeeds; after 20s without one the
  previous network comes back. A wrong password changes nothing.
- **Setup mode** starts when there is no network at all, or when the
  joystick centre is held pressed for 3s at power-up. The OLED then shows
  `Setup: Bedside-XXXX`, a 6-digit passkey and a countdown. It ends when a
  trial succeeds, or after 5 minutes without phone activity (with no
  network at all it starts over instead).
- **Security:** BLE is up only in setup mode. The phone has to pair using
  the passkey shown on the OLED (MITM + Secure Connections, no bonding);
  the characteristics only accept encrypted, authenticated writes.
- **Phone side:** service `6e5b0001-2f5c-4c0e-9b7d-5a8f0c3e1d00`;
  characteristics `…0002` network name (write), `…0003` password (write),
  `…0004` apply (write any byte), `…0005` status (read, notify:
  `connecting`, `connected <ip>`, `failed: wrong password` / `network not
  found` / `could not connect`). nRF Connect on iOS works for this.
- **Cost:** about 283 KB of flash. The BLE stack is shut down when setup
  mode ends and its memory returned.

Status: written and flashed; setup mode starting has been seen, but a
phone has not yet completed a provisioning run (see `todo.md`).

## Diagnostics
For a server diagnostics page. The device publishes `bedside/diagnostics`
(retained) on every (re)connect and every 30s; the server asks for more
by publishing `{"fast_seconds": N}` (1–600, 0 to stop) to
`bedside/diagnostics/request`, and then diagnostics and `bedside/sensors`
both go out once a second for N seconds. Button presses and stick moves
arrive as events on `bedside/button` and `bedside/stick`.

```json
{"firmware":"Oct  3 2026 14:55:13","uptime_s":7,"heap_free":146316,
 "setup_mode":false,"wifi":{"rssi":-45,"ip":"192.0.2.30"},
 "hardware":{"oled":true,"light":true,"expander":true,"joystick":true,
             "climate":true},
 "stick":{"x":1910,"y":1902,"direction":"none","pressed":false},
 "buttons_down":[]}
```

- `hardware` says what answered: a `false` is a wiring problem. `climate`
  is false while the DHT11 has no reading.
- `stick.x`/`y` are raw 12-bit readings (centre about 1900); `null` when no
  joystick. `direction` is `none`, `up`, `down`, `left` or `right`.
- `buttons_down` lists the buttons held right now (debounced), 1–8.
- `firmware` is the build date and time.

## Bench bring-up

1. `cp include/secrets.h.example include/secrets.h` and fill it in.
2. `pio run -t upload` over USB (first flash must be USB — see the
   partition note in `platformio.ini`), then `pio device monitor`.
3. The serial log's `[i2c] found:` line should list `0x23 0x27 0x3C`.
4. The screen is one big line (24 px, `helvB24`; the panel is too small for
   three). It starts on the clock, `--:--` until NTP has set the time, then
   Central time with a small AM/PM. Push the stick right for temperature
   (°F), humidity and light, left to go back; it returns to the clock after
   8s. `W` and `M` at the top right show WiFi and MQTT up. A `!` at the top
   left means hardware is missing, with a last page naming it. A button or
   stick press shows for 1.5s (`BTN 3`, `HOLD 3`, `UP`, `PRESS`). In setup
   mode the big line is the passkey, alternating with a small status view.
5. Cover the light sensor: the screen dims, then turns off. Press a button:
   it comes back at Dim, and that press isn't published.

## Sounds
The server decides what a press did and says which sound to play on
`bedside/sound` (bedside-dashboard-server's DESIGN.md, "Button feedback":
confirm presses, and checking in Home Assistant that an action really
happened). The firmware only knows the tunes:

| Name | Means | Tune |
|---|---|---|
| (tick) | a hold just became a long press — played locally | 4 kHz click, 8 ms |
| `armed` | press the same way again to run it | one 1.76 kHz beep |
| `running` | sent to Home Assistant | two quick rising beeps |
| `success` | done (and checked, if the action has a check) | three rising notes |
| `failure` | failed, timed out, or never reached its state | three falling low notes, ~1.2 s |
| `cancel` | an armed action timed out or was cancelled | a short 880 Hz blip |

Rising means good, falling means bad, so they can be told apart half
asleep. A new sound cuts off whatever is still playing (the tick is
immediately replaced by `armed` for a confirm action).

**Volume:** `sounds.volume` normally; `sounds.dark_volume` while the
dimmer says the room is dark (below `dark_lux`, woken or not) — unless the
message is `urgent`, which the server sets for `failure` so a door that
didn't close is heard at night. `enabled: false` silences everything.
Duty cycle sets loudness (50% is the loudest a piezo gets); 0–100 maps to
duty squared so the quiet end has usable steps. Kept in NVS like the
dimmer.

## Still to build
- **A clock** — NTP time is the obvious resting screen at night, with
  the light, temperature and humidity readings.
- **On-screen confirmations** — a brief message alongside each button
  sound ("Press again: Close garage", "Garage closed", "Failed: timed
  out"), worded by the server. No menus: the screen is too small to
  scroll through lists, so actions are reached through buttons only.
- **Icons** — the desk's icon set was a TFT_eSPI `.vlw` font the OLED
  can't draw, so it's gone. Icons come back from U8g2's own icon fonts
  once the screen is designed.
- **Server side** — the bedside server stack (port 8001),
  button bindings, the dimmer settings UI, and Home Assistant MQTT
  discovery for the three sensors.
