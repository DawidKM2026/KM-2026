#include "thrusters_control.h"

#include "uart_comm.h"
#include "commands.h"

bool thruster_ping(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_PING,
        0,
        0, 0, 0, 0);
}

bool thruster_status(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_STATUS,
        0,
        0, 0, 0, 0);
}

bool thruster_live_mpu(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_LIVE_MPU,
        0,
        0, 0, 0, 0);
}

bool thruster_hist_mpu(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_HIST_MPU,
        0,
        0, 0, 0, 0);
}

bool thruster_live_gyro(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_LIVE_GYRO,
        0,
        0, 0, 0, 0);
}

bool thruster_hist_gyro(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_HIST_GYRO,
        0,
        0, 0, 0, 0);
}

bool thruster_push_info(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_PUSH_INFO,
        0,
        0, 0, 0, 0);
}

bool thruster_reset_stat(uint8_t id)
{
    return uart_send_frame(
        id,
        CMD_RESET_STAT,
        0,
        0, 0, 0, 0);
}

bool thruster_home(
    uint8_t id,
    uint8_t motorID)
{
    return uart_send_frame(
        id,
        CMD_HOMING,
        motorID,
        0, 0, 0, 0);
}

bool thruster_move_abs(
    uint8_t id,
    uint8_t motorID,
    int16_t position)
{
    return uart_send_frame(
        id,
        CMD_MOVE_ABS,
        motorID,
        position,
        0,
        0,
        0);
}

bool thruster_move_rel(
    uint8_t id,
    uint8_t motorID,
    int16_t steps)
{
    return uart_send_frame(
        id,
        CMD_MOVE_REL,
        motorID,
        steps,
        0,
        0,
        0);
}

bool thruster_stop(
    uint8_t id,
    uint8_t motorID)
{
    return uart_send_frame(
        id,
        CMD_STOP_MOTOR,
        motorID,
        0,
        0,
        0,
        0);
}

bool thruster_set_speed(
    uint8_t id,
    uint8_t motorID,
    int16_t speed)
{
    return uart_send_frame(
        id,
        CMD_SET_SPEED,
        motorID,
        speed,
        0,
        0,
        0);
}

bool thruster_set_accel(
    uint8_t id,
    uint8_t motorID,
    int16_t accel)
{
    return uart_send_frame(
        id,
        CMD_SET_ACCEL,
        motorID,
        accel,
        0,
        0,
        0);
}

bool thruster_move_velocity(
    uint8_t id,
    uint8_t motorID,
    int16_t velocity)
{
    return uart_send_frame(
        id,
        CMD_MOVE_VELOCITY,
        motorID,
        velocity,
        0,
        0,
        0);
}