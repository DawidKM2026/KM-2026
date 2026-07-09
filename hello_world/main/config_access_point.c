#include "config_access_point.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"


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
