// 1 obrót = około 12.5 mm
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
 
 alarm_count | RPS  | RPM | mm/s
------------+------+-----+-------
    10000   | 0.25 |  15 |   3.1
     5000   | 0.50 |  30 |   6.3
     2500   | 1.00 |  60 |  12.5
     2000   | 1.25 |  75 |  15.6
     1250   | 2.00 | 120 |  25.0
     1000   | 2.50 | 150 |  31.3
      625   | 4.00 | 240 |  50.0 
      500   | 5.00 | 300 |  62.5
      250   |10.00 | 600 | 125.0
 
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

static gptimer_handle_t timer = NULL;
static bool step_state = false;
static bool enabled = true;
static bool last_state = true;
static bool enabled_direction = true;
static int last_state_direction = 1;

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
        .alarm_count = 1000,
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

void motor_button_on_off(void)
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

void motor_button_direction(void)
{
    bool state = gpio_get_level(DIRECTION_BUTTON_PIN);

    if (last_state_direction == 1 && state == 0)
    {
        enabled_direction = !enabled_direction;

        if (enabled_direction)
        {
            gpio_set_level(DIR_PIN, 0);
            printf("Silnik przód\n");
        }
        else
        {
            gpio_set_level(DIR_PIN, 1);
            printf("Silnik tył\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_state_direction = state;
}

void motor_set_speed(uint32_t motor_rpm)
{
    if(motor_rpm!=0){

    uint32_t alarm_count =(150000/motor_rpm);
    
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = alarm_count,
        .flags.auto_reload_on_alarm = true,
    };
    
    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            timer,
            &alarm_config));
    }else{
        
            gpio_set_level(EN_PIN, 1);
    }
    printf("RPM = %" PRIu32 "\n", motor_rpm);

}
