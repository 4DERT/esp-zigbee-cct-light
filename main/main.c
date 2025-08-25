#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include "input_handler.h"
#include "led_controller.h"
#include "zb_app.h"
#include "zb_attr_report.h"
#include "zb_config.h"
#include "zigbee_cct_light_model.h"

#define LED_GPIO GPIO_NUM_15
#define LED_ACTIVE_LEVEL 1

static const char *TAG = "main";

static void init_nvs(void);
static void wait_for_zigbee_connection(void);

/**
 * @brief Main application entry point.
 */
void app_main(void) {
    // Initialize NVS (required for Zigbee stack and other components)
    init_nvs();

    // Configure status LED GPIO
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, LED_ACTIVE_LEVEL); // Turn LED on to indicate boot

    // Initialize light controller (PWM/MOSFET driver)
    lc_init();

    // Initialize Zigbee CCT Light Model
    zcctlm_init();

    // Initialize input handler (button press detection)
    input_init();

    // Initialize and start Zigbee stack
    appzb_init();

    // Wait until device joins Zigbee network
    wait_for_zigbee_connection();

    // Report current device state to coordinator
    zcctlm_report_current_state();

    // Turn off LED to indicate end of init phase
    gpio_set_level(LED_GPIO, !LED_ACTIVE_LEVEL);
}

/**
 * @brief Initialize non-volatile storage (NVS)
 */
static void init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/**
 * @brief Wait until the device is joined to a Zigbee network.
 *
 * LED will blink while waiting.
 */
static void wait_for_zigbee_connection(void) {
    while (!appzb_is_connected()) {
        ESP_LOGI(TAG, "Waiting for Zigbee connection...");
        gpio_set_level(LED_GPIO, LED_ACTIVE_LEVEL);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(LED_GPIO, !LED_ACTIVE_LEVEL);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Keep LED on to indicate successful network join
    gpio_set_level(LED_GPIO, LED_ACTIVE_LEVEL);
}