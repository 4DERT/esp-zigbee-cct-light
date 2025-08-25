#include "led_controller.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

// Number of channels (warm + cold = 2)
#define LEDC_CH_NUM (2)

static const char *TAG = "LEDC";

typedef struct {
    uint16_t duty;
    uint32_t fade_time;
} lc_job_params_t;

typedef struct {
    const char *label;
    ledc_channel_config_t config;
    QueueHandle_t queue;
    TaskHandle_t task;
} lc_channel_t;

static lc_channel_t lc_channels[LEDC_CH_NUM];
/*
 * Prepare and set configuration of timers
 * that will be used by LED Controller
 */
ledc_timer_config_t ledc_timer = {
    .duty_resolution = LC_DUTY_RESOLUTION, // resolution of PWM duty
    .freq_hz = LC_FREQUENCY,               // frequency of PWM signal
    .speed_mode = LC_LS_MODE,              // timer mode
    .timer_num = LC_LS_TIMER,              // timer index
    .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
};

/*
 * This callback function will be called when fade operation has ended
 * Use callback only if you are aware it is being called inside an ISR
 * Otherwise, you can use a semaphore to unblock tasks
 */
static IRAM_ATTR bool on_ledc_fade_end_event(const ledc_cb_param_t *param, void *user_arg) { return true; }

static void lc_leds_task(void *params) {
    lc_channel_t *chan = (lc_channel_t *)params;
    QueueHandle_t q = chan->queue;

    // Init
    // Set LED Controller with previously prepared configuration
    ledc_channel_config(&chan->config);

    ledc_cbs_t callbacks = {.fade_cb = on_ledc_fade_end_event};
    ledc_cb_register(chan->config.speed_mode, chan->config.channel, &callbacks, NULL);

    ledc_set_duty(chan->config.speed_mode, chan->config.channel, LC_OFF_DUTY);
    ledc_update_duty(chan->config.speed_mode, chan->config.channel);

    lc_job_params_t job_params;
    while (1) {
        if (xQueueReceive(q, &job_params, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "(%s) -> Setting led to %" PRIu16 " duty in %" PRIu32 "ms", chan->label, job_params.duty, job_params.fade_time);

            if (job_params.fade_time == 0 || job_params.fade_time > LC_FADE_MAX_TIME_MS) {
                ledc_set_duty(chan->config.speed_mode, chan->config.channel, job_params.duty);
                ledc_update_duty(chan->config.speed_mode, chan->config.channel);
            } else {
                ledc_set_fade_with_time(chan->config.speed_mode, chan->config.channel, job_params.duty, job_params.fade_time);
                ledc_fade_start(chan->config.speed_mode, chan->config.channel, LEDC_FADE_WAIT_DONE);
            }
        }
    }
}

static void lc_set_duty_generic(lc_channel_t *chan, uint16_t duty, uint16_t fade_time) {
    lc_job_params_t params = {.duty = duty, .fade_time = fade_time};
#if LC_QUEUE_SIZE == 1
    BaseType_t ret = xQueueOverwrite(chan->queue, &params);
#else
    BaseType_t ret = xQueueSend(chan->queue, &params, 0);
#endif
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Error while adding %s leds job", chan->label);
    }
}

// --- PUBLIC ---

void lc_init() {
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);
    ESP_LOGI(TAG, "LEDC timer configured: frequency=%" PRIu32 "Hz, resolution=%" PRIu16 " bits", ledc_timer.freq_hz, ledc_timer.duty_resolution);

    // Initialize fade service.
    ledc_fade_func_install(0);

    lc_channels[0] = (lc_channel_t){.label = "warm",
                                    .config =
                                        {
                                            .channel = LC_WARM_CHANNEL,
                                            .gpio_num = LC_WARM_GPIO,
                                            .duty = 0,
                                            .speed_mode = LC_LS_MODE,
                                            .hpoint = 0,
                                            .timer_sel = LC_LS_TIMER,
                                            .flags.output_invert = 0,
                                        },
                                    .queue = xQueueCreate(LC_QUEUE_SIZE, sizeof(lc_job_params_t))};

    lc_channels[1] = (lc_channel_t){.label = "cold",
                                    .config =
                                        {
                                            .channel = LC_COLD_CHANNEL,
                                            .gpio_num = LC_COLD_GPIO,
                                            .duty = 0,
                                            .speed_mode = LC_LS_MODE,
                                            .hpoint = 0,
                                            .timer_sel = LC_LS_TIMER,
                                            .flags.output_invert = 0,
                                        },
                                    .queue = xQueueCreate(LC_QUEUE_SIZE, sizeof(lc_job_params_t))};

    for (int i = 0; i < LEDC_CH_NUM; i++) {
        xTaskCreate(lc_leds_task, lc_channels[i].label, 3072, &lc_channels[i], 1, &lc_channels[i].task);
    }
}

void lc_set_duty_warm(uint16_t duty, uint16_t fade_time) { lc_set_duty_generic(&lc_channels[0], duty, fade_time); }

void lc_set_duty_cold(uint16_t duty, uint16_t fade_time) { lc_set_duty_generic(&lc_channels[1], duty, fade_time); }