#include "zigbee_cct_light_model.h"

#include <math.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "led_controller.h"
#include "zb_attr_report.h"

#define ZCCTLM_NVS_NAMESPACE "zcctlm"
#define ZCCTLM_NVS_KEY_ON_OFF "state"
#define ZCCTLM_NVS_KEY_BRIGHTNESS "brightness"
#define ZCCTLM_NVS_KEY_MIREDS "mireds"
#define ZCCTLM_NVS_KEY_ON_TRANSITION_TIME "ontime"
#define ZCCTLM_NVS_KEY_OFF_TRANSITION_TIME "offtime"
#define ZCCTLM_NVS_KEY_STARTUP_ON_OFF "onoff"

static const char *TAG = "ZCCTLM";

typedef struct {
    bool on_off;
    uint8_t brightness;
    uint16_t mireds;

    // synchronized with NVS
    uint16_t on_transition_time;
    uint16_t off_transition_time;
    zcctl_startup_behavior_e startup_behavior;
} zcctlm_state_t;

zcctlm_state_t state;

static SemaphoreHandle_t state_mutex;

#if ZCCTLM_ENABLE_ON_OFF_DUTY_BLOCK_WORKAROUND == 1
void zcctlm_set_duty(); 
static TimerHandle_t block_set_duty_timer;
static volatile bool block_set_duty;

void block_set_duty_timer_cb(TimerHandle_t timer) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        block_set_duty = false;
        zcctlm_set_duty();

        xSemaphoreGive(state_mutex);
    }
}
#endif

#if ZCCTLM_USE_GAMMA_CORRECTION == 1
uint16_t apply_gamma_correction(uint8_t brightness_raw) {
    if (brightness_raw == 0)
        return 0;

    float norm = (float)brightness_raw / ZCCTLM_MAX_BRIGHTNESS;
    float corrected = powf(norm, ZCCTLM_GAMMA_CORRECTION);
    uint16_t pwm = (uint16_t)(corrected * (LC_MAX_DUTY - LC_MIN_DUTY)) + LC_MIN_DUTY;

    if (pwm > LC_MAX_DUTY)
        pwm = LC_MAX_DUTY;
    return pwm;
}
#endif

void zcctlm_set_duty() {
    // this function should be executed while state_mutex is taken
    // calculate duty for warm and cold

#if ZCCTLM_ENABLE_ON_OFF_DUTY_BLOCK_WORKAROUND == 1
    if (block_set_duty == true) {
        ESP_LOGI(TAG, "Skipped zcctlm_set_duty due to active On/Off workaround (block_set_duty = true)");
        return;
    }
#endif

    if (!state.on_off || state.brightness == 0) {
        lc_set_duty_warm(0, state.off_transition_time);
        lc_set_duty_cold(0, state.off_transition_time);
        return;
    }

    if (state.mireds < ZCCTLM_MIN_TEMP)
        state.mireds = ZCCTLM_MIN_TEMP;
    if (state.mireds > ZCCTLM_MAX_TEMP)
        state.mireds = ZCCTLM_MAX_TEMP;

    float temp_frac = (state.mireds - ZCCTLM_MIN_TEMP) / (float)(ZCCTLM_MAX_TEMP - ZCCTLM_MIN_TEMP);
    float warm_frac = 1.0f - temp_frac;
    float cold_frac = temp_frac;

#if ZCCTLM_USE_GAMMA_CORRECTION == 1
    // Gamma-corrected total duty
    uint16_t total_duty = apply_gamma_correction(state.brightness);
#else
    // Linear brightness
    float brightness_frac = state.brightness / 254.0f;
    uint16_t total_duty = (uint16_t)(LC_MAX_DUTY * brightness_frac);
#endif

    uint16_t warm_duty = (uint16_t)(total_duty * warm_frac);
    uint16_t cold_duty = (uint16_t)(total_duty * cold_frac);

    lc_set_duty_warm(warm_duty, state.on_transition_time);
    lc_set_duty_cold(cold_duty, state.on_transition_time);
}

