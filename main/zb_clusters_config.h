#pragma once

#pragma once

#include "ha/esp_zigbee_ha_standard.h"

// esp_zb_attribute_list_t *zb_create_basic_cluster(void);
// esp_zb_attribute_list_t *zb_create_identify_cluster(void);
// esp_zb_attribute_list_t *zb_create_onoff_cluster(void);
// esp_zb_attribute_list_t *zb_create_color_control_cluster(void);
// esp_zb_attribute_list_t *zb_create_level_control_cluster(void);

esp_zb_cluster_list_t *zb_create_cluster_list(void); 
