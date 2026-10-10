# Crimping checklist

One section per cable. Every wire is defined by its signal, so the order can differ at the two ends: at each end, put the wire on the pin with that name, and let the wires cross over in between. The hub side is fixed by the strip map in [pinout.html](pinout.html); the device side is each module's own silkscreen order.

Position 1 at a device is the first label in the order given below. Where a wire crosses over (the device's order differs from the hub's), the table says so: these are the ones to double-check.

## Before you start

- **Colours.** Follow the table below. The same signal is the same colour at both ends of every cable.
- **Mark the hub plugs.** Solder the XH headers into the hub first. Then fit each plug on its header and put a dot on the end of the plug nearest strip **s0**. XH plugs only fit one way, so that dot tells you which end is "hub position 1" for every plug below, counting from the dot.
- **Mark the device housings.** For each Dupont-style housing, put a dot on the end that goes on the module's position 1 pin (the first label in its order below).
- **Tag every cable** with its name before you move on.

## Wire colours

Red is always 3V3 and black is always GND. No two wires in one plug share a colour. A colour can repeat on different cables when they never share a plug (white is the DHT11's data and the stick's press; yellow is SCL and the lid's button signals).

| Signal | Pin | Colour | Where |
|---|---|---|---|
| 3V3 | | red | every cable |
| GND | | black | every cable, and the button grounds |
| SDA | D21 | blue | OLED, BH1750, expander |
| SCL | D22 | yellow | OLED, BH1750, expander |
| INTA | D27 | orange | expander |
| DHT11 data | D4 | white | DHT11 |
| PIR out | D18 | green | PIR |
| Buzzer | D25 | brown | buzzer |
| Stick VRx | D34 | purple | joystick |
| Stick VRy | D35 | grey | joystick |
| Stick press | PB0 | white | its own one-wire plug |
| Buttons 1 to 8 | PA0 to PA7 | yellow | the lid harness you already have |

The ESP32's ten wires are all different, so you can tell them apart in the two housings: red, black, white, green, blue, yellow on the 3V3 row, and orange, brown, grey, purple on the VIN row (in plug order).

## Cables to the hub

For each, "hub pos" counts from the dot on the XH plug; "device pos" counts from the dot on the device housing.

### DHT11 (HW-507), hub port A, s0 to s2
Device order: **S, +, −** (3 pins, 3-way housing)

| Wire | Signal | Hub pos | Device pos | |
|---|---|---|---|---|
| red | 3V3 (s0) | 1 | 2 (+) | crosses |
| white | D4 (s1) | 2 | 1 (S) | crosses |
| black | GND (s2) | 3 | 3 (−) | straight |

- [ ] hub plug filled red, white, black
- [ ] device housing filled white, red, black

### OLED, hub port A, s9 to s12
Device order: **GND, VCC, SCK, SDA** (4 pins, 4-way housing). SCK is the I2C clock, SCL.

| Wire | Signal | Hub pos | Device pos | |
|---|---|---|---|---|
| red | 3V3 (s9) | 1 | 2 (VCC) | crosses |
| blue | SDA (s10) | 2 | 4 (SDA) | crosses |
| black | GND (s11) | 3 | 1 (GND) | crosses |
| yellow | SCL (s12) | 4 | 3 (SCK) | crosses |

- [ ] hub plug filled red, blue, black, yellow
- [ ] device housing filled black, red, yellow, blue
- ⚠ GND and VCC are swapped between the two ends. Straight-through would reverse the power.

### BH1750 (GY-302), hub port B, s9 to s12
Device order: **ADDR, SDA, SCL, GND, VCC** (5 pins). Leave ADDR unconnected: use a 4-way housing on the last four pins, or a 5-way with cavity 1 empty.

| Wire | Signal | Hub pos | Device pos | |
|---|---|---|---|---|
| red | 3V3 (s9) | 1 | 5 (VCC) | crosses |
| blue | SDA (s10) | 2 | 2 (SDA) | same number, but check |
| black | GND (s11) | 3 | 4 (GND) | crosses |
| yellow | SCL (s12) | 4 | 3 (SCL) | crosses |

- [ ] hub plug filled red, blue, black, yellow
- [ ] device housing (cavity 1 empty) filled empty, blue, yellow, black, red
- ⚠ The whole order is reversed between the two ends.

### Joystick (KY-023), hub port C, s6 to s9
Device order: **GND, 5V, VRx, VRy, SW** (5 pins). Use a 4-way housing on the first four, and the press switch separately (below).

| Wire | Signal | Hub pos | Device pos | |
|---|---|---|---|---|
| black | GND (s6) | 1 | 1 (GND) | straight |
| purple | D34 (s7) | 2 | 3 (VRx) | crosses |
| grey | D35 (s8) | 3 | 4 (VRy) | crosses |
| red | 3V3 (s9) | 4 | 2 (5V pin) | crosses |

- [ ] hub plug filled black, purple, grey, red
- [ ] device housing filled black, red, purple, grey
- ⚠ The pin printed 5V gets **3V3**. Never 5V.

### Joystick press, one-wire plug
- [ ] one-wire Dupont-style plug on the stick's **SW** pin (position 5), white wire; the other end goes to the expander's PB0.

### PIR (AM312), hub port B, s2 to s4
The module has no silkscreen on either side. Published pinouts agree that the **middle pin is OUT**, but they disagree about which end is VCC and which is GND, and some clones swap them. So find out on your own board before you crimp, and **don't connect it to the hub until you have**.

**How to find the ends**
1. Unplugged, put a multimeter in continuity mode and touch the metal can around the lens base and each end pin in turn. The end pin that beeps is probably GND (the can is normally tied to ground). Treat this as a hint, not proof.
2. Confirm with a protected power test. Wire the ESP32's 3V3 through a **1 kΩ resistor** to the end pin you think is VCC, and the ESP32's GND to the other end pin. The module draws microamps, so the resistor costs it nothing, but if you have the ends backwards the resistor limits the current to about 3 mA, which won't hurt it.
3. Measure the middle pin against GND. About 0 V when still, about 3.3 V for a couple of seconds after you wave a hand in front of it (give it 10 s to settle after power-up). If it never changes, or the module warms up, remove power and swap the ends.
4. Mark the confirmed GND pin on the module itself with a dot of marker, so future you doesn't need the test.

**Then crimp, by the device order you found**

| Wire | Signal | Hub pos | Device pos if the order is VCC, OUT, GND | Device pos if the order is GND, OUT, VCC |
|---|---|---|---|---|
| black | GND (s2) | 1 | 3 | 1 |
| green | D18 (s3) | 2 | 2 | 2 |
| red | 3V3 (s4) | 3 | 1 | 3 |

- [ ] test done, device order confirmed: VCC, OUT, GND  /  GND, OUT, VCC (circle one)
- [ ] hub plug filled black, green, red
- [ ] device housing filled to match the column above

### Buzzer, hub port A, s5 to s6
| Wire | Signal | Hub pos | Device |
|---|---|---|---|
| brown | D25 (s5) | 1 | + lead |
| black | GND (s6) | 2 | − lead |

- Device end is soldered to the two leads (no housing).

### Expander, hub port D, s9 to s13
The stock cable's order is VCC (red), GND (black), SDA (blue), SCL (yellow), INTA (orange), but the hub plug's order is now 3V3, SDA, GND, SCL, INTA, because no two rail strips may be neighbours. **Swap the black and blue wires in the hub-end plug.**

| Wire | Signal | Hub pos |
|---|---|---|
| red | 3V3 (s9) | 1, to VCC (feed 3V3, never 5V) |
| blue | SDA (s10) | 2 |
| black | GND (s11) | 3 |
| yellow | SCL (s12) | 4 |
| orange | D27 (s13) | 5, to INTA |

- [ ] **Leave the green INTB wire out of the housing**: s14 is ground and sits next to it.

## ESP32 end (wires soldered to the hub strips)

The board has 15 pins on each side. It sits flipped (pins up, USB-C at the left wall), so **count positions from the USB-C end** along each row. Name the rows by their power pin. Fill **only** the listed cavities of each housing, and leave the others empty. Never put a wire on EN, D12, D15 or D2: they are boot-strap pins.

### 3V3-row housing, 15-way, six cavities filled
This is the row with 3V3, GND, D15, D2, D4 ... D22, D23, counted from the USB-C end.

| Position | Pin | Colour | To hub strip |
|---|---|---|---|
| 1 | 3V3 | red | s0 |
| 2 | GND | black | s2 |
| 5 | D4 | white | s1 |
| 9 | D18 | green | s3 |
| 11 | D21 (SDA) | blue | s10 |
| 14 | D22 (SCL) | yellow | s12 |

- [ ] six wires, in positions 1, 2, 5, 9, 11 and 14

### VIN-row housing, 12-way, four cavities filled
This is the row with VIN, GND, D13, D12, D14, D27, D26, D25 ... EN, counted from the USB-C end.

| Position | Pin | Colour | To hub strip |
|---|---|---|---|
| 6 | D27 | orange | s13 |
| 8 | D25 | brown | s5 |
| 11 | D35 | grey | s8 |
| 12 | D34 | purple | s7 |

- [ ] four wires, in positions 6, 8, 11 and 12

## Buttons and the lid

### Button ground
- [ ] 8-way housing on the 1×8 header along s14. Every wire is ground (black), so the order doesn't matter. The other ends go to each button's ground terminal.

### Button signals
The expander's PA header has PA7 at the top, then PA6 down to PA0, then GND and VCC at the bottom. Count positions from the PA7 end, so button *n* goes to position 9 − *n*.

| Button | Header pin | Housing position |
|---|---|---|
| 1 | PA0 | 8 |
| 2 | PA1 | 7 |
| 3 | PA2 | 6 |
| 4 | PA3 | 5 |
| 5 | PA4 | 4 |
| 6 | PA5 | 3 |
| 7 | PA6 | 2 |
| 8 | PA7 | 1 |

- [ ] 8-way housing on the PA header, positions 1 to 8, yellow wires (the lid harness you already have). Don't fill position 9 or 10: those are the header's GND and VCC.
- Option: the header's GND sits right beside PA0, so a 9-way housing could carry the buttons' ground too, in position 9, if you'd rather bring it straight from the lid than through the hub's s14 header.

### Stick press
- [ ] one-wire plug on **PB0**: the top pin of the PB header, beside the connector. The other end goes to the stick's SW pin (position 5, above). Don't use the PB header's bottom pins: those are GND and VCC.

## Before first power-up

Use a multimeter in continuity mode, with the cables unplugged from the devices.
- [ ] No beep between strips s0, s4, s9 (3V3) and strips s2, s6, s11, s14 (GND), and none between any two neighbouring strips. A beep means a short or a solder bridge.
- [ ] For each cable: a beep from its hub header pin to the matching pin on the device end, and no beep between any two pins of the same cable. The crossing ones (OLED, BH1750, DHT11, joystick) are the ones to test carefully.
- [ ] The ESP32 housings: each wire beeps from its strip hole to the right ESP32 pin.
- [ ] Only then plug the devices in, and watch the serial log's `[i2c] found:` line for `0x23 0x27 0x3C`.
