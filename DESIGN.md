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
- Power: Waveshare UPS Module 3S (3x 18650 in series), 5V/5A output,
  supports simultaneous charge+discharge, I2C battery monitoring. This is
  the final power architecture — no separate charger/boost module needed.

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
The server sends a pixel-index -> color mapping in its status payload (or a
separate small payload) — e.g. `{0: "#1D9E75", 1: "#BA7517", ...}`. The
firmware just sets those pixel colors directly; it does not decide which
service maps to which pixel — that's server-side config.

## Testing before hardware arrives
Use **Wokwi** (wokwi.com, also a VS Code extension) — simulates ESP32 +
ST7789 + SSD1306 + rotary encoder + WS2812, all relevant parts here. Its
simulated ESP32 has working WiFi that reaches the public internet.

To test against the real homelab Mosquitto broker (LAN-only, not public),
use `wokwi-server`, a local proxy that bridges the simulated device's
network calls to the real local network.

Note: Wokwi does not simulate the UPS/battery power electronics — that
part still needs validation on real hardware once it arrives.

## Open items for Claude Code to help resolve
- Confirm the `/status` (or MQTT payload) JSON schema against the server
  repo before writing the parser
- Decide exact SPI pin assignments once wiring is finalized, write
  TFT_eSPI's User_Setup.h accordingly
- Write the Wokwi diagram.json for local simulation
- Decide whether "Mute alerts" and "Pin a screen" states need to persist
  across a reboot (likely not, given desk-bound always-on nature) or reset
  each boot
