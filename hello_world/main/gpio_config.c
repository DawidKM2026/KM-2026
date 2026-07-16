#include "gpio_config.h"

void init_gpio(void)
{
    gpio_set_direction(STEP_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    
    gpio_set_direction(LIMIT_SWITCH_X_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LIMIT_SWITCH_X_PIN, GPIO_PULLUP_ONLY);

    gpio_set_level(DIR_PIN, 1);
}