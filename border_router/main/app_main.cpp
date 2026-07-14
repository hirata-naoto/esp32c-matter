/*
 * XIAO ESP32C5 WiFi–Thread Matter Border Router
 *
 * The Seeed Studio XIAO ESP32C5 contains:
 *   - Dual-band WiFi 2.4 / 5 GHz  → backbone network
 *   - IEEE 802.15.4 radio          → Thread mesh network
 *
 * This firmware bridges the two networks and exposes a Matter
 * Thread Border Router device type (0x0090) so the board can be
 * commissioned and managed by any Matter-compatible app or controller.
 *
 * Prerequisites
 * ─────────────
 *   1. ESP-IDF v5.1+   https://github.com/espressif/esp-idf
 *   2. esp-matter SDK  https://github.com/espressif/esp-matter
 *      cd esp-matter && ./install.sh && source export.sh
 *
 * Build / flash
 * ─────────────
 *   idf.py set-target esp32c5
 *   idf.py menuconfig   # set WiFi credentials under "Border Router Configuration"
 *   idf.py build flash monitor
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs_flash.h"

/* OpenThread & Border Router ------------------------------------------------ */
#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/instance.h"

/* Matter -------------------------------------------------------------------- */
#include <esp_matter.h>
#include <esp_matter_console.h>

#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/thread-border-router-management-server/GenericOpenThreadBorderRouterDelegate.h>
#include <lib/support/CHIPMem.h>
#include <platform/KvsPersistentStorageDelegate.h>

static const char *TAG = "br_main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

/* ── Matter callbacks ────────────────────────────────────────────────────── */

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                          uint16_t endpoint_id,
                                          uint32_t cluster_id,
                                          uint32_t attribute_id,
                                          esp_matter_attr_val_t *val,
                                          void *priv_data)
{
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
        ESP_LOGI(TAG, "Interface IP address changed");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Matter commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Commissioning fail-safe timer expired");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed");
        break;
    default:
        break;
    }
}

/* ── WiFi (backbone) ─────────────────────────────────────────────────────── */

static esp_netif_t *s_wifi_netif = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to WiFi '%s'...", CONFIG_BR_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected – IP: " IPSTR, IP2STR(&e->ip_info.ip));
        /* Register WiFi interface as the Thread backbone network */
        esp_openthread_lock_acquire(portMAX_DELAY);
        esp_openthread_set_backbone_netif(s_wifi_netif);
        esp_openthread_lock_release();
    }
}

static void wifi_init(void)
{
    s_wifi_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                         &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                         &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid,     CONFIG_BR_WIFI_SSID,
            sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_BR_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ── OpenThread Border Router ────────────────────────────────────────────── */

static esp_netif_t *s_ot_netif = NULL;

/** Create or reuse a Thread operational dataset stored in NVS. */
static void ensure_thread_dataset(otInstance *inst)
{
    otOperationalDatasetTlvs ds_tlvs;
    if (otDatasetGetActiveTlvs(inst, &ds_tlvs) == OT_ERROR_NONE) {
        ESP_LOGI(TAG, "Existing Thread dataset found – reusing");
        return;
    }

    /* Build a new dataset and personalise the network name with the MAC. */
    otOperationalDataset ds;
    otDatasetCreateNewNetwork(inst, &ds);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BASE);
    snprintf(ds.mNetworkName.m8, sizeof(ds.mNetworkName.m8),
             "%.12s-%02X%02X", CONFIG_BR_THREAD_NETWORK_NAME, mac[4], mac[5]);

    otDatasetConvertToTlvs(&ds, &ds_tlvs);
    ESP_ERROR_CHECK(esp_openthread_auto_start(&ds_tlvs));

    ESP_LOGI(TAG, "New Thread network created: '%s'", ds.mNetworkName.m8);
}