void zcctlm_save_to_nvs(const char *key, uint16_t value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ZCCTLM_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u16(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write key '%s' to NVS: %s", key, esp_err_to_name(err));
    } else {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Saved key '%s' = %u to NVS", key, value);
    }

    nvs_close(handle);
}

uint16_t zcctlm_load_from_nvs(const char *key, uint16_t default_value) {
    nvs_handle_t handle;
    uint16_t value = default_value;

    esp_err_t err = nvs_open(ZCCTLM_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return default_value;
    }

    err = nvs_get_u16(handle, key, &value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Key '%s' not found in NVS. Using default: %u", key, default_value);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read key '%s' from NVS: %s", key, esp_err_to_name(err));
        value = default_value;
    } else {
        ESP_LOGI(TAG, "Loaded key '%s' = %u from NVS", key, value);
    }

    nvs_close(handle);
    return value;
}

static inline bool zcctlm_should_persist_state() {
    return (state.startup_behavior == ZCCTL_STARTUP_PREVIOUS || state.startup_behavior == ZCCTL_STARTUP_TOGGLE);
}

// public

void zcctlm_init() {
    state_mutex = xSemaphoreCreateMutex();

    // Defaults
    bool on_off = ZCCTLM_DEFAULT_ONOFF;
    uint8_t brightness = ZCCTLM_DEFAULT_BRIGHTNESS;
    uint16_t mireds = ZCCTLM_AVG_TEMP;

    // Load persistent attributes
    zcctl_startup_behavior_e startup_behavior =
        (zcctl_startup_behavior_e)zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_STARTUP_ON_OFF, (uint16_t)ZCCTLM_DEFAULT_STARTUP_BEHAVIOUR);
    uint16_t on_transition_time = zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_ON_TRANSITION_TIME, ZCCTLM_DEFAULT_TRANSITION_TIME_MS);
    uint16_t off_transition_time = zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_OFF_TRANSITION_TIME, ZCCTLM_DEFAULT_TRANSITION_TIME_MS);

    // Decide how to initialize ON/OFF, brightness, temperature based on startup_behavior
    switch (startup_behavior) {
    case ZCCTL_STARTUP_PREVIOUS:
        on_off = (bool)zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_ON_OFF, (uint16_t)false);
        brightness = (uint8_t)zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_BRIGHTNESS, ZCCTLM_MIN_BRIGHTNESS);
        mireds = zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_MIREDS, ZCCTLM_AVG_TEMP);
        break;

    case ZCCTL_STARTUP_TOGGLE:
        on_off = !(bool)zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_ON_OFF, (uint16_t)false);
        brightness = on_off == true ? ZCCTLM_DEFAULT_BRIGHTNESS : (uint8_t)zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_BRIGHTNESS, ZCCTLM_MIN_BRIGHTNESS);
        mireds = zcctlm_load_from_nvs(ZCCTLM_NVS_KEY_MIREDS, ZCCTLM_AVG_TEMP);
        break;

    case ZCCTL_STARTUP_ON:
        on_off = true;
        brightness = ZCCTLM_DEFAULT_BRIGHTNESS;
        mireds = ZCCTLM_AVG_TEMP;
        break;

    case ZCCTL_STARTUP_OFF:
    default:
        on_off = false;
        brightness = ZCCTLM_DEFAULT_BRIGHTNESS;
        mireds = ZCCTLM_AVG_TEMP;
        break;
    }

    // Save state
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        state.on_off = on_off;
        state.brightness = brightness;
        state.mireds = mireds;
        state.on_transition_time = on_transition_time;
        state.off_transition_time = off_transition_time;
        state.startup_behavior = startup_behavior;

        // Apply the new state to LEDs
        zcctlm_set_duty();

        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_set_on_off(bool on_off) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        if (on_off == state.on_off) {
            xSemaphoreGive(state_mutex);
            return;
        }

