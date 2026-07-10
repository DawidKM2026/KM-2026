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
#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "config_wifi.h"
#include "esp_now_comm.h"
#include "over_the_air_updates.h"

#define STEP_PIN GPIO_NUM_4
#define DIR_PIN GPIO_NUM_5
#define EN_PIN GPIO_NUM_6
#define BUTTON_PIN GPIO_NUM_15

//1 obrót = około 12.5 mm
/*
 * Timer GPTimer:
 * resolution_hz = 1 000 000 Hz
 *
 * STEP jest przełączany przy każdym alarmie:
 * step_state = !step_state;
 *
 * 1 krok silnika = 2 alarmy
 * Silnik: 1.8° = 200 kroków/obrót
 * Śruba: 12.5 mm/obrót
 *
 * alarm_count | RPM | mm/s
 * ------------+-----+------
 *     10000   |  15 |  3.1
 *      5000   |  30 |  6.3
 *      2500   |  60 | 12.5
 *      2000   |  75 | 15.6
 *      1250   | 120 | 25.0
 *      1000   | 150 | 31.3
 *       625   | 240 | 50.0
 *       500   | 300 | 62.5
 *       250   | 600 | 125.0
 *
 * Wzory:
 * kroki_s     = 1000000 / (2 * alarm_count)
 * RPM         = kroki_s * 60 / 200
 * predkosc_mm = RPM * 12.5 / 60
 */

//Silnik
static bool step_state = false;

static bool IRAM_ATTR step_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    step_state = !step_state;
    gpio_set_level(STEP_PIN, step_state);

    return false;
}

static bool enabled = true;
static bool last_state = 1;

int speed = 100; //[obr/sek]
int direction = 1;


void uart_task(void *pvParameters)
{
    char cmd[32];

    while (1)
    {
        if (fgets(cmd, sizeof(cmd), stdin))
        {
            printf("Odebrano: %s", cmd);

            if (cmd[0] == 'f')
                direction = 1;

            if (cmd[0] == 'b')
                direction = -1;

            if (sscanf(cmd, "%d", &speed) == 1)
                printf("Nowa predkosc: %d\n", speed);
        }
    }
}

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

    // Sprawdzanie aktywnej partycji
    const esp_partition_t *running = esp_ota_get_running_partition();
    printf("Running partition: %s\n", running->label);

    /*
        //Inicjalizacja podłączenia do AP
        wifi_init_sta(); */

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

    /*
        //Start strony WWW do aktualizacji przez przeglądarkę
        start_webserver();

        //Obsługa  ESP-NOW do przesyłania iformacji o położeniu do panelu operatorskiego
        espnow_init(); */

    gpio_set_direction(STEP_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN,GPIO_PULLUP_ONLY);
    gpio_set_level(DIR_PIN, 1);

    gptimer_handle_t timer = NULL;

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    ESP_ERROR_CHECK(
        gptimer_new_timer(
            &timer_config,
            &timer));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = step_timer_callback,
    };

    ESP_ERROR_CHECK(
        gptimer_register_event_callbacks(
            timer,
            &callbacks,
            NULL));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 2500,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            timer,
            &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));

    gpio_set_level(EN_PIN, 0);

    while (1)
{
    bool state = gpio_get_level(BUTTON_PIN);

    if (last_state == 1 && state == 0)
    {
        enabled = !enabled;

        if (enabled)
        {
            gpio_set_level(EN_PIN, 0); // A4988 ON
            printf("Silnik ON\n");
        }
        else
        {
            gpio_set_level(EN_PIN, 1); // A4988 OFF
            printf("Silnik OFF\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_state = state;

    vTaskDelay(pdMS_TO_TICKS(10));
}
}
