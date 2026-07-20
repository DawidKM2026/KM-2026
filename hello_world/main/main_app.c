#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "config_wifi.h"
#include "esp_now_comm.h"
#include "over_the_air_updates.h"
#include "gpio_config.h"
#include "stepper_motor.h"
#include "encoders.h"


void app_main(void)
{
    // Inicjalizacja pamieci długotrwałej
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    
    //Inicjalizacja podłączenia do AP
    wifi_init_sta();

    // Funkcja do sprawdzania własnego adresu MAC do komunikacji ESP-NOW
    /*
    static const char *TAG_WIFI = "WIFI_STA";
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));

    ESP_LOGI(TAG_WIFI,
             "STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]); */

    
    //Start strony WWW do aktualizacji przez przeglądarkę
    start_webserver();

    //Inicjalizacja  ESP-NOW do przesyłania iformacji o położeniu do panelu operatorskiego
    espnow_init();

    //Inicjalizacja wejść/wyjść
    init_gpio();

    //Inicjalizacja enkoderów
    motor_encoder_init(ENCODER_X_1);
    motor_encoder_init(ENCODER_X_2);
    motor_encoder_init(ENCODER_Y);

    //Inicjalizacja silników krokwych
    init_stepper_motor_timers();

    //Bazowanie
    bool homing_done = motor_homing();
    printf("Bazowanie zakonczone: %d\n", homing_done);

    while (1)
    {
        //Obsługa przycisku na ESP 32 S3 PICO
        motor_button_on_off();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
