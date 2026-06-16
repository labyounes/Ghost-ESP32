# Ghost — ESP32 WiFi Toolkit

A collection of custom ESP32 firmware modules for WiFi research and security testing, built with ESP-IDF and controlled over UART.

Each module is a standalone ESP-IDF project with its own build system, Python controller, and documentation.

---

## Modules

| Folder | Description |
|---|---|
| [`ap-scanner/`](ap-scanner/) | Scan nearby WiFi access points over UART |
| [`deauth-attack/`](deauth-attack/) | Send forged deauth frames to disconnect clients from a target network |

---

## Hardware

NodeMCU ESP32-WROOM-32 (38-pin) — all modules use the same board and the same UART2 pinout (GPIO16/GPIO17, 115200 baud).

---

## License

MIT
