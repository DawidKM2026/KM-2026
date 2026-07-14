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
#include <inttypes.h>

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


//Wywoływanie alarmu po upływie określonego czasu
static bool IRAM_ATTR step_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    step_state = !step_state;
    gpio_set_level(STEP_PIN, step_state);

    return false;
}


//Ustawianie prędkości obrotowej silnika
void motor_set_speed(uint32_t motor_rpm)
{
    if(motor_rpm!=0){

    uint32_t alarm_count =(150000/(motor_rpm*16)); //Mnożenie razy 16 wynika ze step mode 1/16
    
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

//Inicjalizacja
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
        .alarm_count = 5000,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            timer,
            &alarm_config));
    
    gpio_set_level(EN_PIN, 0);

    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));

    
    for (int rpm = 15; rpm <= 30; rpm += 15){
            motor_set_speed(rpm);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

//Switch do właczania/wyłączania silnika 
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

//Switch do zmiany  kierunku
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

//Ustawianie kierunku silnika
typedef enum
{
    MOTOR_SURGE,
    MOTOR_SWAY,
} motor_t;

/* void motor_set_direction(motor_t motor,bool direction){
    switch(motor){

        default:
        printf("Zadano błędną wartość"
        );
        break;

        //Surge
        case MOTOR_SURGE:
        gpio_set_level(SURGE_1_DIR_PIN, direction);
        gpio_set_level(SURGE_2_DIR_PIN, !direction);
        printf("Kierunek: %s\n", direction ? "przód" : "tył");
        break;
        
        //Sway
        case MOTOR_SWAY:
        gpio_set_level(SWAY_DIR_PIN, direction);
        printf("Kierunek: %s\n", direction ? "tył" : "przód");
        break;
    }
} */


void motor_move_by(int32_t x, int32_t y)
{

    // Surge Forward
    if (x > 0)
    {
        setSurgeDirection(przód);

        int32_t start_position = motor_encoder_get_count();
        int32_t target_position = start_position + x;

        motor_set_speed(45);

        while (motor_encoder_get_count() < target_position)
        {
            current_x = motor_encoder_get_count();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_set_speed(0);
    }

    // Surge Backward
    else if (x < 0)
    {
        setSurgeDirection(tył);

        int32_t start_position = motor_encoder_get_count();
        int32_t target_position = start_position + x;

        motor_set_speed(45);

        while (motor_encoder_get_count() > target_position)
        {
            current_x = motor_encoder_get_count();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_set_speed(0);
    }

    // Sway Forward
    if (y > 0)
    {
        setSwayDirection(przód);

        int32_t start_position = motor_encoder_get_count();
        int32_t target_position = start_position + y;

        motor_set_speed(45);

        while (motor_encoder_get_count() < target_position)
        {
            current_y = motor_encoder_get_count();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_set_speed(0);
    }

    // Sway Backward
    else if (y < 0)
    {
        setSwayDirection(tył);

        int32_t start_position = motor_encoder_get_count();
        int32_t target_position = start_position + y;

        motor_set_speed(45);

        while (motor_encoder_get_count() > target_position)
        {
            current_y = motor_encoder_get_count();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_set_speed(0);
    }
} */























//------------------------------------ Encoder ---------------------------------------------------
#include <math.h>
#include "driver/pulse_cnt.h"

#define ENCODER_A_GPIO GPIO_NUM_4
#define ENCODER_B_GPIO GPIO_NUM_5

#define ENCODER_CPR             200
#define ENCODER_COUNTS_PER_REV (ENCODER_CPR * 4)

static pcnt_unit_handle_t encoder_unit = NULL;

void motor_encoder_init(void)
{
    pcnt_unit_config_t unit_config = {
        .low_limit = -32768,
        .high_limit = 32767,
    };

    ESP_ERROR_CHECK(
        pcnt_new_unit(&unit_config, &encoder_unit));

    pcnt_channel_handle_t chan_a = NULL;
    pcnt_channel_handle_t chan_b = NULL;

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = ENCODER_A_GPIO,
        .level_gpio_num = ENCODER_B_GPIO,
    };

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = ENCODER_B_GPIO,
        .level_gpio_num = ENCODER_A_GPIO,
    };

    ESP_ERROR_CHECK(
        pcnt_new_channel(encoder_unit, &chan_a_cfg, &chan_a));

    ESP_ERROR_CHECK(
        pcnt_new_channel(encoder_unit, &chan_b_cfg, &chan_b));

    // kanał A
    pcnt_channel_set_edge_action(
        chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE);

    pcnt_channel_set_level_action(
        chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // kanał B
    pcnt_channel_set_edge_action(
        chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);

    pcnt_channel_set_level_action(
        chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    ESP_ERROR_CHECK(pcnt_unit_enable(encoder_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(encoder_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(encoder_unit));
}

int32_t motor_encoder_get_count(void)
{
    int count = 0;

    if (encoder_unit != NULL)
    {
        pcnt_unit_get_count(encoder_unit, &count);
    }

    return count;
}

float motor_encoder_get_angle(void)
{
    int count = motor_encoder_get_count();

    float angle =
        ((float)count * 360.0f) /
        (float)ENCODER_COUNTS_PER_REV;

    angle = fmodf(angle, 360.0f);

    if (angle < 0)
    {
        angle += 360.0f;
    }

    return angle;
}

void motor_encoder_reset_position(void)
{
    if (encoder_unit != NULL)
    {
        pcnt_unit_clear_count(encoder_unit);
    }
}
//