# ESPHome configuration

Turns the OSCD Demo PCB into a Home Assistant device with no C++ to write. Every peripheral on the board has a first-class ESPHome component, so the whole thing is one YAML file.

| File | Purpose |
|---|---|
| [`oscd_demo_pcb.yaml`](oscd_demo_pcb.yaml) | The device configuration |
| [`secrets.yaml.example`](secrets.yaml.example) | Template for Wi-Fi / API / OTA credentials |

## Requirements

**ESPHome 2026.6.0 or newer.** The `lsm6ds` motion platform this config uses for the IMU was added in 2026.6.0. The config declares `min_version`, so an older ESPHome refuses it with a clear message instead of failing halfway through a build.

```bash
pip install esphome
```

## First flash

```bash
cp secrets.yaml.example secrets.yaml   # then fill it in
esphome run oscd_demo_pcb.yaml
```

The board has no auto-reset circuit, so the **first** flash needs the bootloader entered by hand: **hold BOOT (`SW2`) → tap RESET (`SW1`) → release BOOT**, then start the upload. Tap RESET when it finishes.

After that first flash the board is on Wi-Fi and every subsequent update goes over OTA — the buttons are never needed again. On a board with no USB-UART bridge and no auto-reset, that is a real ergonomic win over the Arduino workflow.

`secrets.yaml` is gitignored. Do not commit it.

## What it exposes

| Entity | Source |
|---|---|
| **Ring** (light, 12 LEDs) | WS2812B chain on `GPIO8`, effects: Rainbow, Scan, Pulse, Touch trace |
| **Accel X/Y/Z**, **Roll**, **Pitch** | LSM6DS3 @ `0x6A` |
| **IMU Temperature** | LSM6DS3 on-chip sensor |
| **Ambient Light** (lux) | APDS-9306-065 @ `0x52` |
| **Slider 1–6** (binary) | MPR121 `ELE0`–`ELE5` — `TP2`, the inner slider |
| **Pad 1–6** (binary) | MPR121 `ELE6`–`ELE11` — `TP1`, the outer buttons |
| **Button SW3 / SW4 / SW5** | `GPIO1` / `GPIO0` / `GPIO2`, active low |
| **WiFi Signal**, **Uptime** | Diagnostics |

All of it is also reachable from the board's own web UI at `http://<device-ip>/` — see below.

`SW3` toggles the ring locally, so the board does something useful even with Home Assistant unreachable. The **Touch trace** effect reproduces the Arduino demo's behaviour — each electrode lights the LED nearest it, using the same pad-to-LED map worked out in `test_sensors.ino`.

## Board-specific settings worth understanding

These are the lines that are about *this* board rather than boilerplate. If you adapt the config, keep them.

