#pragma once

#include "driver/ledc.h"

#define LC_LS_TIMER LEDC_TIMER_1
#define LC_LS_MODE LEDC_LOW_SPEED_MODE

// Warm LED strip chanel define
#define LC_WARM_GPIO (7)
#define LC_WARM_CHANNEL LEDC_CHANNEL_0

// Cold LED strip chanel define
#define LC_COLD_GPIO (21)
#define LC_COLD_CHANNEL LEDC_CHANNEL_1

// Duty resolution
#define LC_DUTY_RESOLUTION LEDC_TIMER_11_BIT

#define LC_OFF_DUTY 0
#define LC_MIN_DUTY 150
#define LC_MAX_DUTY (1 << LC_DUTY_RESOLUTION)

// Frequency in Hz
#define LC_FREQUENCY (25000)

// Queue size
// Set to 1 for real-time behavior: xQueueOverwrite() will be used to always keep the latest command
// Set >1 to buffer multiple LED jobs (slower but preserves intermediate states)
#define LC_QUEUE_SIZE 16

// Fade max time
#define LC_FADE_MAX_TIME_MS 5000

void lc_init();
void lc_set_duty_warm(uint16_t duty, uint16_t fade_time);
void lc_set_duty_cold(uint16_t duty, uint16_t fade_time);