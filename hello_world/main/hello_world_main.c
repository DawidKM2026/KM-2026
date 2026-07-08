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

//--------------------------------- Access Point ----------------------------------------
#define WIFI_SSID_NAME "KM Projekt"
#define FIRMWARE_VERSION "1.0.0"
#define WIFI_PASS "KM123456"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 4

static const char *TAG_WIFI = "wifi_ap";

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

        ESP_LOGI(TAG_WIFI, "Device connected, AID=%d", event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event =
            (wifi_event_ap_stadisconnected_t *)event_data;

        ESP_LOGI(TAG_WIFI, "Device disconnected, AID=%d", event->aid);
    }
}

// Konfiguracja Access Point'a
void wifi_init_softap(void)
{
    char WIFI_SSID[32];

    snprintf(
        WIFI_SSID,
        sizeof(WIFI_SSID),
        "%s v%s",
        WIFI_SSID_NAME,
        FIRMWARE_VERSION);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

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
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    strcpy(
        (char *)wifi_config.ap.ssid,
        WIFI_SSID);

    wifi_config.ap.ssid_len = strlen(WIFI_SSID);

    if (strlen(WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    ESP_LOGI(TAG_WIFI, "WiFi mode=%d", mode);

    ESP_LOGI(TAG_WIFI, "Access Point started");
    ESP_LOGI(TAG_WIFI, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG_WIFI, "Password: %s", WIFI_PASS);
    ESP_LOGI(TAG_WIFI, "Channel: %d", WIFI_CHANNEL);
}

//------------------------------------ Koniec Access Point ---------------------------------------------------

//------------------------------------- ESP-NOW Global Config ----------------------------------------------
typedef struct
{
    uint32_t counter;
} message_t;

//------------------------------------ ESP-NOW Sender ------------------------------------------------
static const char *TAG_SENDER = "SENDER";

static uint8_t receiver_mac[] = {
    0xD0, 0xCF, 0x13,
    0x41, 0x10, 0xDC};

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
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));

    ESP_LOGI(TAG_WIFI,
             "STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG_WIFI, "Open browser: http://192.168.4.2");

    //------------------------------------ Koniec Access Point ---------------------------------------------------

    //------------------------------------ ESP-NOW Receiver --------------------------------------------------
    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(
        esp_now_register_recv_cb(recv_cb));

    ESP_LOGI(TAG_RECEIVER, "Waiting for packets...");

    //------------------------------------ ESP-NOW Receiver --------------------------------------------------

    //------------------------------------ ESP-NOW Sender --------------------------------------------------

    ESP_ERROR_CHECK(
        esp_now_register_send_cb(send_cb));

    esp_now_peer_info_t peer = {0};

    memcpy(peer.peer_addr,
           receiver_mac,
           ESP_NOW_ETH_ALEN);

    peer.channel = WIFI_CHANNEL;
    peer.ifidx=WIFI_IF_STA;
    peer.encrypt = false;

    ESP_ERROR_CHECK(
        esp_now_add_peer(&peer));

    uint32_t counter = 0;

    while (1)
    {
        message_t msg = {
            .counter = counter++};

        
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

        //----------------------------------- Koniec ESP-NOW Sender ------------------------------------

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
