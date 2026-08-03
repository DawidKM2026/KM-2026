#include "esp_now_comm.h"

#include <inttypes.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "gpio_config.h"
#include "stepper_motor.h"
#include "spm_motors.h"


// Koordynaty
int32_t target_x;
int32_t target_y;

int32_t current_x1 = 5;
int32_t current_x2 = 5;
int32_t current_x = 5;

int32_t current_y = 8;


//Fizyczne wymiary pola, na którym porusza się makieta statku
int max_x_limit=100; 
int min_x_limit=-100;
int max_y_limit=100;
int min_y_limit=-100;


//Komendy
typedef enum
{
    CMD_SET_FIELD_DIMENSIONS = 1,
    CMD_GET_POSITION = 2,
    CMD_SET_MOVE_TO = 3,
    CMD_SET_MOVE_BY = 4,
    CMD_POSITION_RESPONSE = 5,
    CMD_ACK_POSITION = 6,

    CMD_SET_SPM = 7,
    CMD_GET_SPM = 8,
    CMD_SPM_RESPONSE = 9,
    CMD_ACK_SPM = 10

} command_t;

//Struktura odpowiedzi
typedef struct
{
    uint32_t id;
    uint8_t cmd;

    int32_t x;
    int32_t y;

    float roll;
    float pitch;
    float yaw;

    float theta1_actual;
    float theta2_actual;
    float theta3_actual;

    float theta1_target;
    float theta2_target;
    float theta3_target;

} message_t;

float current_roll = 0.0f;
float current_pitch = 0.0f;
float current_yaw = 0.0f;

//------------------------------------ ESP-NOW Sender ------------------------------------------------
static const char *TAG_SENDER = "SENDER";


static uint8_t receiver_mac[] = {
    0x7C, 0xDF, 0xA1,
    0xE4, 0x00, 0x68};

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
        default:
            break;
        case CMD_SET_FIELD_DIMENSIONS:{
            max_x_limit = msg.x/2;
            min_x_limit = -msg.x/2;
            max_y_limit = msg.y/2;
            min_y_limit = -msg.y/2;
            break;
        }
        case CMD_GET_POSITION:{
            update_current_position();

            message_t response ={
                    .id = msg.id,
                    .cmd = CMD_POSITION_RESPONSE,
                    .x = current_x,
                    .y = current_y};

            printf("Pozycja do wysłania: X=%" PRId32 " Y=%" PRId32 "\n", current_x, current_y);
            esp_now_send(info->src_addr, (uint8_t *)&response, sizeof(response));
            break;
        }
        
        case CMD_SET_MOVE_TO:{
            
            target_x = msg.x;
            target_y = msg.y;
            if(msg.x < min_x_limit){
                msg.x = min_x_limit;
            }
            if(msg.x > max_x_limit){
                msg.x = max_x_limit;
            }
            if(msg.y < min_y_limit){
                msg.y = min_y_limit;
            }
            if(msg.y > max_y_limit){
                msg.y = max_y_limit;
            }

            ESP_LOGI(TAG_RECEIVER, "SET_POSITION id=%" PRIu32 " X=%" PRId32 " Y=%" PRId32, msg.id, msg.x, msg.y);

            message_t response ={
                    .id = msg.id,
                    .cmd = CMD_ACK_POSITION,
                };
            
            esp_now_send(info->src_addr, (uint8_t *)&response, sizeof(response));
            printf("Pozycja otrzymana do osiągnięcia: X=%" PRId32 " Y=%" PRId32 "\n", msg.x, msg.y);
            motor_send_command(MOVE_TO, msg.x, msg.y);
            break;
        }

        case CMD_SET_MOVE_BY:{
            
            ESP_LOGI(TAG_RECEIVER, "MOVE_BY id=%" PRIu32 " X=%" PRId32 " Y=%" PRId32, msg.id, msg.x, msg.y);

            message_t response ={
                    .id = msg.id,
                    .cmd = CMD_ACK_POSITION,
                };
            printf("Wychylenie otrzymane: X=%" PRId32 " Y=%" PRId32 "\n", msg.x, msg.y);
            esp_now_send(info->src_addr, (uint8_t *)&response, sizeof(response));
            motor_send_command(MOVE_BY, msg.x, msg.y);
            break;
        }

        case CMD_SET_SPM:{
            current_roll = msg.roll;
            current_pitch = msg.pitch;
            current_yaw = msg.yaw;

            ESP_LOGI(
                TAG_RECEIVER,
                "SET_SPM R=%.2f P=%.2f Y=%.2f",
                msg.roll,
                msg.pitch,
                msg.yaw);

            bool result =
                spm_motors_move_rpy(
                    msg.roll,
                    msg.pitch,
                    msg.yaw,
                    0.5f);

            message_t response =
            {
                .id = msg.id,
                .cmd = CMD_ACK_SPM
            };

            response.x = result ? 1 : 0;

            esp_now_send(
                info->src_addr,
                (uint8_t *)&response,
                sizeof(response));

            break;
        }

        case CMD_GET_SPM:{
            message_t response =
            {
                .id = msg.id,
                .cmd = CMD_SPM_RESPONSE,

                .roll = current_roll,
                .pitch = current_pitch,
                .yaw = current_yaw
            };

            spm_motors_get_actual_angles(
                &response.theta1_actual,
                &response.theta2_actual,
                &response.theta3_actual);

            spm_motors_get_target_angles(
                &response.theta1_target,
                &response.theta2_target,
                &response.theta3_target);

            esp_now_send(
                info->src_addr,
                (uint8_t *)&response,
                sizeof(response));

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