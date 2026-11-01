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

Once enabled, these entities are auto‑created under a single device
(`BLLED <name>`). Home Assistant lists the configuration and diagnostic ones in
their own sections on the device page and leaves them off auto‑generated
dashboards.

**Controls and status**

| Entity | Type | Purpose |
| --- | --- | --- |
| LED (`light.blled_led` ≈) | Light (RGB + colour temperature + brightness) | Turn the strip on/off, set colour & brightness. In Hybrid mode this starts an override |
| Mode | Select | Switch operating mode: `Printer` / `HomeAssistant` / `Hybrid` |
| Enable | Switch | Master enable. When **off** the strip is forced fully off |
| Printer brightness | Number (1–100 %) | The setup page's **Brightness**: how bright the printer‑driven colours are. A change redraws the current colour and doesn't wake LEDs the inactivity timeout turned off |
| Maintenance mode | Switch | White light whatever the printer is doing, even when it's off. Turning it on also turns off RGB cycle, colour test and WiFi‑strength display, and ends a Hybrid override |
| Resume printer control | Button | Ends a Hybrid override now, instead of waiting for a print to start |
| Wake LEDs | Button | Does what opening the door does: ends the finish indication and restarts the inactivity timer. For printers without a door sensor |
| Home Assistant override | Binary sensor | On while a Hybrid override is in effect (a printer error still shows over it) |
| Printer connection | Binary sensor (connectivity) | Whether BLLED is connected to the printer |
| Printer error | Binary sensor (problem) | On while the printer reports an error BLLED shows in an error colour: filament runout, front cover removed, nozzle or bed temperature failure, or a Serious or Fatal HMS code. This works even with error detection off. The `hms_code` and `hms_severity` attributes give the HMS code the printer is reporting, in the form the HMS ignore list uses, or none |

Printer brightness and maintenance mode only change the strip in *Printer* and
*Hybrid* modes. In *Home Assistant* mode the light entity's own brightness
applies.

**Configuration** (the setup page's LED settings)

| Entity | Type | Setup page setting |
| --- | --- | --- |
| Replicate chamber light | Switch | Replicate State |
| Control chamber light | Switch | Control Chamber Light |
| Error detection | Switch | Error Detection |
| Finish indication | Switch | Finish Indication |
| Finish indication exit | Select: `Door` / `Timer` | Exit Finish after Door Action / Exit Finish after … |
| Finish indication timer | Number (minutes) | Exit Finish after … Min. |
| Inactivity timeout | Switch | Inactivity Timeout |
| Inactivity timeout length | Number (minutes) | Inactivity Timeout (Minutes) |
| Offline dim | Switch | Dim (instead of staying lit) when printer is unreachable |
| Offline dim delay | Number (seconds) | Dim after (sec) |
| Offline dim brightness | Number (%) | Dim brightness (%) |

This section also has a **Restart** button that restarts the controller.

**Diagnostic**

| Entity | Type | Purpose |
| --- | --- | --- |
| LED source | Sensor | What is driving the strip, in the same words as the log, e.g. `Stage 0, PRINTING - gcodeState RUNNING`, `Idle Timeout - OFF` or `Home Assistant override` |
| WiFi signal | Sensor (dBm) | Signal strength, updated every minute |
| Free memory, Largest free memory block | Sensors (bytes) | The memory figures from the Diagnostics panel, updated every minute |

(Exact entity IDs follow the device and entity names, e.g. `light.blled_led`
and `number.blled_printer_brightness`.)

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
  (the physical button or the Bambu app), you press **Resume printer control**,
  you turn on **Maintenance mode**, or you switch the mode to *Printer only*.

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
4. In Home Assistant, the device and its entities appear automatically under
   *Settings → Devices & Services → MQTT*.

### Protecting the broker password

The broker connection is plain MQTT, so the broker username and password cross
your network unencrypted. TLS isn't offered: the usual BLLED board (an ESP32
without PSRAM) doesn't have the memory for a second TLS connection next to the
printer's, and with printers that send large status reports, trying it stops
the controller working.

To limit what a captured password is worth, give BLLED its own broker account
instead of one shared with other devices. If your broker supports access
control lists (the Mosquitto add-on does, through its `customize` option),
allow that account only `blled/#` and publishing discovery messages under
`homeassistant/`.

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
| Other commands | `blled/blled_<mac>/<key>/set` | Switches `ON` / `OFF`, numbers a value, `finish_exit` `Door` / `Timer`, buttons any payload |
| Settings and status | `blled/blled_<mac>/settings/state` | JSON with one field per key, plus `override`, `printer`, `printer_error`, `hms_code`, `hms_severity`, `led_source`. Sent when something changes |
| Diagnostics | `blled/blled_<mac>/diagnostics/state` | JSON: `rssi`, `free_memory`, `largest_block`. Sent every minute |

The keys are `brightness`, `maintenance`, `replicate`, `chamber_light`,
`error_detection`, `finish`, `finish_exit`, `finish_minutes`, `inactivity`,
`inactivity_minutes`, `offline_dim`, `offline_dim_delay`,
`offline_dim_brightness`, and the buttons `resume`, `wake` and `restart`.

Discovery configs are published retained under `homeassistant/<component>/…`
for the `light`, `select`, `switch`, `number`, `button`, `binary_sensor` and
`sensor` components.
