# WiFi–Thread Matter Border Router

**Target board:** Seeed Studio XIAO ESP32C5

## Description

This firmware turns the XIAO ESP32C5 into a **WiFi–Thread Border Router** that is also a **Matter device**. It uses the ESP32-C5's built-in dual-band WiFi (backbone) and IEEE 802.15.4 radio (Thread mesh) — no external radio co-processor needed.

## Features

| Feature | Details |
|---------|---------|
| Backbone network | WiFi STA (2.4 GHz or 5 GHz) |
| Thread mesh | IEEE 802.15.4, auto-creates network with MAC-based name |
| Matter device type | Thread Border Router (0x0090) |
| TBR Management | PAN-change feature enabled |
| OpenThread CLI | Interactive debugging via UART |
| Persistence | Thread dataset and Matter fabric data stored in NVS |

## Hardware Requirements

- Seeed Studio XIAO ESP32C5
- USB-C cable

## Software Prerequisites

1. **ESP-IDF v5.1 or newer**
2. **esp-matter SDK**

```bash
git clone --recursive https://github.com/espressif/esp-matter.git
cd esp-matter && ./install.sh && source export.sh
```

## Build & Flash

```bash
# From this directory (border_router/)
idf.py set-target esp32c5
idf.py menuconfig
# → Border Router Configuration
#     • Backbone WiFi SSID      ← your home WiFi SSID
#     • Backbone WiFi Password  ← your home WiFi password
idf.py build flash monitor
```

## Configuration Reference

| Kconfig option | Default | Description |
|----------------|---------|-------------|
| `BR_WIFI_SSID` | `myhome` | Backbone WiFi SSID |
| `BR_WIFI_PASSWORD` | *(empty)* | WiFi password |
| `BR_THREAD_NETWORK_NAME` | `ESP-Thread` | Thread network name prefix |
| `ENABLE_MATTER_CONSOLE` | `y` | Enable Matter UART CLI |

The Thread network name has the last 2 MAC bytes appended automatically (e.g. `ESP-Thread-A1B2`).

## Commissioning

1. Power on the XIAO ESP32C5.
2. Open your Matter app (Apple Home, Google Home, Amazon Alexa, CHIP Tool, etc.).
3. Add a new device — the border router advertises itself over BLE.
4. After commissioning, the board joins your Matter fabric and its Thread network is accessible to other Thread devices.

## OpenThread CLI

Connect with `idf.py monitor` and type:

```
state              → current device state (leader / router / …)
dataset active     → print active operational dataset
router table       → list Thread routers in the network
neighbor table     → list Thread neighbors
ipaddr             → list IPv6 addresses
help               → all available commands
```

## Thread Network Auto-Creation

If no Thread dataset exists in NVS, the firmware creates a new network with:
- A random channel, PAN ID, network key, and extended PAN ID
- Network name: `<BR_THREAD_NETWORK_NAME>-<MAC[4]><MAC[5]>` (e.g. `ESP-Thread-A1B2`)

The dataset is persisted in NVS and reused across reboots.
