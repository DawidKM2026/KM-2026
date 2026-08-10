#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "nvs_flash.h"
#include "esp_err.h"

#include "config_access_point.h"
#include "esp_now_comm.h"

#include <stdint.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"

#define TAG_WIFI "WIFI"

static uint8_t receiver_mac[] = {
    0xD0, 0xCF, 0x13,
    0x41, 0x10, 0xDC};
    
typedef enum
{
    CMD_SET_FIELD_DIMENSIONS = 1,
    CMD_GET_POSITION = 2,
    CMD_SET_MOVE_TO = 3,
    CMD_SET_MOVE_BY = 4,
    CMD_POSITION_RESPONSE = 5,
    CMD_ACK_POSITION = 6
} command_t;

typedef struct
{
    uint32_t id;
    command_t cmd;
    int32_t x;
    int32_t y;
} message_t;

static void serial_task(void *arg)
{
    char buffer[128];
    int pos = 0;
    int counter = 0;

    while (1)
    {
        int c = getchar();

        if (c != EOF)
        {
            if (c == '\r')
            {
                // ignorujemy CR
            }
            else if (c == '\n')
            {
                buffer[pos] = '\0';

                message_t msg = {0};
                msg.id = ++counter;

                if (strcmp(buffer, "GET_POSITION") == 0)
                {
                    msg.cmd = CMD_GET_POSITION;
                }
                else if (sscanf(buffer, "MOVE_TO %ld %ld",
                                &msg.x,
                                &msg.y) == 2)
                {
                    msg.cmd = CMD_SET_MOVE_TO;
                }
                else if (sscanf(buffer, "MOVE_BY %ld %ld",
                                &msg.x,
                                &msg.y) == 2)
                {
                    msg.cmd = CMD_SET_MOVE_BY;
                }
                else if (sscanf(buffer, "FIELD %ld %ld",
                                &msg.x,
                                &msg.y) == 2)
                {
                    msg.cmd = CMD_SET_FIELD_DIMENSIONS;
                }
                else
                {
                    printf("Unknown command: %s\r\n", buffer);
                    pos = 0;
                    continue;
                }

                esp_err_t err = esp_now_send(
                    receiver_mac,
                    (uint8_t *)&msg,
                    sizeof(msg));

                if (err == ESP_OK)
                {
                    printf("SENT: id=%lu cmd=%d x=%ld y=%ld\r\n",
                           msg.id,
                           msg.cmd,
                           msg.x,
                           msg.y);
                }
                else
                {
                    printf("esp_now_send failed: %s\r\n",
                           esp_err_to_name(err));
                }

                fflush(stdout);
                pos = 0;
            }
            else if (pos < sizeof(buffer) - 1)
            {
                buffer[pos++] = (char)c;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{

    // Inicjalizacja pamięci
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Sprawdzenie swojego adresu MAC do komunikacji ESP-NOW
    wifi_init_softap();
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG_WIFI,
             "THIS ESP MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG_WIFI,
             "RECEIVER MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             receiver_mac[0], receiver_mac[1], receiver_mac[2],
             receiver_mac[3], receiver_mac[4], receiver_mac[5]);

    // Komunikacja ESP-NOW

    espnow_init();

    xTaskCreate(
        serial_task,
        "serial_task",
        4096,
        NULL,
        5,
        NULL);

}
