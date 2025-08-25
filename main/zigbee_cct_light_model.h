#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ZCCTLM_MIN_TEMP 167
#define ZCCTLM_MAX_TEMP 370
#define ZCCTLM_DEFAULT_ONOFF 0
#define ZCCTLM_DEFAULT_BRIGHTNESS 51
#define ZCCTLM_MIN_BRIGHTNESS 0
#define ZCCTLM_MAX_BRIGHTNESS 254
#define ZCCTLM_DEFAULT_TRANSITION_TIME_MS 100
#define ZCCTLM_DEFAULT_STARTUP_BEHAVIOUR 0
#define ZCCTLM_DEFAULT_TEMP ((ZCCTLM_MIN_TEMP + ZCCTLM_MAX_TEMP) / 2)

#define ZCCTLM_USE_GAMMA_CORRECTION 1
#define ZCCTLM_GAMMA_CORRECTION 1.5

/*
    This is a workaround that solves the issue of the LED strip briefly flashing with an old color temperature
    when turning on a Zigbee lamp via Home Assistant. After sending the "on" command,
    the ZHA integration would restore the previous brightness and color temperature (mireds)
    before the new values from the UI or scene had time to arrive.
    As a result, the lamp would light up for a brief moment using outdated parameters,
    causing an unwanted flash of the previous color or brightness.

    The workaround works by temporarily blocking any PWM updates (zcctlm_set_duty)
    for a short period (e.g., 200 ms) after receiving the "on" command.
    This gives Home Assistant enough time to send the updated brightness and color temperature values.
    Once the timer expires, duty updates are enabled again and applied based on the latest state.
*/
#define ZCCTLM_ENABLE_ON_OFF_DUTY_BLOCK_WORKAROUND 1
#define ZCCTLM_DUTY_BLOCK_TIME_MS 200

#define ZCCTLM_AVG_TEMP ((ZCCTLM_MIN_TEMP + ZCCTLM_MAX_TEMP) / 2)

typedef enum { ZCCTL_STARTUP_OFF = 0, ZCCTL_STARTUP_ON, ZCCTL_STARTUP_TOGGLE, ZCCTL_STARTUP_PREVIOUS = 255 } zcctl_startup_behavior_e;

void zcctlm_init();
void zcctlm_set_on_off(bool on_off);
void zcctlm_toggle_on_off();
void zcctlm_set_brightness(uint8_t val);
void zcctlm_set_color_temp(uint16_t mireds);
void zcctlm_set_on_transition_time(uint16_t time_ms);
void zcctlm_set_off_transition_time(uint16_t time_ms);
void zcctlm_set_startup_behavior(zcctl_startup_behavior_e startup_behavior);
void zcctlm_clear_nvs();
void zcctlm_report_current_state();
void zcctlm_identify(uint16_t value);