idf_component_register(
    SRCS
        "app_main.cpp"
        "app_driver.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_netif
        driver
        esp_matter
        app_reset
        iot_button
)
