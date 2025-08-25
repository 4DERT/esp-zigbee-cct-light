#include "zb_app.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "ha/esp_zigbee_ha_standard.h"

#include "led_controller.h"
#include "zb_attr_handlers.h"
#include "zb_clusters_config.h"
#include "zigbee_cct_light_model.h"

static const char *TAG = "zbapp";

#define CONNECTED_BIT BIT0

static EventGroupHandle_t connected_event_group;

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
                xEventGroupSetBits(connected_event_group, CONNECTED_BIT);
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG,
                     "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short "
                     "Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2],
                     extended_pan_id[1], extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            xEventGroupSetBits(connected_event_group, CONNECTED_BIT);
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE: // End Device + Router
        ESP_LOGI(TAG, "Factory resetting Zigbee stack, device will reboot!");
        zcctlm_clear_nvs();
        esp_zb_factory_reset();
        break;
#if USE_DEEP_SLEEP
    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        ESP_LOGV(TAG, "Zigbee can sleep");
        break;
#endif
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;

    default:
        ESP_LOGW(TAG, "(zb_action_handler) -> Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters) {
    /* initialize Zigbee stack with Zigbee end-device config */
    // esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
#if USE_DEEP_SLEEP
    esp_zb_sleep_enable(true);
#endif
    esp_zb_init(&zb_nwk_cfg);

    // Endpoint
    esp_zb_cluster_list_t *cluster_list = zb_create_cluster_list();
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,
        .app_device_version = 4,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, ep_cfg);

    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

// public
void appzb_init() {
    connected_event_group = xEventGroupCreate();

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    /* load Zigbee platform config to initialization */
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL);
}

bool appzb_is_connected() {
    EventBits_t bits = xEventGroupGetBits(connected_event_group);
    return (bits & CONNECTED_BIT) != 0;
}

void appzb_wait_until_connected() { xEventGroupWaitBits(connected_event_group, CONNECTED_BIT, false, true, portMAX_DELAY); }

void appzb_factory_reset() {
    ESP_LOGI(TAG, "Factory resetting Zigbee stack, device will reboot!");
    zcctlm_clear_nvs();
    esp_zb_factory_reset();
}