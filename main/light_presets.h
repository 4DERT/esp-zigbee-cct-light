#pragma once

#include <stdint.h>

typedef struct {
    uint16_t mireds;
    uint8_t brightness;
} light_preset_t;

void light_presets_cycle();