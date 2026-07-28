#include "uart_comm.h"

#include "commands.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_RESPONSE_TIMEOUT_MS 500

static uart_port_t uart_num = UART_NUM_1;

typedef enum
{
    UART_TX_IDLE = 0,
    UART_TX_WAIT_RESPONSE

} uart_tx_state_t;

static volatile uart_tx_state_t txState = UART_TX_IDLE;

static TickType_t txTimestamp = 0;

static uint8_t calc_crc(
    const UARTFrame *f)
{
    uint8_t crc = 0;

    crc ^= f->preamble;
    crc ^= f->boardID;

    crc ^= (f->command >> 8);
    crc ^= f->command;

    crc ^= f->motorID;

    crc ^= (f->data1 >> 8);
    crc ^= f->data1;

    crc ^= (f->data2 >> 8);
    crc ^= f->data2;

    crc ^= (f->data3 >> 8);
    crc ^= f->data3;

    crc ^= (f->data4 >> 8);
    crc ^= f->data4;

    return crc;
}

static void check_timeout(void)
{
    if (txState != UART_TX_WAIT_RESPONSE)
    {
        return;
    }

    TickType_t now = xTaskGetTickCount();

    if ((now - txTimestamp) >
        pdMS_TO_TICKS(UART_RESPONSE_TIMEOUT_MS))
    {
        txState = UART_TX_IDLE;
    }
}

bool uart_comm_ready(void)
{
    check_timeout();

    return (txState == UART_TX_IDLE);
}

void uart_comm_response_received(void)
{
    txState = UART_TX_IDLE;
}

bool uart_comm_init(
    int txPin,
    int rxPin,
    uint32_t baud)
{
    uart_config_t cfg =
    {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };

    uart_driver_install(
        uart_num,
        2048,
        2048,
        0,
        NULL,
        0);

    uart_param_config(
        uart_num,
        &cfg);

    uart_set_pin(
        uart_num,
        txPin,
        rxPin,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE);

    txState = UART_TX_IDLE;

    return true;
}

bool uart_send_frame(
    uint8_t boardID,
    uint16_t command,
    uint8_t motorID,
    int16_t data1,
    int16_t data2,
    int16_t data3,
    int16_t data4)
{
    check_timeout();

    if (txState == UART_TX_WAIT_RESPONSE)
    {
        return false;
    }

    UARTFrame frame;

    frame.preamble = PREAMBLE;
    frame.boardID  = boardID;
    frame.command  = command;
    frame.motorID  = motorID;

    frame.data1 = data1;
    frame.data2 = data2;
    frame.data3 = data3;
    frame.data4 = data4;

    frame.crc = calc_crc(&frame);

    int len = uart_write_bytes(
        uart_num,
        (const void *)&frame,
        sizeof(frame));

    if (len != sizeof(frame))
    {
        return false;
    }

    /*
     * Broadcast (ID = 0)
     * brak odpowiedzi
     * można wysyłać dalej
     */
    if (boardID == 0)
    {
        txState = UART_TX_IDLE;
    }
    /*
     * Unicast (ID > 0)
     * czekamy na odpowiedź
     */
    else
    {
        txState = UART_TX_WAIT_RESPONSE;
        txTimestamp = xTaskGetTickCount();
    }

    return true;
}

bool uart_receive_frame(UARTFrame *frame)
{
    int len = uart_read_bytes(
        uart_num,
        (uint8_t *)frame,
        sizeof(UARTFrame),
        pdMS_TO_TICKS(10));

    if (len != sizeof(UARTFrame))
    {
        return false;
    }

    if (frame->preamble != PREAMBLE)
    {
        return false;
    }

    if (calc_crc(frame) != frame->crc)
    {
        return false;
    }

    /*
     * Odpowiedź tylko dla unicast.
     * Broadcast nie powinien nic odsyłać.
     */
    if (frame->boardID != 0)
    {
        uart_comm_response_received();
    }

    return true;
}