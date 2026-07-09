/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "config_wifi.h"
#include "esp_now_comm.h"
#include "over_the_air_updates.h"


void app_main(void)
{

    //Inicjalizacja pamieci długotrwałej
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    // Sprawdzanie aktywnej partycji
    const esp_partition_t *running = esp_ota_get_running_partition();
    printf("Running partition: %s\n", running->label);


    //Inicjalizacja podłączenia do AP
    wifi_init_sta();


    //Funkcja do sprawdzania własnego adresu MAC do komunikacji ESP-NOW
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

    //Obsługa  ESP-NOW do przesyłania iformacji o położeniu do panelu operatorskiego
    espnow_init();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
