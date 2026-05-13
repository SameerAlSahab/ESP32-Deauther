

# Hydra-ESP

<div align="center">
<img src="resources/hydra_logo.png" alt="Hydra-ESP Logo" width="350"/>
</div>

[![Stars](https://img.shields.io/github/stars/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=yellow)](https://github.com/SameerAlSahab/ESP32-Deauther/stargazers)
[![Forks](https://img.shields.io/github/forks/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=orange)](https://github.com/SameerAlSahab/ESP32-Deauther/network/members)
[![Issues](https://img.shields.io/github/issues/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=red)](https://github.com/SameerAlSahab/ESP32-Deauther/issues)
[![Pull Requests](https://img.shields.io/github/issues-pr/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=purple)](https://github.com/SameerAlSahab/ESP32-Deauther/pulls)
[![License](https://img.shields.io/github/license/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=blue)](LICENSE)
[![Last Commit](https://img.shields.io/github/last-commit/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=brightgreen)](https://github.com/SameerAlSahab/ESP32-Deauther/commits)
[![Repo Size](https://img.shields.io/github/repo-size/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=informational)](https://github.com/SameerAlSahab/ESP32-Deauther)
[![Contributors](https://img.shields.io/github/contributors/SameerAlSahab/ESP32-Deauther?style=for-the-badge&color=pink)](https://github.com/SameerAlSahab/ESP32-Deauther/graphs/contributors)

A wireless security research firmware for the ESP32 microcontroller. Built on top of [risinek's](https://github.com/risinek/esp32-wifi-penetration-tool) original ESP32 Wi-Fi penetration tool foundation, ProjectHydraOS extends the original with a redesigned web interface, multi-target deauthentication, BLE attack capabilities, a deauth attack detector, optional OLED display support, and several additional attack modules.

---

## Hardware

**Required**

- ESP32 DevKit V1 or any board based on the ESP32 (Xtensa LX6 dual-core) SoC. The firmware is developed and tested on the standard 38-pin DevKit V1. Other ESP32 variants with the same chip such as the ESP32-WROOM-32 and ESP32-WROVER modules are expected to work. ESP32-S2, S3, C3, and other variants are not supported as they use different hardware radio architectures.

**Optional**

- SSD1306 OLED display (128x64, I2C). When connected, the display shows live attack timers, current attack status, menu navigation, captured passwords from the Evil Twin module, and device logs. The firmware auto-detects the display on boot. If no display is found, initialisation is skipped silently and all functionality remains available through the web interface.

---

## How It Works

The ESP32 runs its own Wi-Fi access point (management AP) on boot. You connect a phone or laptop to that AP and open `http://192.168.4.1` in a browser. The web interface is served from SPIFFS flash storage on the device. From there you can scan nearby networks, select targets, configure and launch attacks, monitor deauthentication activity, and change device credentials.

The management AP is temporarily disabled during some attacks that require exclusive use of the radio (Deauth, Evil Twin, Super Clone). For those attacks, you lose the web interface connection while the attack runs. A configurable timeout brings the device back automatically. Without a timeout set, a power cycle is required to stop the attack and restore access.

---
## Dependencies
 
| Library | License |
|---|---|
| [u8g2-hal-esp-idf](https://github.com/mkfrey/u8g2-hal-esp-idf) | See repo |
| [ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard) | See repo |
| [u8g2](https://github.com/olikraus/u8g2) | BSD 2-Clause |
| [esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) | Apache 2.0 |
 
---

## Attacks

### Deauthentication

Sends raw 802.11 deauthentication frames to disconnect clients from a target AP. Supports up to 16 simultaneous targets. Devices with 802.11w (Management Frame Protection) enabled may resist frame-injection deauth. See Super Clone for an approach that bypasses MFP.

**Methods:** Deauth frames only / Deauth + disassociation frames

---

### WPA Handshake Capture

Forces connected clients to re-authenticate by sending deauth frames, then captures the resulting WPA2 4-way handshake. The capture is saved as `.pcap` and `.hccapx` files, compatible with Hashcat and aircrack-ng for offline auditing against a wordlist.

A connected client must be present on the target network. The attack runs until a handshake is captured or stopped manually.

---

### Clientless PMKID Capture

Requests the PMKID from the first EAPOL-Key frame during association. Unlike handshake capture, no connected client is required — only the AP needs to be in range. The PMKID is derived from the PMK and both MAC addresses, and can be audited offline with Hashcat (hash mode 22000).

Works on most modern WPA2 access points. Some APs do not include the PMKID in their EAPOL frames.

---

### Beacon Spam

Floods the local radio environment with fake 802.11 beacon frames, each carrying a randomly generated SSID. This pollutes the Wi-Fi scan list on all nearby devices. Configurable number of fake networks (1–100).

---

### Ghost Mode (Probe Request Spam)

Many devices continuously broadcast probe requests for every saved Wi-Fi network. Ghost Mode listens for these probes, extracts the SSIDs, and begins advertising those exact network names. Devices attempt to connect to the ESP32 instead of their saved network.

---

### Evil Twin

Deploys an open (no password) clone of the selected AP using the same SSID. Simultaneously runs a deauthentication attack against the legitimate AP to force clients to disconnect. Clients searching for a network see the open clone and connect to it. They are redirected to a captive portal that requests the network password. The attack continues until a password submission is received and verified, at which point the result is logged and displayed.

The management web interface is unavailable during this attack. The device must be power cycled to stop it if no timeout is set.

---

### BSSID Clone (Twin Deauth)

Clones the target AP's SSID and BSSID onto the ESP32 on the same channel. Both the real AP and the clone now appear identical to clients. The conflicting presence causes clients to disconnect. Unlike raw frame injection deauth, this technique is effective on devices with 802.11w Management Frame Protection because it does not rely on sending unprotected management frames — it creates a legitimate-looking second AP.

---

### SSID Cloner

Make multiple clones with the same SSID names by adding some spaces with the name.

---
### BLE Spam

Broadcasts Bluetooth Low Energy advertisement packets that mimic Apple, Samsung, and Google device proximity pairing signals. iPhones, iPads, and Android devices display pairing popups for nearby audio devices, setup notifications for Apple TV, HomePod, Vision Pro, and others. The target device type is selectable. Random MAC address rotation is supported.

Supported targets include AirPods (all generations), AirPods Pro (all generations), AirPods Max, Beats products, Apple TV setup and pairing prompts, HomePod setup, Vision Pro, Galaxy Buds (all variants), Pixel Buds, and random selection modes.

---

### BT Payload 

Advertises the ESP32 as a Bluetooth HID keyboard under the name "Hydra-RandomNumber". When a Windows PC pairs with it, the firmware sends keystrokes to perform the payloads.

---

### Deauth Attack Detector

Puts the ESP32 into promiscuous 802.11 monitor mode and inspects raw management frames. Alerts when more than 10 deauthentication frames from a single BSSID are observed within one second — the signature pattern of a deauthentication attack. Broadcast deauth frames (source `00:00:00:00:00:00`) are also flagged. Results are shown in a live log table in the web interface.

---

## Web Interface

Accessible at `http://192.168.4.1` after connecting to the device's management AP.

- **Scan** — scans nearby networks and displays SSID, BSSID, and signal strength. Tap a row to select it as an attack target.
- **Attack** — configure and launch any of the above attacks. Shows live status, elapsed time, and captured results.
- **Detector** — start/stop the deauth monitor and view the alert log.
- **Settings** — change the management AP SSID and password. The device reboots on save.
- **About** — firmware version, credits, and legal notice.

---

## Default Credentials

| Field    | Default         |
|----------|-----------------|
| SSID     | `hydra`  |
| Password | `notforfun`  |
| Web UI   | `192.168.4.1`   |

Credentials can be changed from the Settings tab and are persisted to NVS flash across reboots.

---

## Credits

| Role | Name |
|---|---|
| Lead Developer | Sameer Al Sahab |
| Original Codebase | [risinek](https://github.com/risinek/esp32-wifi-penetration-tool) |
| Inspiration | [spacehuhn](https://github.com/SpacehuhnTech/esp8266_deauther) |
| BLE Spam Code | [justcallmekoko and ckcr4lyf](https://github.com/ckcr4lyf/EvilAppleJuice-ESP32)

---

## Legal

This firmware is an educational security research tool. Use it only on networks and devices you own or have received explicit written authorisation to test. Unauthorised use is illegal under the Computer Fraud and Abuse Act (US), Computer Misuse Act (UK), IT Act 2000 (Bangladesh/India), and equivalent legislation in most other jurisdictions.

The authors accept no liability for misuse. You are solely responsible for your actions.
