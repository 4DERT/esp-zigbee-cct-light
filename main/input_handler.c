#include "input_handler.h"

#include "button_gpio.h"
#include "esp_log.h"
#include "iot_button.h"
#include "zb_app.h"

#include "light_presets.h"
#include "zigbee_cct_light_model.h"

static const char *TAG = "input handler";

const button_config_t button_cfg = {0};
const button_gpio_config_t button_gpio_cfg = {
    .gpio_num = INPUT_BUTTON_GPIO,
    .active_level = INPUT_BUTTON_ACTIVE_LEVEL,
};

button_handle_t button;

static void button_single_click_cb(void *arg, void *usr_data) {
    ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
    zcctlm_toggle_on_off();
    zcctlm_report_current_state();
}

static void button_double_click_cb(void *arg, void *usr_data) {
    ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK");
    light_presets_cycle();
}

static void button_long_click_cb(void *arg, void *usr_data) {
    ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START");
    appzb_factory_reset();
}

void input_init() {
    iot_button_new_gpio_device(&button_cfg, &button_gpio_cfg, &button);
    if (NULL == button) {
        ESP_LOGE(TAG, "Button create failed");
    }

    button_event_args_t args = {
        .long_press.press_time = 5000,
    };

    iot_button_register_cb(button, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb, NULL);
    iot_button_register_cb(button, BUTTON_DOUBLE_CLICK, NULL, button_double_click_cb, NULL);
    iot_button_register_cb(button, BUTTON_LONG_PRESS_START, &args, button_long_click_cb, NULL);
}