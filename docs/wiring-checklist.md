# Wiring checklist

Four buses go through the bus board (see [pinout.html](pinout.html)): GND, 3V3, SDA and SCL, each a row of screw terminals. Everything else is a Dupont jumper straight from the ESP32 (or the expander) to the device. Any free screw on a bus is the same as any other, so fill them in whatever order is convenient.

## Wire colours

Red is always 3V3 and black is always GND.

| Signal | Pin | Colour |
|---|---|---|
| 3V3 | | red |
| GND | | black |
| SDA | D21 | blue |
| SCL | D22 | yellow |
| INTA | D27 | orange |
| DHT11 data | D4 | white |
| PIR out | D18 | green |
| Buzzer | D25 | brown |
| Stick VRx | D34 | purple |
| Stick VRy | D35 | grey |
| Stick press | PB0 | white |
| Buttons 1 to 8 | PA0 to PA7 | yellow |

## Bus board (26 wires)

Strip the wire about 6 mm, push it in fully, tighten, and tug it. Never put 5V on anything here: the joystick's pin printed 5V gets 3V3.

- [ ] **GND, 10 screws:** ESP32 (both GND pins), OLED, BH1750, expander, DHT11, PIR, buzzer, joystick, and the buttons' common ground (one wire).
- [ ] **3V3, 8 screws:** ESP32 3V3, OLED, BH1750, expander, DHT11, PIR, buzzer, joystick.
- [ ] **SDA, 4 screws:** ESP32 D21, OLED, BH1750, expander.
- [ ] **SCL, 4 screws:** ESP32 D22, OLED, BH1750, expander.

## Direct jumpers

- [ ] D4 to DHT11 S
- [ ] D18 to PIR OUT
- [ ] D25 to buzzer I/O
- [ ] D34 to joystick VRx
- [ ] D35 to joystick VRy
- [ ] D27 to expander INTA (leave INTB unconnected)

Never use EN, D12, D15 or D2: they are boot-strap pins.

## Device pin orders

Wire by each module's own silkscreen. Orders differ between modules, so don't assume.

| Device | Pin order | Notes |
|---|---|---|
| DHT11 (HW-507) | S, +, − | + to 3V3 |
| OLED | GND, VCC, SCK, SDA | SCK is the I2C clock, SCL. GND and VCC are the first two, the reverse of most modules. |
| BH1750 (GY-302) | ADDR, SDA, SCL, GND, VCC | Leave ADDR unconnected. Its board carries the bus's pull-ups. |
| Joystick (KY-023) | GND, 5V, VRx, VRy, SW | The pin printed 5V gets **3V3**. SW goes to PB0. |
| Buzzer module | I/O, VCC, GND | Power from 3V3. |
| Expander stock cable | VCC, GND, SDA, SCL, INTA, INTB (red, black, blue, yellow, orange, green) | Feed VCC 3V3, never 5V. |

### PIR (AM312)
The module has no silkscreen. Published pinouts agree that the **middle pin is OUT**, but disagree about which end is VCC and which is GND, and some clones swap them. Find out on your own board before connecting it:

1. Multimeter in continuity mode: touch the metal can and each end pin. The end that beeps is probably GND (hint, not proof).
2. Protected power test: ESP32 3V3 through a **1 kΩ resistor** to the end you think is VCC, ESP32 GND to the other end. If you have them backwards the resistor limits the current to about 3 mA.
3. Measure the middle pin against GND: about 0 V still, about 3.3 V for a few seconds after you wave (give it 10 s to settle after power-up). If it never changes, or the module warms up, remove power and swap the ends.
4. Mark the confirmed GND pin on the module.

- [ ] test done, order confirmed: VCC, OUT, GND  /  GND, OUT, VCC (circle one)

## Expander and the lid

The expander's PA header has PA7 at the top, then PA6 down to PA0, then GND and VCC at the bottom. Bend the pins flat so a plug goes on sideways, and wire by the printed labels, not position.

| Button | Header pin |
|---|---|
| 1 | PA0 |
| 2 | PA1 |
| 3 | PA2 |
| 4 | PA3 |
| 5 | PA4 |
| 6 | PA5 |
| 7 | PA6 |
| 8 | PA7 |

- [ ] eight yellow signal wires on PA0 to PA7. The header's GND and VCC are the two pins below PA0: don't use them for buttons.
- [ ] the buttons' eight ground terminals joined together, with **one** black wire from that join to one GND screw
- [ ] joystick press: one wire from PB0 (the top pin of the PB header, beside the connector) to the stick's SW. The PB header's bottom pins are GND and VCC.

## Before first power-up

Multimeter in continuity mode, with nothing plugged into the devices.

- [ ] No beep between any 3V3 screw and any GND screw, and none between SDA and SCL, or between SDA or SCL and either supply. A beep means a short or a stray strand.
- [ ] Each direct jumper beeps end to end.
- [ ] Only then plug the devices in, and watch the serial log's `[i2c] found:` line for `0x23 0x27 0x3C`.

If the log shows I2C errors or phantom stick presses, see [troubleshooting.md](troubleshooting.md): the usual causes are a device with no ground, the expander on 5V instead of 3V3, or a loose terminal screw.
