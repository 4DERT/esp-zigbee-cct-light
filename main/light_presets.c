#include "light_presets.h"

#include "zigbee_cct_light_model.h"

const light_preset_t light_presets[] = {
    {ZCCTLM_MAX_TEMP, ZCCTLM_MAX_BRIGHTNESS / 5}, // 2700K, ~20%
    {ZCCTLM_MAX_TEMP, ZCCTLM_MAX_BRIGHTNESS / 2}, // 2700K, ~50%
    {ZCCTLM_MAX_TEMP, ZCCTLM_MAX_BRIGHTNESS},     // 2700K, ~100%
    {ZCCTLM_AVG_TEMP, ZCCTLM_MAX_BRIGHTNESS / 5}, // 3500K, ~30%
    {ZCCTLM_AVG_TEMP, ZCCTLM_MAX_BRIGHTNESS / 2}, // 3500K, ~50%
    {ZCCTLM_AVG_TEMP, ZCCTLM_MAX_BRIGHTNESS},     // 3500K, ~100%
    {ZCCTLM_MIN_TEMP, ZCCTLM_MAX_BRIGHTNESS / 5}, // 5000K, ~20%
    {ZCCTLM_MIN_TEMP, ZCCTLM_MAX_BRIGHTNESS / 2}, // 5000K, ~50%
    {ZCCTLM_MIN_TEMP, ZCCTLM_MAX_BRIGHTNESS},     // 5000K, ~100%
};

const uint8_t light_presets_count = sizeof(light_presets) / sizeof(light_presets[0]);

static uint8_t current_preset_index = 0;

void light_presets_cycle() {
    light_preset_t preset = light_presets[current_preset_index];

    zcctlm_set_brightness(preset.brightness);
    zcctlm_set_color_temp(preset.mireds);
    zcctlm_report_current_state();

    ++current_preset_index;
    if (current_preset_index >= light_presets_count)
        current_preset_index = 0;
}