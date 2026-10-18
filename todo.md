# To do

## Screen
The one-line screen (clock, temperature, humidity, light; stick left/right) is built. A flicker
every 10 s turned out to be the display's periodic panel ping (an address-only I2C transaction
to the OLED); it is gone now that the panel is no longer probed while running.

## Diagnostics for the server page
Done on both sides: the firmware (`src/diagnostics.cpp`, `bedside/diagnostics`, `bedside/stick`,
`bedside/diagnostics/request`; contract in DESIGN.md "Diagnostics") and the server's `/diagnostics`
page. Fast mode and the stick events were checked end to end on the live broker. Left:
- Ask the server to stop offering button 8 on its Buttons page (the device never publishes it).

## Hardware
All of it is built, wired and working: the OLED, the BH1750, the eight buttons, the joystick (both
axes inverted in `input.cpp`, as mounted), the DHT11 and the AM312 PIR. What's left:
- Decide what else the PIR is for on the server (night light, occupancy). On the device it only
  counts as activity for the idle screen-off timer.
- Add `idle_off_minutes` (default 15, 0 = never) to the server's device config and settings page;
  the firmware side is done. Brief: [docs/idle-off-server-brief.md](docs/idle-off-server-brief.md).
