# Matter over Thread – On/Off Light

**Target boards:** Seeed Studio XIAO ESP32C5 / XIAO ESP32C6

## Description

A minimal Matter On/Off Light that communicates over a **Thread** mesh network. After commissioning, it can be controlled by any Matter-compatible app or voice assistant. No WiFi credentials are needed on the device itself — networking is handled by the Thread Border Router.

## Features

| Feature | Details |
|---------|---------|
| Matter device type | On/Off Light (0x0100) |
| Transport | IEEE 802.15.4 / Thread |
| Clusters | On/Off, Level Control, Descriptor, Basic Information |
| Commissioning | Bluetooth LE (BLE) |
| Factory reset | Long-press BOOT button (>3 s) |
| Persistence | Commissioning data and attributes stored in NVS |

## Hardware Requirements

- Seeed Studio XIAO ESP32C5 **or** XIAO ESP32C6
- LED connected to GPIO8 (configurable) with a current-limiting resistor (~220 Ω)
- USB-C cable

### GPIO Reference

| Signal | Default GPIO | Notes |
|--------|-------------|-------|
| LED | GPIO8 | Connect LED → resistor → GPIO8 |
| Reset button | GPIO9 | Onboard BOOT button (active-low) |

Adjust GPIOs in `idf.py menuconfig → Matter Thread Device Configuration`.

## Software Prerequisites

1. **ESP-IDF v5.1 or newer**
2. **esp-matter SDK**

```bash
git clone --recursive https://github.com/espressif/esp-matter.git
cd esp-matter && ./install.sh && source export.sh
```

## Build & Flash

```bash
# From this directory (matter_thread_device/)

# XIAO ESP32C5:
idf.py set-target esp32c5

# XIAO ESP32C6:
idf.py set-target esp32c6

idf.py menuconfig
# → Matter Thread Device Configuration
#     • LED GPIO number          ← adjust if your LED is on a different pin
#     • Factory-reset button GPIO ← adjust if needed
idf.py build flash monitor
```

## Configuration Reference

| Kconfig option | Default | Description |
|----------------|---------|-------------|
| `MTD_LED_GPIO` | `8` | GPIO for the status LED |
| `MTD_BUTTON_GPIO` | `9` | GPIO for factory-reset button (BOOT) |
| `ENABLE_MATTER_CONSOLE` | `y` | Enable Matter UART CLI diagnostics |

## Commissioning Flow

1. Power on the XIAO board.
2. Open your Matter app and add a new device.
3. The device is discovered via BLE.
4. The commissioner (app) provides Thread network credentials from the border router.
5. The device joins the Thread network and appears as a controllable light.

> **Note:** The border router must be commissioned and running first so the Thread network exists.

## Factory Reset

Hold the **BOOT** button for **more than 3 seconds**. The device:
1. Erases all commissioning data from NVS.
2. Reboots and enters commissioning mode (BLE advertising restarts).

## Extending the Device

The On/Off Light is a starting point. You can replace or augment the endpoint type:

| Device type | esp-matter endpoint | Description |
|------------|---------------------|-------------|
| On/Off Light | `on_off_light::create()` | Simple on/off LED |
| Color Temperature Light | `color_temperature_light::create()` | CCT LED |
| Temperature Sensor | `temperature_sensor::create()` | Reports temperature |
| Contact Sensor | `contact_sensor::create()` | Door/window sensor |
| Generic Switch | `generic_switch::create()` | Momentary or latching switch |
