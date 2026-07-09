/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_err.h"

#include "config_access_point.h"
#include "esp_now_comm.h"

void app_main(void)
{

    //Inicjalizacja pamięci
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);



    //Sprawdzenie swojego adresu MAC do komunikacji ESP-NOW
    /* wifi_init_softap();
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));

    ESP_LOGI(TAG_WIFI,
             "STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]); */

    //Komunikacja ESP-NOW 
    espnow_init();

    //Zadawanie pozcyji
    send_position(100, 200);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        //Zczytywanie aktualnej pozcyji
        get_position();
    }
}
