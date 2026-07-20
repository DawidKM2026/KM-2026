#include "over_the_air_updates.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG_WEB = "WEB";

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

httpd_handle_t start_webserver(void)
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
