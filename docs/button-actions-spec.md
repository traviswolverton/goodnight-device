# Bedside device: button actions, version 1

## Context
The bedside ESP32 has 8 buttons. It publishes presses to MQTT as `{"button": N, "press": "short"|"long"}`
on `bedside/button` (not retained). The server decides what each press does and replies with a sound on
`bedside/sound`: `armed`, `running`, `success`, `failure` or `cancel`, with `urgent: true` on a failure so
it's heard at night. **Button 8 is reserved for the device and is never published**, so there are **14
bindable actions**: buttons 1 to 7, short and long. Buttons 1 to 6 are used below, and 7 stays free.

The primary user is someone who uses it half-asleep, so each action should be one press with predictable
behavior. Lamps are not smart on purpose and aren't controlled. The bedroom overhead and bathroom overhead
are both dimmable.

## The map

| Button | Short press | Long press (held 600 ms) |
|---|---|---|
| **1** | **Goodnight:** run the existing goodnight script unchanged (fan to max, lights off, Alexa joke) | **Quiet goodnight:** the same, **without the Alexa joke** |
| **2** | **Bathroom trip:** bathroom overhead on at night level (suggest **10%**), auto-off after **15 minutes** | Bathroom overhead off now, and cancel the timer |
| **3** | **Fan, step down:** high → medium → low → off → high (3 fixed speeds) | Fan off |
| **4** | Bedroom overhead light on/off (when turning on, use a default level, suggest 40%) | Bedroom overhead to a dim preset (suggest 10%) |
| **5** | **TV on and open the chosen app** (app to be decided) | TV off |
| **6** | TV play / pause | **Sleep timer:** TV off in **30 minutes** |
| **7** | spare | spare |
| **8** | device only: toggles the on-screen echo | — |

## Behavior rules
- **One press, no confirmation** for all of the above. If a confirm is wanted for any (TV off, goodnight),
  use the existing `armed` flow and say so.
- **Idempotent.** Pressing the same thing twice never leaves a surprising state. Bathroom trip while the
  light is already on sets 10% and restarts the 15-minute timer. Fan step down reads the **actual current
  state** from Home Assistant, not an assumed one. If the state is unknown, go to high.
- **Fan order matters.** Goodnight sets the fan to max, so the usual next change is "less". Step down from
  there, and from off go back to high.
- **Auto-off and sleep timers:** restart if the button is pressed again. A script with `mode: restart` and
  a `delay`, or a `timer` helper plus an automation, both work. Prefer whichever survives an HA restart.
- **Quiet goodnight** exists because a joke on the Alexa could wake someone who's already asleep.

## Feedback sounds
- `success` once the action has run and Home Assistant shows the expected state, as the server's existing
  "Button feedback" design does.
- `failure` with `urgent: true` if it fails or never reaches its state, so it's heard at night.
- Use `running` if an action takes more than about a second.

## What the Home Assistant side needs
Entity IDs aren't known here, so please look them up:
- the fan (3 speeds; confirm the step percentages)
- bedroom overhead light (dimmable) and bathroom overhead light (dimmable)
- the existing goodnight script, and a way to run it without the Alexa step (a variant, or a parameter)
- the Google TV remote entity from the Android TV Remote integration, and its media player for play/pause.
  Launching a specific app uses `remote.turn_on` with an activity or deep link. Confirm what the
  integration supports.
- timers or restart-mode scripts for the 15-minute and 30-minute timers

## Decisions still open
1. Bathroom night level (10%) and bedroom default level (40%) and preset (10%).
2. Which app button 5 opens.
3. Whether any action needs the `armed` confirmation.
4. Where the button bindings are configured in the server.

## Not in scope
The stick (`bedside/stick` events), the five-click Mario easter egg, and button 8's echo toggle are
device-side and stay as they are.
