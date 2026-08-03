#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "STATE/system_state.h"

void app_main(void)
{
    xTaskCreate(
        system_state_task,
        "system_state",
        8192,
        NULL,
        5,
        NULL
    );
}