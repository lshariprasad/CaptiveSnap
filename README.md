
<div align="center">

# 📡 CaptiveSnap++
### Ultimate Rogue Wi‑Fi Toolkit for ESP8266

![Version](https://img.shields.io/badge/version-3.0-red?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-blue?style=for-the-badge)
![Platform](https://img.shields.io/badge/platform-ESP8266-green?style=for-the-badge)

**For authorized security research & education only. Do not use against any system or user without explicit written consent.**

</div>

---

## 📖 Table of Contents
- [Overview](#-overview)
- [🚀 Features](#-features)
- [🧰 Hardware Requirements](#-hardware-requirements)
- [📦 Installation](#-installation)
- [🕹️ Usage](#️-usage)
- [🧩 Customisation](#-customisation)
- [📁 Project Structure](#-project-structure)
- [⚠️ Legal & Ethical Disclaimer](#️-legal--ethical-disclaimer)
- [🤝 Contributing](#-contributing)

---

## 📝 Overview
**CaptiveSnap++** is a powerful, feature‑rich captive portal toolkit for the ESP8266 (NodeMCU v3). It simulates a legitimate Wi‑Fi network to demonstrate how easy it is to harvest credentials, capture selfies, exfiltrate files, log keystrokes, and gather device telemetry – all from an open access point. 

Built specifically for pentesters, red‑teamers, and security educators who need a realistic Wi‑Fi phishing platform to demonstrate physical and network security vulnerabilities.

---

## 🚀 Features

| Category | Capabilities |
| :--- | :--- |
| **Phishing Templates** | 5 ready‑to‑use login pages (Wi‑Fi, Google, Facebook, Microsoft, Apple) served from SPIFFS. |
| **Data Exfiltration** | WebSocket real‑time exfiltration of credentials, photos, file contents, keystrokes, and telemetry. |
| **Keylogger** | Injected JavaScript captures every keystroke on input fields before form submission. |
| **Camera Capture** | Automatically takes a selfie via the victim's webcam (front‑facing). |
| **Full‑Page Screenshot**| Captures the entire visible page using `html2canvas`. |
| **File Exfiltration** | Reads entire files (not just preview) – sent as Base64 via WebSocket. |
| **Device Fingerprinting**| Browser user‑agent, screen size, language, geolocation (with permission), battery level. |
| **Persistent Logging** | All captured data stored in SPIFFS (`/logs.json`), survives reboot. |
| **Attacker Dashboard** | Live WebSocket feed, photo gallery, log download (JSON/CSV). |
| **OTA Updates** | Upgrade firmware wirelessly via `/update` with basic auth. |
| **Self‑Defense** | Watchdog timer resets the ESP if the loop hangs; optional hidden SSID. |
| **Stealth Mode** | Hide SSID, change Wi‑Fi channel, disable beaconing when idle (configurable). |
| **Captive Portal** | DNS spoofing for all domains, answers all known OS connectivity checks. |
| **Configuration Page** | Change SSID/password via web interface (basic auth). |

---

## 🧰 Hardware Requirements
* **ESP8266 board** (e.g., NodeMCU v3, Wemos D1 Mini)
* **USB cable** for programming
* *Optional:* External antenna for better range

---

## 📦 Installation

### 1. Install Required Libraries
In the Arduino IDE, go to **Sketch → Include Library → Manage Libraries…** and install:
* `ESP8266WiFi`, `ESP8266WebServer`, `DNSServer`, `WebSockets`, `ESP8266mDNS` (built‑in)
* `ESP8266HTTPUpdateServer` (built‑in)
* `ArduinoJson` by Benoit Blanchon (v6)
* `ESP8266Watchdog` by earlephilhower

### 2. Prepare SPIFFS Files
Create a folder named `data` inside your sketch directory. Place the following HTML files inside it:
* `wifi.html`, `google.html`, `facebook.html`, `microsoft.html`, `apple.html`
* `activate.html`, `viewer.html`

> **Note:** All HTML pages must include JavaScript that connects to the WebSocket server (`ws://192.168.1.1:81`) and sends exfiltrated data. Example snippets are provided in the `docs/` folder of this repository.

### 3. Upload SPIFFS
1. Install the **ESP8266 Sketch Data Upload tool** (via Tools → Board → Boards Manager → search "ESP8266").
2. Once installed, go to **Tools → ESP8266 Sketch Data Upload**.
3. This will upload all files from the `data` folder to the ESP's SPIFFS.

### 4. Compile & Upload the Sketch
1. Open `CaptiveSnap++.ino` in the Arduino IDE.
2. Select your board (e.g., `NodeMCU 1.0 (ESP‑12E Module)`).
3. Set the correct COM port.
4. Click **Upload**.

### 5. Open the Serial Monitor
Set baud rate to `115200`. You'll see the AP IP address (default: `192.168.1.1`) and confirmation that the server is running.

---

## 🕹️ Usage

### For the Attacker
1. Power on the ESP8266 – it creates an open Wi‑Fi network named `SIMATS` (or hidden if configured).
2. Connect your attacking device to this AP (or use a second Wi‑Fi interface).
3. Open a browser and navigate to:

| URL | Purpose |
| :--- | :--- |
| `http://192.168.1.1/` | Login page (template selectable via `?template=1..5`) |
| `http://192.168.1.1/view` | Attacker Dashboard – live WebSocket feed, photo gallery, logs |
| `http://192.168.1.1/logs` | View captured logs (JSON) or download CSV (`?download=1`) |
| `http://192.168.1.1/photos` | Gallery of all captured selfies/screenshots |
| `http://192.168.1.1/config` | Change AP settings (User: `admin` / Pass: `captivesnap`) |
| `http://192.168.1.1/update` | OTA firmware update (User: `admin` / Pass: `update`) |

### For the Victim
1. Any device that joins the open AP will be captive‑portaled – every DNS request is redirected to the ESP's IP.
2. The victim is shown the selected phishing login page.
3. When they submit credentials, they are quietly redirected to `https://www.google.com` to avoid suspicion.
4. The `activate.html` page is loaded (via redirect) which:
   * Requests camera permission (for selfie).
   * Prompts for a file selection (exfiltrates the chosen file).
   * Logs keystrokes and sends telemetry.
   * Takes a full‑page screenshot (if `html2canvas` is included).
5. All exfiltrated data is sent live to the WebSocket and stored persistently.

---

## 🧩 Customisation

### Change the Default Template
Append `?template=1|2|3|4|5` to the root URL:
* `1` – Wi‑Fi login
* `2` – Google
* `3` – Facebook
* `4` – Microsoft
* `5` – Apple

### Modify AP Settings
Edit the constants at the top of the sketch:
* `AP_SSID` – visible SSID
* `HIDDEN_SSID` – set to `1` to hide the network
* `AP_PASSWORD` – set a WPA2 password (empty = open)
* `AP_CHANNEL` – Wi‑Fi channel (1‑11)

### Add Your Own Phishing Pages
Simply add a new `.html` file to the `data` folder and update the `handleRoot()` function to serve it based on a new template ID.

---

## 📁 Project Structure
```text
CaptiveSnap++/
├── CaptiveSnap++.ino    # Main sketch (≈1050 lines)
├── data/                # SPIFFS content (upload via ESP8266 Sketch Data Upload)
│   ├── wifi.html
│   ├── google.html
│   ├── facebook.html
│   ├── microsoft.html
│   ├── apple.html
│   ├── activate.html
│   └── viewer.html
├── docs/                # Additional documentation (how to build templates)
└── README.md            # This file
```

---

## ⚠️ Legal & Ethical Disclaimer

**CaptiveSnap++ is a powerful tool that can be misused. It is intended solely for:**
* Authorised penetration testing engagements (with written permission).
* Cybersecurity training and workshops in controlled environments.
* Educational demonstrations to raise awareness about Wi‑Fi attacks.

**By using this software, you agree that:**
* You are fully responsible for your actions.
* You will not deploy this against any network, system, or individual without explicit consent.
* The author(s) assume no liability for any damage, loss, or legal consequences arising from misuse.

*If you are unsure about the legality of your actions, do not use this tool.*

---

## 🤝 Contributing
Found a bug? Have an idea for a new feature? Pull requests are welcome! Please open an issue first to discuss major changes.

### 🙏 Acknowledgements
* The ESP8266 community for the incredible libraries.
* All security researchers who push the boundaries of education and awareness.

> Stay ethical, stay legal, and stay safe. 🛡️
