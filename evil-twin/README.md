# Ghost — Evil Twin Module

Standalone ESP32 module that clones a real WiFi network, deauthenticates clients from it, and traps them on a fake AP with DNS poisoning. Built step by step on top of ESP-IDF.

> **For authorized security testing and educational use only.**

---

## What it does

When you send `CMD:TWIN_START:<ssid>`:

1. ESP32 scans for the target network and grabs its BSSID and channel
2. Starts a fake AP with the exact same SSID on the exact same channel
3. Starts a DNS server — every domain query from connected clients resolves to the ESP32
4. Starts sending deauth frames to the real router — clients get kicked off and auto-reconnect to the fake AP

### What's coming next
- **Step 3:** HTTP captive portal — fake login page served automatically to connected clients
- **Step 5:** Deauth loop — re-deauth clients the moment they try to escape back to the real network

---

## Project Structure

```
evil-twin/
├── CMakeLists.txt
├── README.md
├── Twin.py
└── main/
    ├── CMakeLists.txt
    └── evil_twin.c
```

---

## Build & Flash

Activate ESP-IDF (CMD, not PowerShell):

```
C:\esp32\v6.0.1\esp-idf\export.bat
```

From the `evil-twin/` folder:

```
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash
```

> Hold **BOOT** when **Connecting......** appears, release once flashing starts.

---

## Usage

```
python Twin.py
```

The script asks for an SSID, sends it to the ESP32, and streams all events to the console. Press **Ctrl+C** to stop everything cleanly.

### What you'll see
```
TWIN:SCANNING_FOR:HomeNetwork
TWIN:TARGET_FOUND BSSID=AA:BB:CC:DD:EE:FF CH=6
TWIN:AP_STARTED:HomeNetwork
DNS:STARTED
DEAUTH:STARTED BSSID=AA:BB:CC:DD:EE:FF CH=6
TWIN:CLIENT_JOINED:11:22:33:44:55:66
```

---

## UART Protocol

| Command | Description |
|---|---|
| `CMD:TWIN_START:<ssid>` | Scan, clone, deauth and trap clients from the given network |
| `CMD:TWIN_STOP` | Stop everything — AP, DNS, deauth |

| Response | Description |
|---|---|
| `GHOST:READY` | Firmware booted |
| `TWIN:SCANNING_FOR:<ssid>` | Looking up target network |
| `TWIN:TARGET_FOUND BSSID=... CH=...` | Target located |
| `TWIN:AP_STARTED:<ssid>` | Fake AP is live |
| `DNS:STARTED` | DNS server running |
| `DEAUTH:STARTED BSSID=... CH=...` | Deauth attack running |
| `TWIN:CLIENT_JOINED:<mac>` | A device connected to the fake AP |
| `TWIN:CLIENT_LEFT:<mac>` | A device disconnected |
| `DEAUTH:STOPPED` | Deauth stopped |
| `DNS:STOPPED` | DNS stopped |
| `TWIN:AP_STOPPED` | Everything shut down |
| `ERR:SSID_NOT_FOUND:<ssid>` | Target network not visible |
| `ERR:NO_SSID` | No SSID provided in command |
| `ERR:UNKNOWN` | Unrecognized command |

---

## Communication

All communication runs over USB — no external wiring needed. See the root README for details on the UART pinout if extending this project for use with external hardware.