- **`logger: hardware_uart: USB_SERIAL_JTAG`** — there is no USB-UART bridge, so logs go over the C3's native USB peripheral. This is the default on C3 and is written out only because it is the thing people most often get wrong on this chip.
- **`use_psram: false`** — the ESP32-C3-MINI-1 has no PSRAM. The LED component would otherwise try to use it.
- **`ignore_strapping_warning: true` on `GPIO8` and `GPIO2`** — both are ESP32-C3 strapping pins that must be high at reset. `R3` and `R2` (10 kΩ to 3V3) hold them there by design, so the warning is expected and is acknowledged rather than left to fire on every build. `R2` is also why the `SW5` pull-up `R8` is DNP.
- **No internal pull-ups on the buttons** — the board has external 10 kΩ pull-ups and 100 nF debounce caps.
- **`address: 0x5A` (MPR121) and `0x6A` (LSM6DS3)** — set by the fitted 0 Ω links `R21` and `R27`. If you re-jumper the board per [docs/hardware.md](../../docs/hardware.md#address-select-options), change these to match.

## Verification status

`esphome config` validates cleanly against **ESPHome 2026.8.2** with no warnings, and the firmware compiles clean — including the `addressable_lambda`, which config validation does not typecheck.

```
RAM:    34.1%  (109436 / 321296 bytes)
Flash:  60.0%  (1101716 / 1835008 bytes)
```

Flash includes ~101 KB for the embedded web UI (see [Demoing without Home Assistant](#demoing-without-home-assistant)); without `web_server` the build is 54.4%.

That leaves real headroom, which matters if you later add TLS or a VPN client — see [Running away from Home Assistant](#running-away-from-home-assistant). It has **not** been run on physical hardware — sensor readings, touch thresholds and the LED map are from the schematic and the Arduino demo, not from a bench test. Treat the touch `touch_threshold` / `release_threshold` values as starting points; the right numbers depend on your enclosure and how the board is held.

## Demoing without Home Assistant

The board serves its own UI, so it is fully demonstrable with nothing but a phone.

- **On your network:** browse to `http://<device-ip>/`.
- **Away from any known network:** the board fails to find one, raises its own access point (`OSCD Demo PCB Fallback`), and the same UI is at **`http://192.168.4.1/`**. Every entity is live — toggle the ring, pick effects, watch the sensors update.

### `local: true` is the setting that makes this work

Without it, the page loads its CSS and JS from `esphome.io` — exactly what you do not have while connected to the board's own AP. You would get a blank page at the worst possible moment.

With it, the UI is compiled into the firmware. Verified in the build: `USE_WEBSERVER_LOCAL` is defined, the generated `server_index_v3.h` asset is compiled in, and `web_server.cpp` serves it straight from flash. It costs ~101 KB of flash and essentially no RAM.

(`esphome config` still prints a `js_url` pointing at the CDN even with `local: true`. That value is vestigial — the embedded path does not use it. Do not let the config dump talk you out of this setting.)

### Two things to know before standing in front of an audience

**The fallback AP only appears if the board cannot reach a known network.** If you are demoing somewhere you have previously joined the Wi-Fi, it will connect to that instead and no AP will appear. It also takes a few seconds after power-up to give up and switch over. Test this in the room before you need it.

**`web_server` and `captive_portal` both want port 80.** In fallback-AP mode `web_server` takes it, which is what you want here — but it means the captive portal's Wi-Fi provisioning page is not reachable at the same time. This is a long-standing and somewhat version-dependent interaction upstream, so if a future ESPHome release flips the precedence, move the web UI to its own port and address them separately:

```yaml
web_server:
  port: 8080     # http://192.168.4.1:8080/ — captive portal keeps 80
```

Typing an IP and port skips DNS, so the captive portal cannot intercept it.

### OTA is off on the web UI

`ota: false` keeps firmware upload off a page you may hand to strangers. OTA still works through the Home Assistant API and the captive portal. If you want a password on the UI itself:

```yaml
web_server:
  auth:
    username: oscd
    password: !secret web_password
```


## Running away from Home Assistant

Short answer: **Home Assistant's external URL does not help here, because the connection runs the other way.**

The ESPHome native API is not a device that dials home. The device *listens* on TCP 6053, and Home Assistant *connects to it*. An external URL is what lets browsers and phones reach HA from outside — it is the opposite direction from what a remote device needs. Put the board on someone else's Wi-Fi and HA simply has nothing it can reach.

Three ways to actually do it, best first.

### 1. WireGuard on the device

ESPHome has a [`wireguard:`](https://esphome.io/components/wireguard/) component for ESP32. The board dials into your VPN, gets an address on it, and from HA's point of view is local again — the native API and everything else in this config keep working untouched.

```yaml
time:
  - platform: sntp     # REQUIRED — see below
    id: sntp_time

wireguard:
  address: 10.44.0.100
  netmask: 255.255.255.0
  private_key: !secret wg_private_key
  peer_endpoint: vpn.example.com
  peer_public_key: !secret wg_peer_public_key
  peer_port: 51820
  peer_allowed_ips:
    - 10.44.0.0/24
  peer_persistent_keepalive: 25s
  require_connection_to_proceed: true
```

Three things that catch people:

- **You must add an SNTP time source, and must _not_ use the `homeassistant` time platform.** WireGuard needs a synchronised clock to do its handshake, and getting the time from HA over the tunnel you have not established yet is a circular dependency that never resolves.
- **Auto-discovery usually fails.** Add the device manually in HA using its VPN address.
- **The ESP cannot set static routes.** `peer_allowed_ips` filters tunnel traffic, but actual routing follows `address`/`netmask` — outgoing connections only reach networks inside that range.

Only one peer is supported, which is fine for this use case.

### 2. MQTT

Swap the native API for [`mqtt:`](https://esphome.io/components/mqtt/). The device connects *out* to a broker, so it works from behind any NAT with no inbound access at all. HA subscribes to the same broker.

```yaml
mqtt:
  broker: mqtt.example.com
  port: 8883
  username: !secret mqtt_username
  password: !secret mqtt_password
  discovery: true
  certificate_authority: |
    -----BEGIN CERTIFICATE-----
    ... your broker's CA, in PEM ...
    -----END CERTIFICATE-----
```

**Delete the `api:` block when you do this.** If MQTT is enabled and the native API is configured but unused, the device reboots every 15 minutes waiting for an API connection that never arrives. Removing `api:` (or setting `reboot_timeout: 0s` on it) is the fix. This is the single most common way an otherwise-correct MQTT config goes wrong.

If you expose a broker to the internet, use TLS and real credentials. The CA block above is a placeholder — `esphome config` does not parse it, so a malformed certificate will validate happily and then fail on the device.

You also lose OTA-from-HA convenience and some native-API niceties.

### 3. Port-forward to the device

Forward 6053 from the remote site's router to the board. It works, and the API's Noise encryption means the traffic is protected — but you are exposing an IoT device's control port to the internet, and it is the option with the least margin for error. Prefer 1 or 2.

### Not really an option: pushing to the external URL

You *can* use `http_request` to POST sensor values to HA's REST API at its external URL with a long-lived access token. That genuinely uses the external URL — but it is one-way. Sensors would report; the ring would not be controllable from HA. Fine for a remote telemetry beacon, wrong for this board.

### Will it fit?

The base firmware uses 34% of RAM. WireGuard and MQTT-over-TLS both want a meaningful chunk on top — TLS handshakes are the expensive part — and the ESP32-C3-MINI-1 has no PSRAM to fall back on. There is headroom, but if you add either, check the build output rather than assuming.

Both variants above were validated with `esphome config` against ESPHome 2026.8.2. Neither has been run on hardware.


## Trade-off versus the Arduino sketch

ESPHome is declarative. Anything reactive and continuous — the tilt-to-LED mapping, brightness following ambient light — becomes a lambda, which is a worse place to teach from than [`test_sensors.ino`](../test_sensors/test_sensors.ino). Use ESPHome when you want the board to be a *device*; use the Arduino sketch when you want it to be an *example*.
