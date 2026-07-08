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
#include "esp_now.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"



static const char *TAG_WEB = "WEB";
static const char *TAG_WIFI = "WIFI_STA";
static const char *TAG_ESPNOW = "ESPNOW";
#define WIFI_SSID "KM Projekt v1.0.0"
#define WIFI_PASS "KM123456"
#define WIFI_CHANNEL 1

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static esp_netif_t *sta_netif = NULL;



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

    ESP_LOGI(TAG_WEB, "Starting HTTP server");

    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &update_uri));
        ESP_LOGI(TAG_WEB, "HTTP server started");
        return server;
    }

    ESP_LOGE(TAG_WEB, "Failed to start HTTP server");
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
        ESP_LOGE(TAG_WEB, "No OTA partition found");
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
        ESP_LOGE(TAG_WEB, "OTA begin failed");
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
        ESP_LOGE(TAG_WEB, "OTA end failed");
        return ESP_FAIL;
    }

    // Ustawienie nowej default'owej partycji
    ret = esp_ota_set_boot_partition(
        update_partition);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_WEB, "Set boot partition failed");
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


//------------------------------------- ESP-NOW Global Config ----------------------------------------------


//------------------------------------- Łączenie do AP -----------------------------------------
static void wifi_sta_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG_WIFI, "Disconnected from AP, reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG_WIFI,
                 "Got IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_sta_event_handler,
            NULL,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_sta_event_handler,
            NULL,
            NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Connecting to AP: %s", WIFI_SSID);

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY);

    ESP_LOGI(TAG_WIFI, "WiFi connected");
}
//----------------------------------------------------------- Koniec łączenia do AP
typedef struct
{
    uint32_t counter;
} message_t;


//------------------------------------ ESP-NOW Sender ------------------------------------------------
static const char *TAG_SENDER = "SENDER";

static uint8_t receiver_mac[] = {
    0x24, 0x58, 0x7C,
    0xE1, 0xF7, 0xB8};

static void send_cb(
    const esp_now_send_info_t *tx_info,
    esp_now_send_status_t status)
{
    if (tx_info == NULL)
    {
        ESP_LOGE(TAG_SENDER, "Send callback error: tx_info is NULL");
        return;
    }

    ESP_LOGI(TAG_SENDER,
             "Send to %02X:%02X:%02X:%02X:%02X:%02X: %s",
             tx_info->des_addr[0],
             tx_info->des_addr[1],
             tx_info->des_addr[2],
             tx_info->des_addr[3],
             tx_info->des_addr[4],
             tx_info->des_addr[5],
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}
//------------------------------------ Koniec ESP-NOW Sender ------------------------------------------------

//------------------------------------ ESP-NOW Receiver ------------------------------------------------

static const char *TAG_RECEIVER = "RECEIVER";

static void recv_cb(
    const esp_now_recv_info_t *info,
    const uint8_t *data,
    int len)
{
    if (len != sizeof(message_t))
    {
        ESP_LOGW(TAG_RECEIVER, "Unexpected packet size");
        return;
    }

    message_t msg;
    memcpy(&msg, data, sizeof(msg));

    ESP_LOGI(TAG_RECEIVER, "Counter: %lu", msg.counter);
}

//------------------------------------ Koniec ESP-NOW Receiver ------------------------------------------------

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    //Sprawdzanie aktywnej partycji
    const esp_partition_t *running = esp_ota_get_running_partition();
    printf("Running partition: %s\n", running->label);

    wifi_init_sta();

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));

    
    ESP_LOGI(TAG_WIFI,
             "STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);

    start_webserver();

    ESP_LOGI(TAG_WEB, "Open browser: http://192.168.4.2");

    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));

    esp_now_peer_info_t peer = {0};

    memcpy(peer.peer_addr,
           receiver_mac,
           ESP_NOW_ETH_ALEN);

    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG_ESPNOW, "ESP-NOW ready");

    uint32_t counter = 0;

    while (1)
    {
        message_t msg = {
            .counter = counter++
        };

        esp_err_t send_result = esp_now_send(
            receiver_mac,
            (uint8_t *)&msg,
            sizeof(msg));

        if (send_result != ESP_OK)
        {
            ESP_LOGE(TAG_SENDER,
                     "esp_now_send failed: %s",
                     esp_err_to_name(send_result));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
