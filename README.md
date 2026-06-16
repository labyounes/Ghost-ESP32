# Ghost — ESP32 WiFi Toolkit

A collection of custom ESP32 firmware modules for WiFi research and security testing, built with ESP-IDF.

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

NodeMCU ESP32-WROOM-32 (38-pin)

---

## Communication

All modules communicate over **USB** — no external wiring needed. The NodeMCU has a built-in USB-to-UART chip (CP2102) that handles the connection transparently. Just plug in the USB cable and run the Python script.

> **Note for extension:** The physical UART pins are not used in any current module. However if you plan to extend this project to work with external hardware (e.g. Flipper Zero), here is the relevant config to avoid confusion:
>
> ```
> Firmware config: UART2 — GPIO17 (TX), GPIO16 (RX)
> Tested on:       TX0 / RX0 physical pins (NodeMCU ESP32-WROOM-32)
> Baud rate:       115200
> ```
> On the NodeMCU ESP32-WROOM-32, UART2 responded on the TX0 and RX0 physical pins of the board — not on the GPIO16/GPIO17 pins directly. Check your board's datasheet if using a different ESP32 variant.

---

## License

MIT
