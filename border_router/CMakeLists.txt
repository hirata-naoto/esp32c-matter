cmake_minimum_required(VERSION 3.16)

# ── esp-matter SDK ────────────────────────────────────────────────────────────
if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR
        "ESP_MATTER_PATH is not set.\n"
        "Source the esp-matter export script first:\n"
        "  cd <path-to-esp-matter> && source export.sh")
endif()

# ── Device HAL (ESP32-C5 only – it has both WiFi and 802.15.4) ──────────────
if(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})
    if("${IDF_TARGET}" STREQUAL "esp32c5")
        set(ENV{ESP_MATTER_DEVICE_PATH}
            $ENV{ESP_MATTER_PATH}/device_hal/device/esp32c5_devkit_c)
    else()
        message(FATAL_ERROR
            "The border router requires an ESP32-C5 (dual WiFi + IEEE 802.15.4).\n"
            "Run:  idf.py set-target esp32c5")
    endif()
endif()

set(ESP_MATTER_PATH  $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH  ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/device_hal/device")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${ESP_MATTER_PATH}/examples/common/cmake_common/components_include.cmake)
include($ENV{ESP_MATTER_DEVICE_PATH}/esp_matter_device.cmake)
project(border_router)

idf_build_set_property(CXX_COMPILE_OPTIONS
    "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS   "-Os" APPEND)
idf_build_set_property(COMPILE_OPTIONS
    "-Wno-format-nonliteral;-Wno-format-security" APPEND)
