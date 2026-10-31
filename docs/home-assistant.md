# Home Assistant integration & operating modes

This fork adds native Home Assistant (HA) control on top of the original
printer‑driven behaviour, so the LED strip can be driven by the printer, by
Home Assistant, or both at once.

It works by opening a **second MQTT connection** (separate from the encrypted
connection to the printer) to your Home Assistant MQTT broker (Mosquitto) and
publishing **MQTT Discovery** entities. Nothing about the existing printer
connection changes.

> Requirement: you need an MQTT broker in Home Assistant (the *Mosquitto broker*
> add‑on + the *MQTT* integration). If you don't have one, set it up first.

## What appears in Home Assistant

Once enabled, three entities are auto‑created under a single device
(`BLLED <name>`):

| Entity | Type | Purpose |
| --- | --- | --- |
| `light.blled_led` (≈) | Light (RGB + brightness) | Turn the strip on/off, set colour & brightness |
| `select.blled_mode` | Select | Switch operating mode: `Printer` / `HomeAssistant` / `Hybrid` |
| `switch.blled_enable` | Switch | Master enable. When **off** the strip is forced fully off |

(Exact entity IDs include the device MAC, e.g. `light.blled_a1b2c3d4e5f6_led`.)

## Operating modes

Set the mode either on the device web page (**Home Assistant & Mode** section)
or from the HA `select` entity.

- **Printer only** – original behaviour. The strip is driven purely by printer
  state; Home Assistant light commands are ignored (the entities still exist but
  don't change the strip).
- **Home Assistant only** – the printer state is ignored. The strip is whatever
  the HA light entity says (on/off, colour, brightness). This state is persisted,
  so it is restored after a reboot.
- **Hybrid** *(default)* – the printer drives the strip as normal, **but** any
  Home Assistant light command (colour, brightness, or off) takes over. The HA
  override is *sticky*: it stays in effect while the printer is idle/finished/off,
  and is automatically released (printer reclaims the strip) when any of these
  happen: a **new print starts**, the **printer's chamber light is toggled**
  (the physical button or the Bambu app), or you switch the mode to *Printer only*.

The **master enable** switch sits above all modes: turning it off forces the
strip off in every mode, turning it on returns control to the selected mode.

## Printer‑offline behaviour (no more "stuck on")

Previously, when the printer was powered off and the controller couldn't
connect, the strip could stay lit on the last colour (e.g. the orange/yellow
connecting colour). Now, under **Printer Offline Behaviour**:

- **Dim when unreachable** *(default on)* – after a configurable timeout
  (`Dim after`, default 60 s) the strip drops to a low‑power dim indicator
  (`Dim brightness`, default 5 %). The firmware keeps retrying the printer
  connection in the background and returns to normal automatically once the
  printer is back.
- Set `Dim brightness` to `0` for fully off, or turn the toggle off to keep the
  legacy "off after timeout" behaviour.

To turn the LEDs **completely** off whenever the printer is off, use Home
Assistant — see the automation below.

## Setup steps

1. Flash/build this firmware (`pio run`, then upload + `pio run -t uploadfs` is
   not required — assets are embedded).
2. Open the BLLED web page → **Home Assistant & Mode**.
3. Tick **Enable Home Assistant integration**, enter your broker **host**,
   **port** (1883 by default), and **username/password** if your broker requires
   them. Save — the device reboots to connect.
   Once saved, the broker password is never shown again: leave the field blank
   to keep it, or clear the username to remove both.
4. In Home Assistant, the device + 3 entities appear automatically under
   *Settings → Devices & Services → MQTT*.

### Encrypting the broker connection (TLS)

By default the broker connection is plain MQTT, so the broker username and
password cross your network unencrypted. If your broker has a TLS listener
(Mosquitto usually uses port 8883):

1. Switch on **Use TLS**. The port changes to 8883 unless you set a custom one.
2. Optionally paste the **CA certificate** (PEM) that signed the broker's
   certificate: your own CA for a self-signed setup, or the public root (such
   as ISRG Root X1 for Let's Encrypt). The **Broker Host** must then be a
   name listed in the broker's certificate, such as `homeassistant.local`.
   Don't use an IP address here: the controller can't match a certificate to
   an IP address, even one the certificate lists, so the certificate check
   fails.
3. Save. The device reboots and connects over TLS.

Without a CA certificate the connection is still encrypted, but the controller
doesn't check that it is talking to your broker, so a device on your network
pretending to be the broker could still collect the password. If the
connection fails, the Web Serial log shows the TLS error (for example, an
untrusted certificate or a host name mismatch).

TLS costs memory: each TLS connection holds about 33 kB for as long as it is
open, and with TLS on, the printer and the broker connections each have one.
The **Diagnostics** panel shows free memory and the largest free block. If the
controller runs short, the log says it dropped printer reports; X1-series
printers send the largest reports. Switching off **Use TLS** frees that memory.

## Example automation: fully disable LEDs when the printer is off

This uses the **enable** switch so the strip is forced off regardless of mode.
Replace the entity IDs with yours, and use whatever tells HA the printer is off
(a smart plug, the Bambu integration's online sensor, etc.).

```yaml
automation:
  - alias: "BLLED off when printer powered down"
    trigger:
      - platform: state
        entity_id: switch.printer_smart_plug   # your printer power source
        to: "off"
        for: "00:01:00"
    action:
      - service: switch.turn_off
        target:
          entity_id: switch.blled_enable

  - alias: "BLLED on when printer powered up"
    trigger:
      - platform: state
        entity_id: switch.printer_smart_plug
        to: "on"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.blled_enable
```

In **Hybrid** mode you can achieve the same with the light entity
(`light.turn_off`) instead of the enable switch; the difference is that the
enable switch is absolute (beats every mode and override), while a light‑off
command is a normal Hybrid override that a new print would clear.

## Finding the web interface (mDNS) & opening it from Home Assistant

- **mDNS / `.local`** – the device advertises its web UI over mDNS, so you can
  reach it at `http://blled.local`. If you change the device name on the setup
  page, the address follows it (e.g. name `printer-leds` → `http://printer-leds.local`).
  The HTTP service is advertised under `_http._tcp`, so network/HA discovery tools
  also see it.
- **Open from Home Assistant** – the HA device page shows a **Visit device** link
  (the device's `configuration_url`) pointing at `http://<name>.local`. Open it
  from *Settings → Devices & Services → MQTT → BLLED → Visit device* (or the link
  on the device page) to jump straight to the web interface.

> Note: `.local` resolution uses mDNS/Bonjour, which works on macOS, iOS, Windows
> and most Linux desktops. The HA "Visit device" link opens in *your* browser, so
> it resolves on your computer even if Home Assistant itself runs in a container
> that can't resolve `.local`.

## MQTT topics (for reference / manual config)

Base topic: `blled/blled_<mac>`

| Function | Topic | Payload |
| --- | --- | --- |
| Availability (LWT) | `blled/blled_<mac>/status` | `online` / `offline` |
| Light command | `blled/blled_<mac>/light/set` | JSON: `{"state":"ON","brightness":200,"color":{"r":255,"g":0,"b":0}}` |
| Light state | `blled/blled_<mac>/light/state` | same JSON schema |
| Mode command/state | `blled/blled_<mac>/mode/{set,state}` | `Printer` / `HomeAssistant` / `Hybrid` |
| Enable command/state | `blled/blled_<mac>/enable/{set,state}` | `ON` / `OFF` |

Discovery configs are published retained under `homeassistant/light/…`,
`homeassistant/select/…`, and `homeassistant/switch/…`.
