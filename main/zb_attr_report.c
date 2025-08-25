#include "zb_attr_report.h"

#include "esp_log.h"
#include "zb_app.h"

static const char *TAG = "zb attr report";

void zbattr_send_attribute_report(uint16_t cluster_id, uint16_t attr_id, void *value) {
    // Check if connected to network
    if (!appzb_is_connected()) {
        ESP_LOGW(TAG, "Failed to send report, device is not connected to network");
        return;
    }

    // Save attribute localy
    esp_zb_zcl_status_t status =
        esp_zb_zcl_set_attribute_val(HA_ESP_LIGHT_ENDPOINT, cluster_id, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, value, false);

    if (status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "Failed to set attribute 0x%04X in cluster 0x%04X", attr_id, cluster_id);
        return;
    }

    // Prepare to send
    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd =
            {
                .dst_endpoint = 1,
                .src_endpoint = HA_ESP_LIGHT_ENDPOINT,
            },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT,
        .clusterID = cluster_id,
        .manuf_specific = 0,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .dis_defalut_resp = 1,
        .manuf_code = 0,
        .attributeID = attr_id,
    };

    // send report
    esp_err_t err = esp_zb_zcl_report_attr_cmd_req(&report_cmd);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send report for attribute 0x%04X in cluster 0x%04X", attr_id, cluster_id);
        return;
    }

    ESP_LOGI(TAG, "Sent report: cluster 0x%04X, attr 0x%04X", cluster_id, attr_id);
}