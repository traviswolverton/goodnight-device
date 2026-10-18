# Troubleshooting

Symptoms we have actually hit, what caused them, and how to tell. Use the serial log (`pio device monitor`, 115200) and, for the buttons, the on-screen echo (hold button 8 for 10 s: `KEYS ON`).

## I2C errors in the log

`[E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error -1` and `Wire.cpp:513 ... Error 263`, dozens per minute. Reads from the expander or the light sensor fail intermittently. Check, in this order:

1. **Every device has its GND connected.** A device with no ground can still answer the boot scan (`[i2c] found: 0x23 0x27 0x3C`) and corrupt the bus. A BH1750 with no ground gave about 50 errors per 100 s. With a meter in continuity mode, check each device's GND pin to a GND screw on the bus board.
2. **Everything is on 3V3, not 5V.** The expander on 5V needs a logic high of about 3.5 V, which the ESP32's 3.3 V never reaches. Measure the expander's VCC against GND: it should read about 3.3 V. The same goes for the OLED and BH1750.
3. **The BH1750 is powered and grounded.** Its board carries the bus's pull-up resistors, so without it SDA and SCL float.
4. **Terminal screws are tight** and each wire is gripped, not just resting in the clamp (tug it).
5. **Isolate.** Unplug the PA wires from the expander and watch the count; then unplug the expander's SDA/SCL (keep it powered); then the OLED, then the BH1750. The error rate drops when the culprit is removed.

A clean run is zero `Wire.cpp` lines in 100 s with all three addresses found.

## `[display] no SSD1306 at 0x3C` or `no BH1750` at boot, but the scan found them

The device answered the scan but failed its first real read: a flaky connection, a missing ground, or a bus fault as above. A missing OLED is retried every 10 s and starts as soon as it answers (`[display] SSD1306 found late`); once running it isn't probed, because probing makes it flicker. The BH1750 and the expander are only set up at boot, so after fixing the wiring reboot the board.

## A stick press that nobody made, or buttons that fire together

A failed I2C read comes back as all zeros, which looks like every button and the stick held at once. The firmware drops reads where all eight button bits are low, but a half-failed read can still produce a stray `[input] stick press` (the PB0 bit). Treat it as an I2C fault and work through the section above.

## Buttons

- **A button does nothing.** Check the wire from its PA pin to the switch, and from the switch to the shared ground. The expander's PA header runs PA7 at the top down to PA0, then GND and VCC; button *n* goes to PA*(n−1)*. If the header's pins are bent flat for a sideways plug, check each one makes contact. A press that never closes the contact can't be seen by the firmware at all, whereas a closed contact always shows in the log as `[input] button N short`.
- **A button seems unresponsive.** It's reported when you let go, not when you push, and held past 600 ms it becomes `long` instead. In a dark room the first press only wakes the screen and is swallowed on purpose. With the echo off (the default at every boot) nothing shows on the OLED for a press; hold button 8 for 10 s to turn it on, then each press shows `BTN n`.
- **Button 8 never appears in the log.** It's the device's own button: it's never published, and holding it for 10 s toggles the echo.

## Joystick

- **`[input] joystick centre x=0 y=0 -- out of range, joystick disabled` at boot.** VRx (D34) and VRy (D35) aren't reading mid-scale: check both signal wires, 3V3 (the pin printed 5V gets 3V3) and GND. A working stick reads about 1900 at rest, and it must be allowed to centre before the board boots, so don't hold it while powering on.
- **Directions are wrong.** The stick is mounted inverted, so `kInvertX` and `kInvertY` in `input.cpp` are both `true`. If you mount it differently, change those and `kSwapAxes`. The log prints `[input] stick <direction>` for each move.
- **The stick press doesn't register.** It goes to the expander's PB0 (the top pin of the PB header), not to the ESP32.

## DHT11: `climate: false`, or no temperature on the OLED

Check D4 to the module's S pin, 3V3 to `+`, and GND to `−` (the HW-507 order is S, +, −). The first reading can take up to 10 s after boot. Its vent slots are in the right wall, so the sensor needs air.

## PIR: no `[sensors] motion`

Wave a hand in front of it 10 s or more after boot. Check that the module's middle pin (OUT) goes to D18 and that the end pins are the right way round for your board: the AM312 has no silkscreen, and some clones swap VCC and GND (the test is in the [wiring checklist](wiring-checklist.md)). On a meter, OUT should be near 0 V at rest and about 3.3 V while it sees movement. The firmware reads it as a plain input on purpose: an internal pull-down holds the weak output near 1.6 V.

## The screen is off

That is usually the dimmer, not a fault. Below `dark_lux` (default 2 lx) the screen turns off, and a press wakes it at Dim for 15 s. It also turns off after `idle_off_minutes` (default 15) with no touch and no movement, and a touch or movement brings it back. Check the `[dim] lux=... screen=... idle=...` line in the log. If the BH1750 is covered or facing a wall, it reads dark all day.

## Reading the serial log

- **Megabytes of garbled or repeating text** is stale data the USB passthrough saved while nothing was reading, not the board spamming or boot-looping. Drain the port until it's quiet, reset the board (pulse RTS), and read only what follows the last `[boot]` line.
- **`/dev/ttyUSB0` disappears** when the board is handled or its cable moves. Wait for it to return, or flash over the air (below).
- **Flashing over the air** needs the board's IP (printed in the `[diag]` line and `[wifi] connected, IP: ...`); mDNS names often don't resolve. See the README.

## The red LED

The ESP32 board's red power LED is wired to 3V3 and can't be turned off in firmware. Cover it with black nail polish, or desolder it.
