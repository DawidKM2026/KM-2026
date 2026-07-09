#include "esp_now_comm.h"

#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_now.h"
#include "esp_wifi.h"


typedef enum
{
    CMD_SET_POSITION = 1,
    CMD_GET_POSITION = 2,
    CMD_POSITION_RESPONSE = 3,
    CMD_ACK_POSITION = 4
} command_t;

typedef struct
{
    uint32_t id;
    uint8_t cmd;
    int32_t x;
    int32_t y;
} message_t;

//ID poleceń
static uint32_t msg_id = 0;


#define WIFI_CHANNEL 1

//------------------------------------ ESP-NOW Master ------------------------------------------------
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

// Wyślij pozycję zadaną
void send_position(int32_t x, int32_t y)
{
    message_t msg =
        {
            .id = ++msg_id,
            .cmd = CMD_SET_POSITION,
            .x = x,
            .y = y};

    esp_now_send(receiver_mac,
                 (uint8_t *)&msg,
                 sizeof(msg));

    ESP_LOGI("MASTER",
             "SET_POSITION id=%" PRIu32 " X=%" PRId32 " Y=%" PRId32,
             msg.id,
             x,
             y);
}

static const char *TAG_RECEIVER = "RECEIVER";

// Wyświetl aktualną pozcyję
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

    switch (msg.cmd)
    {
    case CMD_ACK_POSITION:
    {
        ESP_LOGI(TAG_RECEIVER,
                 "ACK_POSITION id=%" PRIu32,
                 msg.id);
        break;
    }

    case CMD_POSITION_RESPONSE:
    {
        ESP_LOGI(TAG_RECEIVER,
                 "POSITION_RESPONSE id=%" PRIu32
                 " X=%ld Y=%ld",
                 msg.id,
                 msg.x,
                 msg.y);

        break;
    }

    default:
        ESP_LOGW(TAG_RECEIVER,
                 "Unknown cmd=%u",
                 msg.cmd);
        break;
    }
}

// Zarządaj aktualnej pozcyji
void get_position()
{
    message_t msg =
        {
            .id = ++msg_id,
            .cmd = CMD_GET_POSITION};

    esp_now_send(receiver_mac,
                 (uint8_t *)&msg,
                 sizeof(msg));

    ESP_LOGI("MASTER",
             "GET_POSITION id=%" PRIu32,
             msg.id);
}

void espnow_init(){

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
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    ESP_ERROR_CHECK(
        esp_now_add_peer(&peer));
    ESP_LOGI(TAG_SENDER,
             "Peer added");

}
//------------------------------------ Koniec ESP-NOW Master ------------------------------------------------
