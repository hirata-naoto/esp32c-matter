/*
 * app_driver.cpp – Hardware driver for the Matter Thread device
 *
 * LED  : GPIO output for on/off control (GPIO pin configured in menuconfig)
 * Button: GPIO input using iot_button; long press (>3 s) triggers factory reset
 */

#include "app_priv.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "iot_button.h"
#include "app_reset.h"

static const char *TAG = "app_driver";

/* ── LED ─────────────────────────────────────────────────────────────────── */

app_driver_handle_t app_driver_light_init(void)
{
    gpio_num_t pin = (gpio_num_t)CONFIG_MTD_LED_GPIO;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(pin, 0); /* Start with LED off */

    ESP_LOGI(TAG, "LED initialized on GPIO%d", (int)pin);
    return (app_driver_handle_t)(uintptr_t)pin;
}

esp_err_t app_driver_light_set_power(app_driver_handle_t handle, bool power)
{
    gpio_num_t pin = (gpio_num_t)(uintptr_t)handle;
    ESP_LOGI(TAG, "Light %s", power ? "ON" : "OFF");
    return gpio_set_level(pin, power ? 1 : 0);
}

esp_err_t app_driver_light_set_brightness(app_driver_handle_t handle,
                                           uint8_t brightness)
{
    /*
     * The GPIO LED supports only on/off.
     * A brightness of 0 turns the LED off; any other value turns it on.
     * For PWM dimming, replace gpio_set_level() with LEDC calls here.
     */
    return app_driver_light_set_power(handle, brightness > 0);
}

/* ── Button ──────────────────────────────────────────────────────────────── */

app_driver_handle_t app_driver_button_init(void)
{
    button_config_t btn_cfg = {
        .type             = BUTTON_TYPE_GPIO,
        .long_press_time  = 3000, /* 3 s triggers factory reset */
        .short_press_time = 200,
        .gpio_button_config = {
            .gpio_num     = (int8_t)CONFIG_MTD_BUTTON_GPIO,
            .active_level = 0, /* BOOT button is active-low */
        },
    };

    button_handle_t btn = iot_button_create(&btn_cfg);
    if (!btn) {
        ESP_LOGE(TAG, "Failed to create button on GPIO%d", CONFIG_MTD_BUTTON_GPIO);
        return NULL;
    }

    ESP_LOGI(TAG, "Button initialized on GPIO%d (long-press >3 s = factory reset)",
             CONFIG_MTD_BUTTON_GPIO);
    return (app_driver_handle_t)btn;
}
