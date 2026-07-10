//1 obrót = około 12.5 mm
/*
 * Timer GPTimer:
 * resolution_hz = 1 000 000 Hz
 *
 * STEP jest przełączany przy każdym alarmie:
 * step_state = !step_state;
 *
 * 1 krok silnika = 2 alarmy
 * Silnik: 1.8° = 200 kroków/obrót
 * Śruba: 12.5 mm/obrót
 *
 * alarm_count | RPM | mm/s
 * ------------+-----+------
 *     10000   |  15 |  3.1
 *      5000   |  30 |  6.3
 *      2500   |  60 | 12.5
 *      2000   |  75 | 15.6
 *      1250   | 120 | 25.0
 *      1000   | 150 | 31.3
 *       625   | 240 | 50.0
 *       500   | 300 | 62.5
 *       250   | 600 | 125.0
 *
 * Wzory:
 * kroki_s     = 1000000 / (2 * alarm_count)
 * RPM         = kroki_s * 60 / 200
 * predkosc_mm = RPM * 12.5 / 60
 */


#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "esp_err.h"

#include "gpio_config.h"
#include "stepper_motor.h"


static bool step_state = false;
static bool enabled = true;
static bool last_state = true;

static bool IRAM_ATTR step_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    step_state = !step_state;
    gpio_set_level(STEP_PIN, step_state);

    return false;
}

void init_stepper_motor_timer(void)
{
    gptimer_handle_t timer = NULL;

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    ESP_ERROR_CHECK(
        gptimer_new_timer(
            &timer_config,
            &timer));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = step_timer_callback,
    };

    ESP_ERROR_CHECK(
        gptimer_register_event_callbacks(
            timer,
            &callbacks,
            NULL));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 2500,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            timer,
            &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));

    gpio_set_level(EN_PIN, 0);
}

void motor_button_service(void)
{
    bool state = gpio_get_level(BUTTON_PIN);

    if (last_state == 1 && state == 0)
    {
        enabled = !enabled;

        if (enabled)
        {
            gpio_set_level(EN_PIN, 0);
            printf("Silnik ON\n");
        }
        else
        {
            gpio_set_level(EN_PIN, 1);
            printf("Silnik OFF\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_state = state;
}