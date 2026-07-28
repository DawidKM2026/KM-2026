#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdint.h>
#include <stdbool.h>

bool uart_comm_init(
    int txPin,
    int rxPin,
    uint32_t baud);

bool uart_send_frame(
    uint8_t boardID,
    uint16_t command,
    uint8_t motorID,
    int16_t data1,
    int16_t data2,
    int16_t data3,
    int16_t data4);

void uart_comm_response_received(void);

bool uart_comm_ready(void);

bool uart_receive_frame(UARTFrame *frame);

#endif