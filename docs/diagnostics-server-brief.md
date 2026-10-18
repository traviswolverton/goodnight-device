# Bedside device diagnostics page: what to build

## Goal
A page on the server that shows the bedside ESP32's live state, so a hardware fault (a loose
wire, a dead sensor) is visible without a serial cable. The firmware side is done and flashed.
The contract below is also in this repo's DESIGN.md, under "Diagnostics".

## Topics
All are under `bedside/*`. The device's MQTT login is `bedside-esp32`.

| Topic | Direction | Retained | Payload |
|---|---|---|---|
| `bedside/diagnostics` | device → server | yes | JSON state, below |
| `bedside/diagnostics/request` | server → device | no | `{"fast_seconds": N}` |
| `bedside/sensors` | device → server | yes | `{"temperature_c":21.5,"humidity":44,"lux":3.5,"screen":"full","motion":false}`; a field is `null` when that sensor has no reading (`motion` is never null) |
| `bedside/button` | device → server | no | `{"button":3,"press":"short"}` or `"long"`, buttons 1 to 7. Button 8 is reserved for the device and is never published, though it still appears in `buttons_down` while held |
| `bedside/stick` | device → server | no | `{"stick":"up"}`: `up`, `down`, `left`, `right` (repeats while held) or `press` |
| `bedside/sound` | server → device | no | `{"sound":"armed","urgent":false}`; plays a tune on the buzzer |
| `bedside/availability` | device → server | yes | `online` or `offline` (the connection's last will) |

## `bedside/diagnostics` payload
```json
{"firmware":"c62c675","uptime_s":7,"heap_free":146316,
 "wifi":{"rssi":-45,"ip":"192.0.2.30"},
 "hardware":{"oled":true,"light":true,"expander":true,"joystick":true,"climate":true},
 "motion":false,
 "stick":{"x":1910,"y":1902,"direction":"none","pressed":false},
 "buttons_down":[]}
```
- `hardware.*` says what answered at boot. A `false` means a wiring problem: `oled` is the
  display, `light` the BH1750, `expander` the MCP23017 that reads the buttons and stick press,
  `joystick` the axes, and `climate` the DHT11 (false while it has no reading).
- `motion` is the PIR's output right now. The PIR is a single wire, so it has no `hardware` flag: a
  missing sensor looks the same as a quiet room. The AM312 holds its output on for about 2 s after
  movement, so `true` lasts a couple of seconds past the last movement, and the device sends no
  longer timeout of its own. It also rides in `bedside/sensors`, which goes out whenever motion
  starts or stops.
- `stick.x` and `y` are raw 12-bit readings, 0 to 4095, centred near 1900. They're `null` when
  no joystick is present. `direction` is `none`, `up`, `down`, `left` or `right`.
- `buttons_down` lists the buttons held right now, after debounce.
- `uptime_s` and `firmware` show whether the device rebooted or runs old firmware. `firmware` is
  the git commit it was built from, with `-dirty` added if built with uncommitted changes.
- `setup_mode` is no longer sent (the phone WiFi setup was removed); treat it as always false.

## Fast mode
The device normally publishes diagnostics every 30 s. To make the page live, publish
`{"fast_seconds": 60}` to `bedside/diagnostics/request` when the page opens. From then on, the
device publishes both `bedside/diagnostics` and `bedside/sensors` once a second for that long.
The maximum is 600, and `0` stops it early. Re-send the request every minute or so while the
page stays open, and don't leave it on when nobody is looking.

## Page suggestions
- **Status strip:** online or offline from `bedside/availability`, the device's age
  (`uptime_s`), signal strength (`rssi`), the IP, and the firmware build.
- **Hardware checklist:** the five flags as green or red. Red should be prominent.
- **Sensor readouts:** temperature, humidity and lux, with a clear "no reading" for `null`, and a
  motion indicator from `motion`.
- **Buttons 1 to 8:** a row of eight. Flash one on each `bedside/button` event, and show it as
  held while it appears in `buttons_down`.
- **Joystick:** a small pad that highlights the direction from `bedside/stick` and shows the
  raw x and y. The press shows as a separate dot.
- **Sound test:** buttons that publish a sound name to `bedside/sound`. Use the sound names the
  server already knows.

## Things to know
- `bedside/diagnostics` is retained, so it may be old. Show its age, and treat a stale message
  as "device not reporting", not as current state.
- The events (`bedside/button`, `bedside/stick`) are not retained. A page opened after a press
  won't see it.
- The device drops events while it's disconnected, by design.
- `heap_free` is for spotting leaks over time, not a pass or fail value.

## Please check
1. The `bedside-esp32` account must be allowed to publish `bedside/stick` on the central broker. The
   other two topics are proven (the device publishes `bedside/diagnostics` and obeys
   `bedside/diagnostics/request`), but no stick event has been seen arriving there yet. Move the stick
   and press a button while the server listens, and confirm both arrive.
2. The Buttons page lets someone bind button 8, which the device never publishes (it's the device's
   own). It would be better not to offer it.
3. The addresses in these docs (`192.0.2.x`) are documentation placeholders on purpose; the real
   ones are in `secrets.h`.

## Status
Confirmed on the live broker on 2026-10-18 by the server's session:
- The server has a Diagnostics page built from this brief (`/diagnostics`). It parses every field the
  device sends. It also still reads `setup_mode`, which defaults to false and is no longer sent.
- Fast mode works end to end: after a `{"fast_seconds": 10}` request the device published diagnostics
  and sensors once a second, live and not retained. Without a request it publishes about every 30 s.
- While anyone has the page open the server renews the request at most every 30 s, each for 60 s.
- A retained report replayed on connect is shown as waiting for a new report; one older than 90 s, or
  from an offline device, is shown as not reporting.

Not yet seen: a live `bedside/stick` or `bedside/button` event arriving on the server (nobody touched
the device while it listened).
