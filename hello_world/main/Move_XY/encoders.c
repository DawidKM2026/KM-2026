#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "esp_err.h"

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "gpio_config.h"

#include "encoders.h"

typedef struct
{
    gpio_num_t pin_a;
    gpio_num_t pin_b;
} encoder_config_t;

static const encoder_config_t encoder_config[ENCODER_COUNT] =
{
    [ENCODER_X_1] = {
        .pin_a = ENCODER_A_X_1_PIN,
        .pin_b = ENCODER_B_X_1_PIN
    },

    [ENCODER_X_2] = {
        .pin_a = ENCODER_A_X_2_PIN,
        .pin_b = ENCODER_B_X_2_PIN
    },

    [ENCODER_Y] = {
        .pin_a = ENCODER_A_Y_PIN,
        .pin_b = ENCODER_B_Y_PIN
    }
};

static pcnt_unit_handle_t encoder_units[ENCODER_COUNT] = {0};

void motor_encoder_init(
    encoder_id_t encoder){
    
    printf("Encoder init\n");

    
    gpio_num_t pin_a = encoder_config[encoder].pin_a;

    gpio_num_t pin_b = encoder_config[encoder].pin_b;


    pcnt_unit_config_t unit_config = {
        .low_limit = -32768,
        .high_limit = 32767,
    };

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config,&encoder_units[encoder]));

    pcnt_channel_handle_t chan_a = NULL;
    pcnt_channel_handle_t chan_b = NULL;

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = pin_a,
        .level_gpio_num = pin_b,
    };

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = pin_b,
        .level_gpio_num = pin_a,
    };

    ESP_ERROR_CHECK(
        pcnt_new_channel(encoder_units[encoder], &chan_a_cfg, &chan_a));

    ESP_ERROR_CHECK(
        pcnt_new_channel(encoder_units[encoder], &chan_b_cfg, &chan_b));

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

    ESP_ERROR_CHECK(pcnt_unit_enable(encoder_units[encoder]));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(encoder_units[encoder]));
    ESP_ERROR_CHECK(pcnt_unit_start(encoder_units[encoder]));
    printf("encoder_unit=%p\n", encoder_units[encoder]);

int cnt = 0;
esp_err_t err = pcnt_unit_get_count(encoder_units[encoder], &cnt);

printf("PCNT TEST err=%s cnt=%d\n",
       esp_err_to_name(err),
       cnt);
}

int32_t motor_encoder_get_count(encoder_id_t encoder){
    int count = 0;

    if (encoder_units[encoder] != NULL)
    {
        esp_err_t err = pcnt_unit_get_count(encoder_units[encoder], &count);

        if (err != ESP_OK)
        {
            printf("PCNT ERROR: %s\n", esp_err_to_name(err));
        }
    }
    else
    {
        printf("encoder_unit == NULL\n");
    }

    return count;
}

float motor_encoder_get_angle(encoder_id_t encoder)
{
    int count = motor_encoder_get_count(encoder);

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

void motor_encoder_reset_position(encoder_id_t encoder)
{
    if (encoder_units[encoder] != NULL)
    {
        pcnt_unit_clear_count(encoder_units[encoder]);
    }
}