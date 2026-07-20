#pragma once

#include "driver/gpio.h"

#define BUTTON_PIN GPIO_NUM_0

#define STEP_X_1_PIN   GPIO_NUM_1
#define DIR_X_1_PIN    GPIO_NUM_2
#define EN_X_1_PIN     GPIO_NUM_3

#define STEP_X_2_PIN   GPIO_NUM_4
#define DIR_X_2_PIN    GPIO_NUM_5
#define EN_X_2_PIN     GPIO_NUM_6

#define STEP_Y_PIN   GPIO_NUM_7
#define DIR_Y_PIN    GPIO_NUM_8
#define EN_Y_PIN     GPIO_NUM_9

#define ENCODER_A_X_1_PIN GPIO_NUM_10
#define ENCODER_B_X_1_PIN GPIO_NUM_11
#define LIMIT_SWITCH_X_1_PIN GPIO_NUM_12

#define ENCODER_A_X_2_PIN GPIO_NUM_13
#define ENCODER_B_X_2_PIN GPIO_NUM_14
#define LIMIT_SWITCH_X_2_PIN GPIO_NUM_15

#define ENCODER_A_Y_PIN GPIO_NUM_16
#define ENCODER_B_Y_PIN GPIO_NUM_17
#define LIMIT_SWITCH_Y_PIN GPIO_NUM_18

void init_gpio(void);