#pragma once

#include <stdint.h>

#define PREAMBLE 0xAA

#define CMD_PING            0x0001
#define CMD_STATUS          0x0002

#define CMD_LIVE_MPU        0x0004
#define CMD_HIST_MPU        0x0008

#define CMD_LIVE_GYRO       0x0010
#define CMD_HIST_GYRO       0x0020

#define CMD_PUSH_INFO       0x0040
#define CMD_RESET_STAT      0x0080

#define CMD_HOMING          0x0100
#define CMD_MOVE_ABS        0x0200
#define CMD_MOVE_REL        0x0400
#define CMD_STOP_MOTOR      0x0800

#define CMD_SET_SPEED       0x1000
#define CMD_SET_ACCEL       0x2000
#define CMD_MOVE_VELOCITY   0x4000

#pragma pack(push,1)

typedef struct
{
    uint8_t  preamble;
    uint8_t  boardID;
    uint16_t command;
    uint8_t  motorID;

    int16_t data1;
    int16_t data2;
    int16_t data3;
    int16_t data4;

    uint8_t crc;

} UARTFrame;

#pragma pack(pop)