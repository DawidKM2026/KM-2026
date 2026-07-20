#include "esp_now_comm.h"

#include <string.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "gpio_config.h"

#include "stepper_motor.h"



// Koordynaty
int32_t target_x;
int32_t target_y;

int32_t current_x = 5;
int32_t current_y = 8;


//Fizyczne wymiary pola, na którym porusza się makieta statku
int max_x_limit=100; 
int min_x_limit=100;
int max_y_limit=100;
int min_y_limit=100;


//Komendy
typedef enum
{
    CMD_SET_FIELD_DIMENSIONS = 1,
    CMD_GET_POSITION = 2,
    CMD_SET_MOVE_TO=3,
    CMD_SET_MOVE_BY=4,
    CMD_POSITION_RESPONSE = 5,
    CMD_ACK_POSITION = 6
} command_t;

//Struktura odpowiedzi
typedef struct
{
    uint32_t id;
    uint8_t cmd;
    int32_t x;
    int32_t y;
} message_t;



//------------------------------------ ESP-NOW Sender ------------------------------------------------
static const char *TAG_SENDER = "SENDER";


static uint8_t receiver_mac[] = {
    0x24, 0x58, 0x7C,
    0xE1, 0xF7, 0xB8};

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status){
    if (tx_info == NULL){
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

    switch (msg.cmd){
    case CMD_SET_POSITION:{
        target_x = msg.x;
        target_y = msg.y;

        ESP_LOGI(TAG_RECEIVER, "SET_POSITION id=%ld X=%ld Y=%ld", msg.id, msg.x, msg.y);

        message_t response ={
                .id = msg.id,
                .cmd = CMD_ACK_POSITION,
            };

        esp_now_send(info->src_addr, (uint8_t *)&response, sizeof(response));
        break;
    }
    case CMD_GET_POSITION:
    {
        message_t response =
            {
                .id = msg.id,
                .cmd = CMD_POSITION_RESPONSE,
                .x = current_x,
                .y = current_y};

        esp_now_send(info->src_addr,
                     (uint8_t *)&response,
                     sizeof(response));

        break;
    }
    
    case CMD_SET_MOVE_TO:{
        target_x = msg.x;
        target_y = msg.y;

        ESP_LOGI(TAG_RECEIVER, "SET_POSITION id=%ld X=%ld Y=%ld", msg.id, msg.x, msg.y);

        message_t response ={
                .id = msg.id,
                .cmd = CMD_ACK_POSITION,
            };
        
        esp_now_send(info->src_addr, (uint8_t *)&response, sizeof(response));
        motor_send_command(MOVE_TO, msg.x, msg.y);
        break;
    }

    case CMD_SET_MOVE_BY:{
        ESP_LOGI(TAG_RECEIVER, "MOVE_BY id=%ld X=%ld Y=%ld", msg.id, msg.x, msg.y);

        message_t response ={
                .id = msg.id,
                .cmd = CMD_ACK_POSITION,
            };
        esp_now_send(info->src_addr, (uint8_t *)&response, sizeof(response));

        motor_send_command(MOVE_BY, msg.x, msg.y);
        break;
    }
    }
}


static const char *TAG_ESPNOW = "ESPNOW";
void espnow_init(void)
{
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
}

//------------------------------------ Koniec ESP-NOW Receiver ------------------------------------------------
