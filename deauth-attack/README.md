# Ghost — Deauth Attack Module

A standalone ESP32 module that sends forged 802.11 deauthentication frames to disconnect clients from a target WiFi network. Built with ESP-IDF, controlled over UART.

> **For authorized security testing and educational use only.**

---

## How it works

802.11 deauthentication frames are unauthenticated by design — routers and clients have no way to verify who sent them. This module:

1. Scans nearby networks to find the target's BSSID and channel
2. Locks onto that channel
3. Sends forged deauth frames impersonating the router, addressed to broadcast (hits all clients) and individually to each discovered client
4. Simultaneously enables promiscuous mode to discover and track connected clients in real time

**No connection to the target network is required.**

> Note: WPA3 networks with Management Frame Protection (PMF) are immune to this attack. Most WPA2 networks are still vulnerable.

---

## Hardware Required

- NodeMCU ESP32-WROOM-32 (38-pin)
- USB cable (data capable)

---

## Project Structure

```
deauth-attack/
├── CMakeLists.txt
├── README.md
├── Deauth.py
└── main/
    ├── CMakeLists.txt
    └── deauth.c
```

---

## Build & Flash

Activate ESP-IDF environment first (run in CMD, not PowerShell):

```
C:\esp32\v6.0.1\esp-idf\export.bat
```

Then from the `deauth-attack/` folder:

```
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash
```

> When **Connecting......** appears, hold the **BOOT** button on the NodeMCU and release once flashing starts.

---

## Usage via Python

```
python Deauth.py
```

The script will:
1. Wait for the ESP32 to boot
2. Scan for nearby networks and list them
3. Ask you to pick a target by number
4. Start the attack and stream discovered clients to the console
5. Press **Ctrl+C** to stop — sends `CMD:DEAUTH_STOP` cleanly

---

## UART Protocol

| Command | Description |
|---|---|
| `CMD:SCAN_AP` | Scan nearby networks (returns SSID, BSSID, RSSI, channel) |
| `CMD:DEAUTH:<ssid>` | Start deauth attack on the given SSID |
| `CMD:DEAUTH_STOP` | Stop the attack |

| Response | Description |
|---|---|
| `GHOST:READY` | Firmware booted |
| `SCAN:START` / `SCAN:DONE` | Scan boundaries |
| `AP:SSID,BSSID,RSSI,CH` | Network found during scan |
| `DEAUTH:SCANNING_FOR:<ssid>` | Looking up BSSID for target |
| `DEAUTH:FOUND:<ssid> BSSID=... CH=...` | Target located |
| `DEAUTH:STARTED` | Attack running |
| `CLIENT:MAC` | New client discovered on target network |
| `DEAUTH:STOPPED` | Attack stopped |
| `ERR:SSID_NOT_FOUND` | Target SSID not visible |
| `ERR:UNKNOWN` | Unrecognized command |

---

## UART Pinout

Same as main project — UART2 on GPIO16/GPIO17.

| Signal | GPIO | Physical pin (NodeMCU) |
|---|---|---|
| TX | GPIO17 | TX0 |
| RX | GPIO16 | RX0 |
| Baud rate | — | 115200 |

---

## Differences from main project

| Feature | Main (`ghost.c`) | This module (`deauth.c`) |
|---|---|---|
| WiFi scan | Yes | Yes (with BSSID in output) |
| AP scan response | `AP:SSID,RSSI,CH` | `AP:SSID,BSSID,RSSI,CH` |
| Deauth attack | No | Yes |
| Promiscuous mode | No | Yes (client discovery) |
| Raw frame TX | No | Yes (`esp_wifi_80211_tx`) |

---

## License

MIT
