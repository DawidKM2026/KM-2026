/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_partition.h"

//--------------------------------- Access Point ----------------------------------------

#define WIFI_SSID "KM Projekt v1.0.0"
#define WIFI_PASS "KM123456"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 4

static const char *TAG = "wifi_ap";

// Informacje o wifi do debug'owania
static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event =
            (wifi_event_ap_staconnected_t *)event_data;

        ESP_LOGI(TAG, "Device connected, AID=%d", event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event =
            (wifi_event_ap_stadisconnected_t *)event_data;

        ESP_LOGI(TAG, "Device disconnected, AID=%d", event->aid);
    }
}

// Konfiguracja Access Point'a
void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Obsługa zdarzeń związanych z WiFi
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    if (strlen(WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    ESP_LOGI(TAG, "WiFi mode=%d", mode);

    ESP_LOGI(TAG, "Access Point started");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_PASS);
    ESP_LOGI(TAG, "Channel: %d", WIFI_CHANNEL);
}

//------------------------------------ Koniec Access Point ---------------------------------------------------

static esp_err_t ota_post_handler(httpd_req_t *req);
//------------------------------------ Strona Internetowa ---------------------------------------------------

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const size_t html_size = index_html_end - index_html_start;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, html_size);

    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Zwracanie strony
    static const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL};

    // Odbiór pliku firmware.bin
    static const httpd_uri_t update_uri = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Starting HTTP server");

    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &update_uri));
        ESP_LOGI(TAG, "HTTP server started");
        return server;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

//------------------------------------ Koniec Strony Internetowej ---------------------------------------------------

//------------------------------------  Obsługa update'ów ze stronki ---------------------------------------------------

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    // Feedback
    esp_err_t ret;

    // Szukanie partycji do update'u
    const esp_partition_t *update_partition =
        esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL)
    {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_FAIL;
    }

    // Id aktualizacji
    esp_ota_handle_t ota_handle;

    // Nadpisanie nwoego firmware'a
    ret = esp_ota_begin(
        update_partition,
        OTA_SIZE_UNKNOWN,
        &ota_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA begin failed");
        return ESP_FAIL;
    }

    // Temp memory
    char buffer[1024];
    int remaining = req->content_len;

    // Paczkowe pobeiranie i zapisywanie danych
    while (remaining > 0)
    {

        // Pobieranie danych ze stronki po 1KB
        int received = httpd_req_recv(
            req,
            buffer,
            sizeof(buffer));

        if (received <= 0)
        {
            esp_ota_end(ota_handle);
            return ESP_FAIL;
        }

        // Zapis do flash
        ret = esp_ota_write(
            ota_handle,
            buffer,
            received);

        if (ret != ESP_OK)
        {
            esp_ota_end(ota_handle);
            return ESP_FAIL;
        }
        remaining -= received;
    }

    ret = esp_ota_end(ota_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA end failed");
        return ESP_FAIL;
    }

    // Ustawienie nowej default'owej partycji
    ret = esp_ota_set_boot_partition(
        update_partition);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Set boot partition failed");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(
        req,
        "OTA OK, restarting...");

    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();

    return ESP_OK;
}

//------------------------------------ Koniec Obsługi Update'ów ---------------------------------------------------

void app_main(void)
{

    //--------------------------------- Access Point ----------------------------------------

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init_softap();

    start_webserver();

    ESP_LOGI(TAG, "Open browser: http://192.168.4.1");

    //------------------------------------ Koniec Access Point ---------------------------------------------------

    //------------------------------------ Sprawdzenie aktualnej partycji ---------------------------------------------------

    const esp_partition_t *running = esp_ota_get_running_partition();
    printf("Running partition: %s\n", running->label);

    //------------------------------------ Koniec ---------------------------------------------------

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