/** Wait for WiFi then enable border routing between Thread and backbone. */
static void ot_br_init_task(void *arg)
{
    esp_netif_ip_info_t ip_info;
    do {
        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (esp_netif_get_ip_info(s_wifi_netif, &ip_info) != ESP_OK ||
             ip_info.ip.addr == 0);

    esp_openthread_lock_acquire(portMAX_DELAY);

    ESP_ERROR_CHECK(esp_openthread_border_router_init());
    ensure_thread_dataset(esp_openthread_get_instance());

    esp_openthread_lock_release();

    ESP_LOGI(TAG, "Thread Border Router operational");
    vTaskDelete(NULL);
}

/** Owns the OpenThread main loop (must not return). */
static void ot_main_task(void *arg)
{
    esp_openthread_platform_config_t ot_cfg = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    ESP_ERROR_CHECK(esp_openthread_init(&ot_cfg));

    /* Create Thread network interface (lwIP ↔ OpenThread glue) */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    s_ot_netif = esp_netif_new(&netif_cfg);
    assert(s_ot_netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(s_ot_netif,
                                      esp_openthread_netif_glue_init(&ot_cfg)));

    /* Interactive CLI for debugging (type "help" in the monitor) */
    esp_openthread_cli_init();
    esp_openthread_cli_create_task();

    /* Border router init waits for WiFi in a separate task */
    xTaskCreate(ot_br_init_task, "ot_br_init", 4096, NULL, 4, NULL);

    /* Run OpenThread event loop – blocks indefinitely */
    esp_openthread_launch_mainloop();

    /* Reached only if the mainloop exits abnormally */
    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(s_ot_netif);
    vTaskDelete(NULL);
}

/* ── app_main ────────────────────────────────────────────────────────────── */

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== XIAO ESP32C5 WiFi-Thread Matter Border Router ===");

    /* NVS (needed by WiFi, OpenThread, and Matter) */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* TCP/IP stack & system event loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Backbone WiFi */
    wifi_init();

    /* mDNS – required for Matter Operational Discovery */
    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set("xiao-c5-br");
    mdns_instance_name_set("XIAO ESP32C5 Matter Border Router");

    /* OpenThread + border-router runs in its own task */
    xTaskCreate(ot_main_task, "ot_main", 10240, NULL, 5, NULL);

    /* ── Matter ─────────────────────────────────────────────────────────── */

    /*
     * Persistent storage delegate required by GenericOpenThreadBorderRouterDelegate.
     * chip::KvsPersistentStorageDelegate wraps the ESP NVS KVS backend.
     */
    static chip::KvsPersistentStorageDelegate s_tbr_storage;

    node::config_t node_config;
    node_t *node = node::create(&node_config,
                                 app_attribute_update_cb,
                                 app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        abort();
    }

    /* Thread Border Router endpoint (Matter device type 0x0090) */
    using TBRDelegate =
        chip::app::Clusters::ThreadBorderRouterManagement::
            GenericOpenThreadBorderRouterDelegate;

    auto *tbr_delegate = chip::Platform::New<TBRDelegate>(&s_tbr_storage);

    thread_border_router::config_t tbr_config;
    tbr_config.thread_border_router_management.delegate = tbr_delegate;

    endpoint_t *tbr_ep = thread_border_router::create(node, &tbr_config,
                                                       ENDPOINT_FLAG_NONE, NULL);
    if (!tbr_ep) {
        ESP_LOGE(TAG, "Failed to create Thread Border Router endpoint");
        abort();
    }

    /* Optionally enable PAN-change feature for remote Thread network reconfiguration */
    cluster_t *tbr_cluster = cluster::get(tbr_ep, ThreadBorderRouterManagement::Id);
    cluster::thread_border_router_management::feature::pan_change::add(tbr_cluster);

    /* Start the Matter stack */
    ESP_ERROR_CHECK(esp_matter::start(app_event_cb));

#if CONFIG_ENABLE_MATTER_CONSOLE
    ESP_ERROR_CHECK(esp_matter::console::diagnostics_register_commands());
    ESP_ERROR_CHECK(esp_matter::console::wifi_register_commands());
    ESP_ERROR_CHECK(esp_matter::console::init());
#endif

    ESP_LOGI(TAG, "Ready. Commission via a Matter-compatible app.");
    ESP_LOGI(TAG, "Thread CLI available in monitor (type 'help').");
}
