# Ghost — Evil Twin Module (Step 1: Fake AP)

Standalone ESP32 module that creates a fake WiFi access point with any SSID you choose. This is the foundation of the Evil Twin attack — built and verified step by step.

> **For authorized security testing and educational use only.**

---

## Current step: Fake AP

The ESP32 broadcasts an open WiFi network with whatever name you send over UART. Devices nearby will see it and can connect to it. The ESP32 reports every client that joins or leaves over UART.

### What's coming next (built on top of this)
- **Step 2:** DNS server — redirects all domain lookups to the ESP32
- **Step 3:** Captive portal — fake login page served to connected clients
- **Step 4:** Deauth integration — knock clients off the real network so they reconnect to ours

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

The script asks for an SSID, sends it to the ESP32, and streams connection events to the console. Press **Ctrl+C** to bring the AP down.

### Verify it works
After running the script, open WiFi on your phone — the network should appear immediately. When you connect, the terminal will print `TWIN:CLIENT_JOINED:<mac>`.

---

## UART Protocol

| Command | Description |
|---|---|
| `CMD:TWIN_START:<ssid>` | Start fake AP with the given SSID |
| `CMD:TWIN_STOP` | Stop the AP |

| Response | Description |
|---|---|
| `GHOST:READY` | Firmware booted |
| `TWIN:AP_STARTED:<ssid>` | AP is live |
| `TWIN:CLIENT_JOINED:<mac>` | A device connected |
| `TWIN:CLIENT_LEFT:<mac>` | A device disconnected |
| `TWIN:AP_STOPPED` | AP shut down |
| `ERR:NO_SSID` | No SSID provided in command |
| `ERR:UNKNOWN` | Unrecognized command |

---

## UART Pinout

| Signal | GPIO | Physical pin (NodeMCU) |
|---|---|---|
| TX | GPIO17 | TX0 |
| RX | GPIO16 | RX0 |
| Baud rate | — | 115200 |