#if ZCCTLM_ENABLE_ON_OFF_DUTY_BLOCK_WORKAROUND == 1
        if (on_off == true && state.on_off == false) {
            block_set_duty = true;
            block_set_duty_timer = xTimerCreate("block_set_duty", pdMS_TO_TICKS(ZCCTLM_DUTY_BLOCK_TIME_MS), pdFALSE, NULL, block_set_duty_timer_cb);
            xTimerStart(block_set_duty_timer, 0);
        }
#endif

        state.on_off = on_off;

        zcctlm_set_duty();
        if (zcctlm_should_persist_state()) {
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_ON_OFF, (uint16_t)on_off);
        }
        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_toggle_on_off() {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        state.on_off = !state.on_off;
        zcctlm_set_duty();
        if (zcctlm_should_persist_state()) {
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_ON_OFF, (uint16_t)state.on_off);
        }
        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_set_brightness(uint8_t val) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        if (val == state.brightness) {
            xSemaphoreGive(state_mutex);
            return;
        }

        state.brightness = val;
        zcctlm_set_duty();
        if (zcctlm_should_persist_state()) {
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_BRIGHTNESS, (uint16_t)val);
        }
        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_set_color_temp(uint16_t mireds) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        if (mireds == state.mireds) {
            xSemaphoreGive(state_mutex);
            return;
        }

        state.mireds = mireds;
        zcctlm_set_duty();
        if (zcctlm_should_persist_state()) {
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_MIREDS, (uint16_t)mireds);
        }
        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_set_on_transition_time(uint16_t time_ms) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        state.on_transition_time = time_ms;
        zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_ON_TRANSITION_TIME, time_ms);
        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_set_off_transition_time(uint16_t time_ms) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        state.off_transition_time = time_ms;
        zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_OFF_TRANSITION_TIME, time_ms);
        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_set_startup_behavior(zcctl_startup_behavior_e startup_behavior) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        state.startup_behavior = startup_behavior;
        zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_STARTUP_ON_OFF, (uint16_t)startup_behavior);

        // save current color, brightness, and state
        if (zcctlm_should_persist_state()) {
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_ON_OFF, (uint16_t)state.on_off);
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_BRIGHTNESS, (uint16_t)state.brightness);
            zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_MIREDS, state.mireds);
        }

        xSemaphoreGive(state_mutex);
    }
}

void zcctlm_clear_nvs() {
    zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_ON_OFF, ZCCTLM_DEFAULT_ONOFF);
    zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_BRIGHTNESS, ZCCTLM_MIN_BRIGHTNESS);
    zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_MIREDS, ZCCTLM_DEFAULT_TEMP);
    zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_STARTUP_ON_OFF, ZCCTLM_DEFAULT_STARTUP_BEHAVIOUR);
    zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_ON_TRANSITION_TIME, ZCCTLM_DEFAULT_TRANSITION_TIME_MS);
    zcctlm_save_to_nvs(ZCCTLM_NVS_KEY_OFF_TRANSITION_TIME, ZCCTLM_DEFAULT_TRANSITION_TIME_MS);
}

void zcctlm_report_current_state() {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY)) {
        // Send attribute values currently in `state`
        zbattr_send_attribute_report(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &state.on_off);
        zbattr_send_attribute_report(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &state.brightness);
        zbattr_send_attribute_report(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &state.mireds);
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire state mutex for reporting");
    }
}

void zcctlm_identify(uint16_t value) {
    if (value == 0) {
        zcctlm_set_duty(); // back to normal state
        return;
    }

    lc_set_duty_warm(LC_MAX_DUTY / 2, 200);
    lc_set_duty_cold(LC_MAX_DUTY / 2, 200);
    // vTaskDelay(pdMS_TO_TICKS(300));
    lc_set_duty_warm(LC_OFF_DUTY, 200);
    lc_set_duty_cold(LC_OFF_DUTY, 200);
}