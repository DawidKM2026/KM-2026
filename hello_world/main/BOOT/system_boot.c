#include "system_boot.h"

#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "config_wifi.h"
#include "esp_now_comm.h"
#include "over_the_air_updates.h"

#include "gpio_config.h"
#include "stepper_motor.h"
#include "encoders.h"

static const char *TAG = "SYSTEM_BOOT";

/* -------------------------------------------------------------------------- */

void system_init(void)
{
    ESP_LOGI(TAG, "GPIO init");
    init_gpio();

    ESP_LOGI(TAG, "Encoders init");
    motor_encoder_init(ENCODER_X_1);
    motor_encoder_init(ENCODER_X_2);
    motor_encoder_init(ENCODER_Y);

    ESP_LOGI(TAG, "Stepper init");
    init_stepper_motor_timers();
}

/* -------------------------------------------------------------------------- */

void communication_init(void)
{
    ESP_LOGI(TAG, "WiFi init");
    wifi_init_sta();

    ESP_LOGI(TAG, "OTA server init");
    start_webserver();

    ESP_LOGI(TAG, "ESP-NOW init");
    espnow_init();
}

/* -------------------------------------------------------------------------- */

void system_boot(void)
{
    ESP_LOGI(TAG, "System boot start");

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    system_init();

    communication_init();

    ESP_LOGI(TAG, "System boot complete");
}