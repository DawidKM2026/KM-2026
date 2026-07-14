#pragma once

#include "driver/gpio.h"

#define STEP_PIN   GPIO_NUM_4
#define DIR_PIN    GPIO_NUM_5
#define EN_PIN     GPIO_NUM_6
#define BUTTON_PIN GPIO_NUM_0
#define DIRECTION_BUTTON_PIN GPIO_NUM_39

void init_gpio(void);