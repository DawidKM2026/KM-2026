#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MOTOR_SURGE,
    MOTOR_SWAY
} motor_t;

void init_stepper_motor_timer(void);

void motor_set_speed(uint32_t motor_rpm);
void motor_set_direction(motor_t motor, bool direction);

void motor_button_on_off(void);

void motor_move_to(int32_t x_mm, int32_t y_mm);
void motor_move_by(int32_t wychylenie_x, int32_t wychylenie_y);

void motor_test(void);

/* Enkoder */
void motor_encoder_init(void);
int32_t motor_encoder_get_count(void);
float motor_encoder_get_angle(void);
void motor_encoder_reset_position(void);