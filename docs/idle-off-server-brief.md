# Idle screen-off: what the server needs

## What the device does now
The bedside ESP32 turns its screen off after a period with no activity, whatever the room light is. **Activity** is a control being touched (a button or the stick) or the PIR seeing movement. Any activity ends it: a touch wakes the screen (and is swallowed, like a wake in the dark, so it can't also trigger a button action), and movement brings the screen back. In a dark room movement does not light it.

The firmware side is done and tested. The device only needs the new setting delivered.

## The new setting
One field in the existing `dimmer` object of `bedside/config` (retained):

```json
{"dimmer": {"bright_lux": 20, "dark_lux": 2, "dim_contrast": 1, "wake_seconds": 15, "idle_off_minutes": 15},
 "sounds": {"enabled": true, "volume": 80, "dark_volume": 10}}
```

| Field | Type | Default | Range | Meaning |
|---|---|---|---|---|
| `idle_off_minutes` | integer | 15 | 0 to 1440 | Minutes with no touch and no movement before the screen turns off. **0 means never.** |

- The field is optional. A config without it leaves the device's current value alone (15 on a new device), so older retained messages are safe.
- The device clamps to 0 to 1440 and saves the value to flash, like the other dimmer settings, so it survives a power cut.
- The timer starts at boot.

## What to change in the server
Mirror the firmware's `DimmerSettings` (`include/dimmer.h` in the ESP32 repo), as the other dimmer fields already do:

1. `app/device.py`: add `idle_off_minutes: int = Field(15, ge=0, le=1440)` to `DimmerConfig`, with a docstring saying 0 is never.
2. `config.example/device.yaml`: add `idle_off_minutes: 15`, with a comment.
3. Everything that lists the dimmer fields: the example in `app/api/overview.py`, `frontend/src/lib/types.ts` (`DimmerConfig`), and the field list in `frontend/src/routes/device.tsx` (`DIMMER_FIELDS`), for example label "Turn the screen off after (minutes)", help "With no touch and no movement. 0 never turns it off." min 0, max 1440, step 1. The text near the end of the page that says a press wakes an off screen could mention it too.
4. Tests that spell out the dimmer object: `tests/test_device.py`, `frontend/src/routes/buttons.test.tsx`, `device.test.tsx` and `components/device-readings.test.tsx`.
5. `DESIGN.md` or the MQTT docs, wherever `wake_seconds` is described for the device's config.

The device reports `"screen":"off"` in `bedside/sensors` while it is idle, so the existing screen-state display needs no change.
