#include "zb_attr_handlers.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

#include "led_controller.h"
#include "zigbee_cct_light_model.h"

static const char *TAG = "zbapp handlers";

static void handle_on_off_attribute(const esp_zb_zcl_set_attr_value_message_t *message);
static void handle_color_control_attribute(const esp_zb_zcl_set_attr_value_message_t *message);
static void handle_level_control_attribute(const esp_zb_zcl_set_attr_value_message_t *message);
static void handle_identify_attribute(const esp_zb_zcl_set_attr_value_message_t *message);

esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);

    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        switch (message->info.cluster) {
        case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
            handle_on_off_attribute(message);
            break;

        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
            handle_level_control_attribute(message);
            break;

        case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
            handle_color_control_attribute(message);
            break;

        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
            handle_identify_attribute(message);
            break;

        default:
            ESP_LOGW(TAG, "(zb_attribute_handler) -> Received unhandled message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
                     message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);
            break;
        }
    }
    return ret;
}

static void handle_on_off_attribute(const esp_zb_zcl_set_attr_value_message_t *message) {
    bool light_state = 0;
    uint8_t startup_on_off = 0;

    switch (message->attribute.id) {
    // Standard ON/OFF state attribute (true = ON, false = OFF)
    case ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
        if (message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            light_state = *(bool *)message->attribute.data.value;
            ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
            zcctlm_set_on_off(light_state);
        } else {
            ESP_LOGW(TAG, "Invalid type for ON_OFF attribute: 0x%x", message->attribute.data.type);
        }
        break;

    // Determines the power-on behavior (0 = off, 1 = on, 2 = previous state)
    case ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF:
        if (message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
            startup_on_off = *(uint8_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "StartUpOnOff changed to %d", startup_on_off);
            zcctlm_set_startup_behavior((zcctl_startup_behavior_e)startup_on_off);
        } else {
            ESP_LOGW(TAG, "Invalid type for StartUpOnOff: 0x%x", message->attribute.data.type);
        }
        break;

    default:
        ESP_LOGW(TAG, "Unhandled On/Off attribute: 0x%x", message->attribute.id);
        break;
    }
}

static void handle_color_control_attribute(const esp_zb_zcl_set_attr_value_message_t *message) {
    switch (message->attribute.id) {

    // 0x0007 - Current color temperature in mireds (e.g. 2700K = ~370)
    case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID:
        if (message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            uint16_t color_temperature = *(uint16_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Color temperature set to %u mireds", color_temperature);
            zcctlm_set_color_temp(color_temperature);
        } else {
            ESP_LOGW(TAG, "Invalid type for ColorTemperature: 0x%x", message->attribute.data.type);
        }
        break;

    default:
        ESP_LOGW(TAG, "Unhandled Color Control attribute: 0x%x", message->attribute.id);
        break;
    }
}

static void handle_level_control_attribute(const esp_zb_zcl_set_attr_value_message_t *message) {
    switch (message->attribute.id) {

    case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID:
        if (message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            uint8_t curent_level = *(uint8_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Current level set to %u", curent_level);
            zcctlm_set_brightness(curent_level);
        } else {
            ESP_LOGW(TAG, "Invalid type for CurrentLevel: 0x%x", message->attribute.data.type);
        }
        break;

    case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_TRANSITION_TIME_ID:
        if (message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            uint16_t on_transition_time = *(uint16_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "On transition time set to %u", on_transition_time);
            zcctlm_set_on_transition_time(on_transition_time);
        } else {
            ESP_LOGW(TAG, "Invalid type for OnTransitionTime: 0x%x", message->attribute.data.type);
        }
        break;

    case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_OFF_TRANSITION_TIME_ID:
        if (message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            uint16_t off_transition_time = *(uint16_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Off transition time set to %u", off_transition_time);
            zcctlm_set_off_transition_time(off_transition_time);
        } else {
            ESP_LOGW(TAG, "Invalid type for OffTransitionTime: 0x%x", message->attribute.data.type);
        }
        break;

    default:
        ESP_LOGW(TAG, "Unhandled Level Control attribute: 0x%x", message->attribute.id);
        break;
    }
}

static void handle_identify_attribute(const esp_zb_zcl_set_attr_value_message_t *message) {
    ESP_LOGI(TAG, "Identify: %u", *(uint16_t *)message->attribute.data.value);
    zcctlm_identify(*(uint16_t *)message->attribute.data.value);
}