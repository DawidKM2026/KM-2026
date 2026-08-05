#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "STATE/system_state.h"

void app_main(void)
{
    // Ustawienie poziomu logowania na ESP_LOG_NONE, aby wyłączyć logi
    esp_log_level_set("*", ESP_LOG_NONE);

    xTaskCreate(
        system_state_task,
        "system_state",
        8192,
        NULL,
        5,
        NULL
    );
}