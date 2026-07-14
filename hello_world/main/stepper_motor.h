#pragma once

#include <stdint.h>

void motor_set_speed(uint32_t motor_rpm);
void init_stepper_motor_timer(void);
void motor_button_on_off(void);
void motor_button_direction(void);
void motor_set_direction(int motor,int direction);
void motor_move_by(int32_t x,int32_t y);
/* Encoder */

void motor_encoder_init(void);
int32_t motor_encoder_get_count(void);
float motor_encoder_get_angle(void);
void motor_encoder_reset_position(void);
