# Desk Dashboard — ESP32 client

## Role in the system
This is the "client" in a server/client relationship with a homelab-side
adapter microservice (separate repo). The ESP32 is intentionally dumb — it
subscribes to MQTT, parses whatever JSON the server publishes, and renders
it. No business logic (severity decisions, adapter polling) belongs here.

## Hardware
- MCU: ESP32-WROOM-32 dev board (ELEGOO), USB-C
- Display: Waveshare 2" IPS LCD, ST7789VW driver, 240x320, SPI (use
  TFT_eSPI library; needs a `User_Setup.h` matching actual wiring pins).
  Native orientation is portrait 240x320 — rotate to landscape 320x240 in
  software via TFT_eSPI's rotation setting.
- Input: KY-040 rotary encoder module (GND, +, SW, DT, CLK) — rotation for
  navigation, integrated push button for select/menu-open. Onboard pull-ups
  may or may not be present on the breakout — verify, or use
  `INPUT_PULLUP` on the ESP32 side if not.
- Status LEDs: 5x WS2812 (NeoPixel) addressable strip. Use FastLED or
  Adafruit_NeoPixel. Add a series resistor (~300-500 ohm) on the data line
  and a decoupling cap (~100-1000uF) across 5V/GND at the strip's input.
  Data line is 3.3V (ESP32 GPIO) driving 5V-logic pixels — works fine at
  this scale over short wire runs; revisit with a level shifter
  (74AHCT125) only if signal issues appear.
- Power: External 5V/2A DC supply (USB-C wall adapter) — no battery/UPS.
  Desk-bound and always-on, so battery backup isn't solving a real problem
  here; deferred to a future project instead. 2A over 1A for headroom: WS2812
  strip at full white (~300mA) plus a WiFi TX burst (~500mA momentary) plus
  display/backlight leaves a 1A supply almost no margin, risking brownout
  resets. Bring 5V/GND from the adapter to a shared point and branch to (a)
  the ESP32's `5V`/`VIN` pin (or USB-C port) and (b) the LED strip's 5V/GND
  input directly — don't route strip current through the ESP32's onboard
  3.3V regulator.

## Firmware architecture
### Connectivity
- WiFi connect with reconnect logic (don't assume it stays up)
- MQTT client (PubSubClient or ArduinoMqttClient) subscribing to:
  - `homelab/status` — full status payload; update relevant UI on receipt
  - `homelab/alert/camera` — jump to camera pop-up view on receipt
- MQTT reconnect logic is a real piece of work here, not an afterthought —
  handle broker disconnects gracefully, don't just hang

### Screen state machine
Three states, not more:
1. **HOME** (default) — top banner (~30px) shows alert status
   (green/amber/red + short text); body auto-rotates through status cards
   every 4-5s; bottom shows position dots. Manual encoder rotation pauses
   auto-rotate for ~10s then resumes.
2. **MENU** — opened by encoder button press from HOME. Full-screen
   overlay, vertical list, 4-5 visible rows, current selection
   highlighted (inverted color). Items: Pin a screen, Mute alerts,
   Refresh now, Network info, Back. 15-20s idle timeout returns to HOME.
3. **CAMERA** — reached via menu item, or auto-triggered by an incoming
   `homelab/alert/camera` message. Decodes a JPEG snapshot (TJpg_Decoder)
   proxied from the server — NOT live video, ESP32 cannot decode
   RTSP/H.264. This is a distinct screen state, not blended into
   HOME's auto-rotation, since JPEG decode + SPI blit is slow enough to
   disrupt the main loop if run continuously.

### Non-blocking is mandatory
No `delay()` calls anywhere in the main loop — use `millis()`-based timers
for card rotation, menu timeout, etc. Blocking delay breaks encoder
responsiveness and MQTT message handling.

### Pixel mapping
The server sends a pixel-index -> color mapping as the `pixels` object in
`homelab/status` — e.g. `{"0": "#1D9E75", "1": "#BA7517", ...}`, keys `"0"`
through `"4"`, value `null` for an unassigned pixel. The firmware just sets
those pixel colors directly (or turns the pixel off on `null`); it does not
decide which service maps to which pixel or which color a status implies —
that's all server-side (see `pixel_map` config in the server repo).

## MQTT JSON payload schema
Locked against the server repo (`desk-dashboard-server/DESIGN.md`) — both
docs carry this section verbatim; update them together if it changes.

### `homelab/status`
Published on any adapter state change and as a ~30-60s heartbeat regardless.
```json
{
  "version": 1,
  "ts": 1737000000,
  "overall": {
    "level": "ok",
    "text": "All systems normal"
  },
  "services": [
    {
      "name": "plex",
      "status": "ok",
      "value": 4,
      "unit": "streams",
      "detail": "4 active streams"
    }
  ],
  "pixels": {
    "0": "#1D9E75",
    "1": "#BA7517",
    "2": "#1D9E75",
    "3": "#1D9E75",
    "4": null
  }
}
```
- `overall.level` is `"ok"` | `"warn"` | `"down"` — drives the HOME banner
  color directly; the client does not derive it from `services`.
- `overall.text` is the short banner string, already worded server-side.
- `services` is an array, in server-decided order — that order is the HOME
  card rotation order. Each entry matches the adapter's `fetch()` shape
  (`status`/`value`/`unit`/`detail`) from the server repo.
- `pixels` keys are always strings `"0"`-`"4"` (JSON object keys can't be
  bare ints); `null` means "off/unassigned", not "no change".
- `ts` is unix seconds. Not currently used by the firmware but parse it
  anyway — useful for staleness checks later (e.g. banner goes gray if no
  message for N minutes).

### `homelab/alert/camera`
```json
{
  "camera": "front_door",
  "ts": 1737000000,
  "snapshot_url": "http://<host>/api/frigate/front_door/latest.jpg",
  "label": "person",
  "text": "Front Door: person detected"
}
```
- `snapshot_url` is fetched by the ESP32 over plain HTTP GET (not embedded
  in the MQTT payload — keeps the message small and avoids broker
  max-packet-size issues). Server has already resized the JPEG to the
  display's resolution.
- `label` is the detected object class, optional — omit or `null` if not
  applicable.
- `text` is the pre-worded string for the CAMERA screen's caption.

### Firmware implication
Default `PubSubClient` payload buffer is 256 bytes — too small once
`services` has more than 1-2 entries. Call
`mqttClient.setBufferSize(...)` (or equivalent for whichever MQTT lib is
chosen) sized to the largest expected `homelab/status` payload, with
headroom for adapter count growing past the initial 2-3.

## Testing before hardware arrives
Use **Wokwi** (wokwi.com, also a VS Code extension) — simulates ESP32 +
ST7789 + SSD1306 + rotary encoder + WS2812, all relevant parts here. Its
simulated ESP32 has working WiFi that reaches the public internet.

To test against the real homelab Mosquitto broker (LAN-only, not public),
use `wokwi-server`, a local proxy that bridges the simulated device's
network calls to the real local network.

## Open items for Claude Code to help resolve
- ~~Confirm the `/status` (or MQTT payload) JSON schema against the server
  repo before writing the parser~~ — done, see "MQTT JSON payload schema"
  above (locked with the server repo)
- Decide exact SPI pin assignments once wiring is finalized, write
  TFT_eSPI's User_Setup.h accordingly
- Write the Wokwi diagram.json for local simulation
- Decide whether "Mute alerts" and "Pin a screen" states need to persist
  across a reboot (likely not, given desk-bound always-on nature) or reset
  each boot
