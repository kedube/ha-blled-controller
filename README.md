## Home Assistant BL Led Controller

The BL Led Controller is an ESP32 based device that connects to your Bambulab X1,X1C,P1P Or P1S and controls the LED strip based on the state of the printer.

> **This fork** ([kedube/ha-blled-controller](https://github.com/kedube/ha-blled-controller)) is based on [plugowski/HomeAssistant_BLLEDController](https://github.com/plugowski/HomeAssistant_BLLEDController), Pawel Lugowski's fork of DutchDeveloper's original firmware. The **Home Assistant** integration and independent control of the LED strip are plugowski's work. This fork adds security fixes, an encrypted connection to the Home Assistant broker and reliability fixes on top; see [Changes in this fork](#changes-in-this-fork). See the [Home Assistant guide](docs/home-assistant.md) for full details.

### Home Assistant features (from plugowski's fork)

- **Home Assistant via MQTT Discovery** – the device opens a second MQTT connection to your Home Assistant broker (Mosquitto) and auto-creates entities with no YAML: a **light** (RGB **and** warm/cold-white colour temperature + brightness, with smooth transitions), a **mode select**, and a master **enable switch**.
- **Three operating modes** – *Printer only* (original behaviour), *Home Assistant only* (HA fully controls the strip), and *Hybrid* (the printer drives the strip but Home Assistant can override colour/temperature or force it off; the override is released when a print starts or the printer's chamber light is toggled).
- **Live state mirroring** – the HA light always reflects what is physically lit, whether set by the printer (printing, errors, etc.) or by Home Assistant, so you can always see and override the current state.
- **Master "Enable BLLED Light Control"** – a top-level switch (web UI **and** HA) that forces the strip fully off in any mode, e.g. via an automation when the printer is powered down. It applies instantly and stays in sync both ways.
- **Smarter offline behaviour** – instead of getting stuck on the last colour when the printer is unreachable, the strip dims after a configurable timeout and keeps retrying the connection in the background.
- **mDNS** – reach the web UI at `http://blled.local` (follows the device name), and a "Visit device" link to the web UI appears on the Home Assistant device page.

![home assistant setup](./docs/home_assistant_setup.jpg)
![home assistant preview](./docs/home_assistant_preview.jpg)

### Changes in this fork

Compared with plugowski's fork (2.4.2), version 2.4.3 makes these changes.

**Security**

- **Upstream security fixes** – brings in [DutchDevelop/BLLEDController#83](https://github.com/DutchDevelop/BLLEDController/pull/83) by brossow, which plugowski's fork doesn't include:
  - With a web login set, every page and setting needs it, including firmware updates, config restore and the Web Serial log.
  - Saved passwords and the printer access code are never sent back to the browser.
  - Other websites can't change settings or flash firmware through your browser.
  - Factory reset asks you to confirm.
  - A nearby Wi-Fi network's name can't inject script into the setup page.
  - Another device can't redirect the printer login to itself by answering printer discovery.
- **The same protections for the Home Assistant settings** – changes to the Home Assistant connection, the network settings and the enable switch need the login and can't come from another website. The broker password is no longer shown on the setup page; leave the field blank to keep it.
- **Encrypted Home Assistant connection (TLS)** – optional. With your CA certificate, the controller also checks that it is talking to your broker. See [Encrypting the broker connection](docs/home-assistant.md#encrypting-the-broker-connection-tls).
- **Safer config restore** – the file must be a BLLED config (16 KB at most); otherwise the current settings stay as they are.

**Reliability**

- **Larger firmware slots** – 1.875 MB per slot instead of 1.25 MB, so new features keep fitting (see the flash layout note below).
- **Clear firmware update errors** – if you choose the wrong file or the firmware doesn't fit, the update page says so and the current firmware keeps running.
- **No hang when mDNS fails** – the controller logs the error and keeps running instead of stopping at boot.
- **Memory in Diagnostics** – the Diagnostics panel shows free memory and the lowest it has been since boot.
- **Repeatable builds** – the web server and Web Serial libraries are pinned to exact versions.

**Docs and builds**

- The [manual](docs/manual.md) says that passwords are stored unencrypted on the device and that config backups contain them in plain text.
- GitHub Actions builds the firmware on every push and pull request.

### Flashing and Setup

> **For this fork:** download the firmware from the latest run of the [Build firmware workflow](https://github.com/kedube/ha-blled-controller/actions/workflows/build.yml) (you need to be signed in to GitHub; each download expires 90 days after its build), or build it yourself (see below). The download contains two files. If the controller already runs BLLED firmware, install the `.bin.ota` file from its **Firmware Update** page. For a new ESP32, flash the full `.bin` over USB, for example with `esptool.py write_flash 0x0 BLLC_V<version>.bin`.
>
> plugowski's builds at [tech.lugowski.dev/firmware](https://tech.lugowski.dev/firmware/) don't include this fork's changes. The original DutchDeveloper flasher below installs the upstream firmware *without* the Home Assistant features.

1. go to the [Web Flasher](https://dutchdevelop.github.io/blledsetup/) (Or [here](https://softwarecrash.github.io/BLLED-Flasher/) for Nightly and Dev builds)
2. connect your ESP32
3. Select the Firmware build you want
4. Click on Flash
5. Search and connect to a WiFi Hotspot called "BLLED-AP"
6. Surf to http://192.168.4.1
7. Select your WiFi and Enter passwort, Optional enter all the Printer informations
8. enjoy :)

> **Flash layout (2.4.3 and later):** builds use a larger layout with 1.875 MB per firmware slot instead of 1.25 MB, so new features keep fitting. A controller updated over the air keeps its old layout, which still works until a firmware outgrows it; the update page then says so. To switch, flash the full `.bin` once over USB. This erases the settings, so download a backup from **Backup & Restore** first and restore it afterwards.

![main setup_LED](https://github.com/user-attachments/assets/52778d28-98cd-496e-bfce-aca49cd4de89)
![main_setup_printer](https://github.com/user-attachments/assets/1969e4bd-0887-4a0c-a965-8ec575e23396)
![main_setup_error](https://github.com/user-attachments/assets/b026260c-5c33-4f5c-922a-fcc601cb9be1)
![main_setup_debug](https://github.com/user-attachments/assets/6d5a6e48-b052-4305-99f5-5d9c906eb8c1)
![main_setup_navigation](https://github.com/user-attachments/assets/78041d10-2edb-42e2-b600-6cde735d2d1f)
![setup_wifi](https://github.com/user-attachments/assets/4ef7631c-38f9-4424-ad88-72a335286c27)
![setup_wifi_printer](https://github.com/user-attachments/assets/da354544-710d-4e10-9dff-fc2cc1951857)
![ota](https://github.com/user-attachments/assets/7f223361-f811-4ff4-b3fc-45b2a1222c07)
![backup_restore](https://github.com/user-attachments/assets/b21899ba-c97f-48b6-8bc3-a87797029d14)
![navigation](https://github.com/user-attachments/assets/027fe724-ffc6-453b-ba73-c82c1ddb012e)





- Connects to Bambulab X1,X1C,P1P Or P1S
- Controls LED strip based on printer state

### Development Environment

To contribute to the BL Led Controller project, you'll need the following tools:

### Tools & Libraries Used

- [Visual Studio Code](https://code.visualstudio.com/): A lightweight and powerful source code editor.
- [PlatformIO](https://platformio.org/): An open-source ecosystem for IoT development.
- [Python](https://www.python.org/): A programming language used for scripting and automation.
- [qpdf](https://qpdf.sourceforge.io/): A command-line tool and library of compression tools (`gzip`)

### Building and Running the Project
1. Clone the repository to your local machine: `git clone https://github.com/kedube/ha-blled-controller.git`
2. Open the project folder in Visual Studio Code.
3. Ensure that PlatformIO is installed and configured in your Visual Studio Code environment.
4. Connect your BLLED device ESP32 to your computer.
6. Build the project by clicking on the PlatformIO icon in the sidebar and selecting "Build" from the available options.
7. Once the build process is complete, upload the firmware to your device using the "Upload" option in PlatformIO.
8. After uploading the firmware, your BL Led Controller device should be ready to use.

### Setup Instructions
Once you have uploaded the firmware to your device, please visit the [dutchdevelop.com/bl-led-controller](https://dutchdevelop.com/bl-led-controller) website for detailed setup instructions.


### Development Notes

#### Generating .h Files for Compressed HTML

In embedded applications, HTML content is efficiently stored in PROGMEM memory. To achieve this, .h files are generated from compressed HTML files for webpages (i.e., `src/www/setuppage.html`) that are run on the device.

- The `compress_html.py` is used to compress HTML files and generate corresponding .h files and is integrated into the build process and executed as a pre-build step in `platform.ini`
- The generated .h files should not be checked into git (see `.gitignore`)

### License

The BL Led Controller is released under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0) license. See the [LICENSE](https://github.com/DutchDevelop/BLLEDController/blob/main/LICENSE) file for more details.

### Credits
- **[DutchDeveloper](https://dutchdevelop.com/)**: Lead programmer / original author
- **[Modbot](https://github.com/Modbot)**: Tester for X1C, P1P & P1S
- **[xps3riments](https://github.com/xps3riments)**: Inspiration for the foundation of the code
- **[longrackslabs](https://github.com/longrackslabs)**: Build process, documentation, developer & community support
- **[SoftWareCrash](https://github.com/softwarecrash)**: Any small unimportant changes
- **[plugowski (Pawel Lugowski)](https://github.com/plugowski)**: The original Home Assistant fork ([plugowski/HomeAssistant_BLLEDController](https://github.com/plugowski/HomeAssistant_BLLEDController)) – Home Assistant MQTT Discovery integration, operating modes (Printer / HA / Hybrid), colour-temperature (warm/cold white) control, live state mirroring, master enable switch, offline-dim behaviour, mDNS and web UI improvements. Builds: [tech.lugowski.dev/firmware](https://tech.lugowski.dev/firmware/)
- **[brossow](https://github.com/brossow)**: Security fixes for login, secrets and cross-site requests ([DutchDevelop/BLLEDController#83](https://github.com/DutchDevelop/BLLEDController/pull/83))
- **[kedube (Katherine Dubé)](https://github.com/kedube)**: This fork – security fixes for the Home Assistant features, TLS for the broker connection, safer config restore and firmware updates, the larger flash layout and CI builds

### Author

This project was created by [DutchDeveloper](https://dutchdevelop.com/).

The Home Assistant integration was created by [Pawel Lugowski](https://tech.lugowski.dev/) ([plugowski](https://github.com/plugowski)) in [plugowski/HomeAssistant_BLLEDController](https://github.com/plugowski/HomeAssistant_BLLEDController).

This fork ([kedube/ha-blled-controller](https://github.com/kedube/ha-blled-controller)) is maintained by [Katherine Dubé](https://github.com/kedube).
