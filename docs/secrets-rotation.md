# Rotating secrets

The device never talks to OpenBao. Its secrets are written into
`include/secrets.h` by `scripts/gen_secrets.py` and compiled into the
firmware, so **changing a value in OpenBao changes nothing on the device until
it is rebuilt and flashed.** (The one exception is the broker login; see
below.)

This guide covers what each value is, whether it needs a flash, and the order
to do things in so the device is never left unable to be updated.

## What lives where

| OpenBao field | `secrets.h` define | What it does | Needs a flash? |
|---|---|---|---|
| `secrets.ota_password` | `OTA_PASSWORD` | Lets someone on the LAN push firmware | Yes, and mind the order below |
| `secrets.device_token` | `DEVICE_TOKEN` | Proves the device to the bedside server | Yes |
| `secrets.mqtt_password` | `MQTT_PASSWORD` | Broker login, used until the server supplies one | No, normally (see below) |
| `secrets.wifi_ssid`, `secrets.wifi_password` | `WIFI_SSID`, `WIFI_PASSWORD` | The network to join | Yes, and mind the order below |
| `config.server_url` | `SERVER_URL` | Where the device asks for its broker settings | Yes |
| `config.mqtt_host`, `config.mqtt_port` | `MQTT_HOST`, `MQTT_PORT` | Fallback broker address | No, normally |
| `config.mqtt_username` (optional) | `MQTT_USERNAME` | Broker account; defaults to `bedside-esp32` | No, normally |

The device also keeps whatever broker settings the server last gave it in its
own flash (NVS) and prefers those to the compiled-in ones, so the `MQTT_*`
values are only a fallback. See "Broker settings" in `DESIGN.md`.

## Pushing a change to the device

Use this for anything marked "Yes" above. It assumes the build machine has
`BAO_ADDR` and the AppRole settings in the environment (see the README).

1. Change the value in OpenBao.
2. Preview what will change. This prints names only, never values:

       scripts/gen_secrets.py --check

3. Write the new `include/secrets.h`. The previous file is kept as
   `include/secrets.h.old`:

       scripts/gen_secrets.py

4. Build:

       pio run -e esp32dev-ota

5. Upload. Either flash over USB:

       pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0

   or push over the air, with the device's IP address in `DEVICE_IP` (the
   `.local` name does not resolve across subnets):

       OTA_PASSWORD="$(scripts/gen_secrets.py --ota-password)"
       python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
           -i "$DEVICE_IP" -p 3232 -f .pio/build/esp32dev-ota/firmware.bin -a "$OTA_PASSWORD"
       unset OTA_PASSWORD

   Do not add `-d` to the espota command: it prints its options, password
   included. `pio run -t upload -e esp32dev-ota` can also hang, which is why
   espota is called directly.

6. Check it came back. With the USB cable, `pio device monitor` should show
   `[wifi] connected`, `[mqtt] connected` and `[ota] ready`.
7. Clean up: delete `include/secrets.h.old`, and run `pio run -t clean` so no
   old binary with the previous values is left in `.pio/`.

OTA needs the build machine to reach the device on UDP 3232, and the device to
connect back to the build machine on TCP. Across a firewall, allow both (pin
the return port with `--host_port=PORT` in `upload_flags` if you want a single
fixed rule).

## Order matters

### OTA password

The device checks uploads against the password it was *built* with, which is
the old one until you flash. So after step 3 the OpenBao value is **not** the
one the device expects. Upload with the old password, or use USB.

To use OTA for the rotation itself, take the old password from the file
before step 3 overwrites it, and use it for the upload only:

    OLD_OTA="$(sed -n 's/^#define OTA_PASSWORD *"\(.*\)"$/\1/p' include/secrets.h)"
    # ... steps 3 and 4, then in step 5 use -a "$OLD_OTA" ...
    unset OLD_OTA

Afterwards the new password works and the old one is refused. A later upload
with the new password is the check.

If the old password is known to be exposed, rotate it at once: anyone on the
LAN with it can replace the firmware.

### WiFi network or password

A device can only be updated over the air while it can still join its current
network. If you change the router's password or SSID *first*, the device can
no longer connect and OTA cannot reach it. Flash over USB instead, or keep the
old network up until the device has been updated and has rejoined.

### Device token

The token is created on the bedside server's Connections page (Bedside device,
then Device token). Make the new one there, put it in OpenBao
(`secrets.device_token`), regenerate and flash. Check whether the server keeps
the old token valid until you replace it. If it does not, the device cannot
ask the server for broker settings between those two steps, but it keeps
working on the broker settings it already saved.

### MQTT password

Normally no flash. The server hands the device the broker login, and the
device saves it once a connection with it works. So:

1. Change the password on the broker and on the server's Connections page.
2. The device picks it up within about 15 minutes, or sooner after a few
   failed connects.
3. Update `secrets.mqtt_password` in OpenBao so the compiled-in fallback is
   current, and regenerate on the next build.

## The build credential (AppRole)

`scripts/gen_secrets.py` logs in with the `bedside-build` AppRole, using
`~/.config/bedside/role-id` and `~/.config/bedside/secret-id` on the build
machine (mode 600, in a mode-700 directory). To rotate the secret-id, make a
new one in OpenBao, replace that file, check `scripts/gen_secrets.py --check`
works, then destroy the old secret-id by its accessor. The role and policy
are set up from the homelab repo, not here.

## If something was exposed

Rotate the exposed value first, then flash, then clean up.

| Exposed | Do |
|---|---|
| OTA password | Rotate at once; flash with the old password or USB |
| WiFi password | Change it on the router, then USB-flash the new one |
| Device token | New token on the server, then flash |
| MQTT password | Change on broker and server; the device follows |
| AppRole secret-id | New secret-id, destroy the old one by accessor |

After any rotation: delete `include/secrets.h.old` and run `pio run -t clean`.
Nothing here is committed to git: `include/secrets.h` and `include/secrets.h.old`
are gitignored. Never put values in a commit, an issue, or a chat.
