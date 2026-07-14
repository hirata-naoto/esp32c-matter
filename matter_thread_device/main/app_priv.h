/*
 * app_priv.h – Private application API
 *
 * Shared between app_main.cpp (Matter layer) and app_driver.cpp (hardware layer).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle returned by the driver init functions. */
typedef void *app_driver_handle_t;

/**
 * Initialize the LED GPIO.
 *
 * @return Opaque driver handle, or NULL on failure.
 */
app_driver_handle_t app_driver_light_init(void);

/**
 * Initialize the factory-reset button.
 *
 * A long press (> 3 s) erases Matter commissioning data and reboots.
 *
 * @return Opaque driver handle, or NULL on failure.
 */
app_driver_handle_t app_driver_button_init(void);

/**
 * Turn the LED on or off.
 *
 * @param handle  Handle returned by app_driver_light_init().
 * @param power   true = on, false = off.
 */
esp_err_t app_driver_light_set_power(app_driver_handle_t handle, bool power);

/**
 * Set LED brightness.
 *
 * @param handle      Handle returned by app_driver_light_init().
 * @param brightness  0–254 (Matter CurrentLevel range). 0 = off.
 */
esp_err_t app_driver_light_set_brightness(app_driver_handle_t handle,
                                           uint8_t brightness);

#ifdef __cplusplus
}
#endif
