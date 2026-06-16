# Ghost — ESP32 WiFi Toolkit

A collection of custom ESP32 firmware modules for WiFi research and security testing, built with ESP-IDF and controlled over UART.

Each module is a standalone ESP-IDF project with its own build system, Python controller, and documentation.

---

## Modules

| Folder | Description |
|---|---|
| [`ap-scanner/`](ap-scanner/) | Scan nearby WiFi access points over UART |
| [`deauth-attack/`](deauth-attack/) | Send forged deauth frames to disconnect clients from a target network |
| [`evil-twin/`](evil-twin/) | Clone a real network and serve a captive portal to capture credentials |

---

## Hardware

NodeMCU ESP32-WROOM-32 (38-pin) — all modules use the same board and the same UART2 pinout.

## UART Pinout

The firmware is configured for UART2 (GPIO16/GPIO17) by default. However depending on your ESP32 board, the physical pins that respond may differ. Check your board's datasheet to find the correct TX and RX pins.

> **Note:** On the NodeMCU ESP32-WROOM-32 used in this project, UART2 responded on the **TX0 and RX0** physical pins of the board.

```
Firmware config: UART2 — GPIO17 (TX), GPIO16 (RX)
Tested on:       TX0 / RX0 physical pins
Baud rate:       115200
```

---

## License

MIT
