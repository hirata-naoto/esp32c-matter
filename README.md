# esp32c-matter

ESP-IDF + esp-matter projects for the **Seeed Studio XIAO ESP32C5** and **XIAO ESP32C6** boards.

| Project | Target | Description |
|---------|--------|-------------|
| [`border_router/`](border_router/) | XIAO ESP32C5 | WiFi–Thread Matter Border Router |
| [`matter_thread_device/`](matter_thread_device/) | XIAO ESP32C5 / XIAO ESP32C6 | Matter over Thread On/Off Light |

## Architecture

```
Home WiFi (backbone)
       │
  ┌────┴───────────────────────────────────┐
  │  XIAO ESP32C5  ─  border_router        │
  │  • WiFi STA (2.4 / 5 GHz)             │
  │  • IEEE 802.15.4 (Thread router)       │
  │  • Matter Thread Border Router 0x0090  │
  └────┬───────────────────────────────────┘
       │ Thread mesh (802.15.4)
  ┌────┴───────────────────────────────────┐
  │  XIAO ESP32C5/C6  ─  matter_thread_device │
  │  • IEEE 802.15.4 (Thread end device)   │
  │  • Matter On/Off Light 0x0100          │
  └────────────────────────────────────────┘
```

## Prerequisites

### Hardware
- 1 × Seeed Studio XIAO ESP32C5 (border router)
- 1 × Seeed Studio XIAO ESP32C5 **or** XIAO ESP32C6 (Thread device)
- USB-C cables for flashing

### Software
1. **ESP-IDF v5.1 or newer**
   [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

2. **esp-matter SDK**
   ```bash
   git clone --recursive https://github.com/espressif/esp-matter.git
   cd esp-matter
   ./install.sh
   source export.sh   # run this every terminal session
   ```

## Quick Start

### 1 – Flash the Border Router (XIAO ESP32C5)

```bash
cd border_router
idf.py set-target esp32c5
idf.py menuconfig   # Set WiFi SSID/password under "Border Router Configuration"
idf.py build flash monitor
```

### 2 – Flash the Thread Device (XIAO ESP32C5 or C6)

```bash
cd matter_thread_device
idf.py set-target esp32c5   # or esp32c6
idf.py menuconfig           # Adjust LED/button GPIO if needed
idf.py build flash monitor
```

### 3 – Commission via a Matter App

1. Commission the **border router** first – it creates the Thread network.
2. Commission the **Thread device** – the app automatically pushes Thread credentials to it.

See each sub-project's README for detailed instructions.