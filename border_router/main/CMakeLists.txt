idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        mdns
        esp_openthread
        openthread
        esp_matter
        app_reset
)
