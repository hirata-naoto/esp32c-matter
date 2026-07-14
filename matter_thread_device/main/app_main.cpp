/*
 * Matter over Thread – On/Off Light
 *
 * Target: Seeed Studio XIAO ESP32C5  or  XIAO ESP32C6
 * Both chips have a built-in IEEE 802.15.4 radio for Thread.
 *
 * Commissioning flow
 * ──────────────────
 *   1. Power on – device advertises itself over Bluetooth LE (BLE).
 *   2. Open a Matter-compatible app (Apple Home, Google Home, Amazon Alexa,
 *      or the CHIP Tool CLI).
 *   3. Commission the device; the app automatically pushes Thread network
 *      credentials from the border router.
 *   4. Device joins the Thread network and is reachable via Matter.
 *
 * Factory reset
 * ─────────────
 *   Hold the BOOT button for more than 3 seconds.
 *   The device erases its commissioning data and reboots.
 *
 * Prerequisites
 * ─────────────
 *   1. ESP-IDF v5.1+   https://github.com/espressif/esp-idf
 *   2. esp-matter SDK  https://github.com/espressif/esp-matter
 *      cd esp-matter && ./install.sh && source export.sh
 *
 * Build / flash
 * ─────────────
 *   idf.py set-target esp32c5   # or esp32c6
 *   idf.py menuconfig           # adjust LED/button GPIO if needed
 *   idf.py build flash monitor
 */

#include "esp_log.h"
#include "nvs_flash.h"

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>

/* OpenThread platform configuration handed to the Matter stack */
#include "esp_openthread.h"
#include "esp_openthread_types.h"

#include "app_priv.h"

static const char *TAG = "app_main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

/* Endpoint ID and driver handle resolved after Matter node creation */
static uint16_t           s_light_endpoint_id = 0;
static app_driver_handle_t s_light_handle      = NULL;

/* ── Matter callbacks ────────────────────────────────────────────────────── */

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                          uint16_t endpoint_id,
                                          uint32_t cluster_id,
                                          uint32_t attribute_id,
                                          esp_matter_attr_val_t *val,
                                          void *priv_data)
{
    /* Only act on PRE_UPDATE for our light endpoint */
    if (type != PRE_UPDATE || endpoint_id != s_light_endpoint_id) {
        return ESP_OK;
    }

    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            return app_driver_light_set_power(s_light_handle, val->val.b);
        }
    } else if (cluster_id == LevelControl::Id) {
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
            return app_driver_light_set_brightness(s_light_handle, val->val.u8);
        }
    }

    return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t type,
                                        uint16_t endpoint_id,
                                        uint8_t effect_id,
                                        uint8_t effect_variant,
                                        void *priv_data)
{
    ESP_LOGI(TAG, "Identify: endpoint=0x%04x effect=0x%02x variant=0x%02x",
             endpoint_id, effect_id, effect_variant);
    return ESP_OK;
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "IP address changed");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Matter commissioning complete – device is on Thread fabric");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Commissioning fail-safe timer expired");
        break;
    case chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange:
        ESP_LOGI(TAG, "Thread connectivity: %s",
                 event->ThreadConnectivityChange.Result ==
                     chip::DeviceLayer::ConnectivityChange::kConnectivity_Established
                     ? "established"
                     : "lost");
        break;
    default:
        break;
    }
}

/* ── app_main ────────────────────────────────────────────────────────────── */

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== XIAO ESP32C5/C6 Matter over Thread – On/Off Light ===");

    /* NVS – required by Matter and OpenThread for persistent storage */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Hardware drivers */
    s_light_handle = app_driver_light_init();

    app_driver_handle_t button_handle = app_driver_button_init();
    if (button_handle) {
        /* Register the button for long-press factory reset */
        app_reset_button_register(button_handle);
    }

    /* Pass OpenThread platform config to the Matter stack.
     * The Matter stack initialises OpenThread internally after esp_matter::start(). */
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    esp_openthread_platform_config_t ot_config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&ot_config);
#endif

    /* Create Matter node */
    node::config_t node_config;
    node_t *node = node::create(&node_config,
                                 app_attribute_update_cb,
                                 app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        abort();
    }

    /* On/Off Light endpoint (Matter device type 0x0100) */
    on_off_light::config_t light_config;
    light_config.on_off.on_off = false; /* Start powered off */

    endpoint_t *endpoint = on_off_light::create(node, &light_config,
                                                  ENDPOINT_FLAG_NONE,
                                                  s_light_handle);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create On/Off Light endpoint");
        abort();
    }

    s_light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "On/Off Light endpoint: ID=0x%04x", s_light_endpoint_id);

    /* Defer NVS writes for the CurrentLevel attribute (changes frequently) */
    attribute_t *current_level = attribute::get(
        s_light_endpoint_id,
        LevelControl::Id,
        LevelControl::Attributes::CurrentLevel::Id);
    if (current_level) {
        attribute::set_deferred_persistence(current_level);
    }

    /* Start Matter stack (also starts OpenThread internally) */
    ESP_ERROR_CHECK(esp_matter::start(app_event_cb));

#if CONFIG_ENABLE_MATTER_CONSOLE
    ESP_ERROR_CHECK(esp_matter::console::diagnostics_register_commands());
    ESP_ERROR_CHECK(esp_matter::console::init());
#endif

    ESP_LOGI(TAG, "Waiting for BLE commissioning...");
    ESP_LOGI(TAG, "Long-press BOOT button (>3 s) to factory-reset.");
}
