#include "zb_clusters_config.h"

#include "zigbee_cct_light_model.h"

esp_zb_attribute_list_t *zb_create_basic_cluster(void) {
    esp_zb_basic_cluster_cfg_t cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x03,
    };
    static uint32_t ApplicationVersion = 0x0001;
    static uint32_t StackVersion = 0x0002;
    static uint32_t HWVersion = 0x0002;
    static uint8_t ManufacturerName[] = {5, '4', 'D', 'E', 'R', 'T'};
    static uint8_t ModelIdentifier[] = {4, 'L', 'a', 'm', 'p'};
    static uint8_t DateCode[] = {8, '2', '0', '2', '5', '0', '7', '2', '7'};

    esp_zb_attribute_list_t *cl = esp_zb_basic_cluster_create(&cfg);
    esp_zb_basic_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &ApplicationVersion);
    esp_zb_basic_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &StackVersion);
    esp_zb_basic_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &HWVersion);
    esp_zb_basic_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ManufacturerName);
    esp_zb_basic_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ModelIdentifier);
    esp_zb_basic_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, DateCode);
    return cl;
}

esp_zb_attribute_list_t *zb_create_identify_cluster(void) {
    esp_zb_identify_cluster_cfg_t cfg = {.identify_time = 0};
    return esp_zb_identify_cluster_create(&cfg);
}

esp_zb_attribute_list_t *zb_create_onoff_cluster(void) {
    esp_zb_on_off_cluster_cfg_t cfg = {.on_off = ZCCTLM_DEFAULT_ONOFF};
    esp_zb_attribute_list_t *cl = esp_zb_on_off_cluster_create(&cfg);

    static uint16_t startup_on_off = ZCCTLM_DEFAULT_STARTUP_BEHAVIOUR;
    esp_zb_on_off_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF, &startup_on_off);
    return cl;
}

esp_zb_attribute_list_t *zb_create_color_control_cluster(void) {
    esp_zb_color_cluster_cfg_t cfg = {
        .current_x = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE,
        .current_y = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE,
        .color_mode = 0x0002,
        .options = ESP_ZB_ZCL_COLOR_CONTROL_OPTIONS_DEFAULT_VALUE,
        .enhanced_color_mode = ESP_ZB_ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_DEFAULT_VALUE,
        .color_capabilities = 0x0010,
    };
    esp_zb_attribute_list_t *cl = esp_zb_color_control_cluster_create(&cfg);

    static uint16_t color_attr = ZCCTLM_DEFAULT_TEMP;
    static uint16_t min_temp = ZCCTLM_MIN_TEMP;
    static uint16_t max_temp = ZCCTLM_MAX_TEMP;
    esp_zb_color_control_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &color_attr);
    esp_zb_color_control_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_temp);
    esp_zb_color_control_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_temp);
    return cl;
}

esp_zb_attribute_list_t *zb_create_level_control_cluster(void) {
    esp_zb_attribute_list_t *cl = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL);

    static uint8_t level = ZCCTLM_DEFAULT_BRIGHTNESS;
    static uint16_t off_transition_time = ZCCTLM_DEFAULT_TRANSITION_TIME_MS;
    static uint16_t on_transition_time = ZCCTLM_DEFAULT_TRANSITION_TIME_MS;

    esp_zb_level_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &level);
    esp_zb_level_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_OFF_TRANSITION_TIME_ID, &off_transition_time);
    esp_zb_level_cluster_add_attr(cl, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_TRANSITION_TIME_ID, &on_transition_time);
    return cl;
}

esp_zb_attribute_list_t *zb_create_groups_cluster(void) {
    return esp_zb_groups_cluster_create(NULL);
}

esp_zb_cluster_list_t *zb_create_cluster_list(void) {
    esp_zb_cluster_list_t *list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(list, zb_create_basic_cluster(), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(list, zb_create_identify_cluster(), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(list, zb_create_groups_cluster(), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(list, zb_create_onoff_cluster(), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(list, zb_create_level_control_cluster(), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(list, zb_create_color_control_cluster(), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    return list;
}
