#pragma once

#include <stdint.h>

void init_stepper_motor_timer(void);
void motor_button_on_off(void);
void motor_button_direction(void);
void motor_set_speed(uint32_t motor_rps);   