#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdint.h>
#include <stdbool.h>

#include "commands.h"

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

typedef struct
{
    int16_t pitchCurrent;
    int16_t rollCurrent;

    int16_t pitchPeak;
    int16_t rollPeak;
    int16_t maxDynamicG;
    int16_t pushCount;

    int16_t gxCurrent;
    int16_t gyCurrent;
    int16_t gzCurrent;

    int16_t gxPeak;
    int16_t gyPeak;
    int16_t gzPeak;

} TelemetryData;

const TelemetryData *uart_get_telemetry(void);

#endif