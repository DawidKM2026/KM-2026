#include "gpio_config.h"

void init_gpio(void)
{
    gpio_set_direction(STEP_X_1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_X_1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_X_1_PIN, GPIO_MODE_OUTPUT);
    
    gpio_set_direction(STEP_X_2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_X_2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_X_2_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(STEP_Y_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_Y_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_Y_PIN, GPIO_MODE_OUTPUT);

    //Przycisk na ESP 32 S3 PICO
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    //Krańcówki
    gpio_set_direction(LIMIT_SWITCH_X_1_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LIMIT_SWITCH_X_1_PIN, GPIO_PULLUP_ONLY);
    
    gpio_set_direction(LIMIT_SWITCH_X_2_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LIMIT_SWITCH_X_2_PIN, GPIO_PULLUP_ONLY);
    
    gpio_set_direction(LIMIT_SWITCH_Y_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LIMIT_SWITCH_Y_PIN, GPIO_PULLUP_ONLY);
}