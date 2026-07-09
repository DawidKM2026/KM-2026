#ifndef ESP_NOW_COMM_H
#define ESP_NOW_COMM_H

#include "esp_now_comm.h"
#include <stdint.h>

void get_position();
void send_position(int32_t x, int32_t y);
void espnow_init();

#endif