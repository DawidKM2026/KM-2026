#pragma once

#include "driver/gpio.h"

#define BUTTON_PIN GPIO_NUM_0
#define STEP_PIN   GPIO_NUM_4
#define DIR_PIN    GPIO_NUM_5
#define EN_PIN     GPIO_NUM_6
#define ENCODER_A_GPIO GPIO_NUM_7
#define ENCODER_B_GPIO GPIO_NUM_8
#define LIMIT_SWITCH_X_PIN GPIO_NUM_39

void init_gpio(void);