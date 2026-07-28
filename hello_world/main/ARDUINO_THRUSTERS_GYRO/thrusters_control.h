#ifndef THRUSTERS_CONTROL_H
#define THRUSTERS_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

bool thruster_ping(uint8_t id);
bool thruster_status(uint8_t id);

bool thruster_live_mpu(uint8_t id);
bool thruster_hist_mpu(uint8_t id);

bool thruster_live_gyro(uint8_t id);
bool thruster_hist_gyro(uint8_t id);

bool thruster_push_info(uint8_t id);
bool thruster_reset_stat(uint8_t id);

bool thruster_home(
    uint8_t id,
    uint8_t motorID);

bool thruster_move_abs(
    uint8_t id,
    uint8_t motorID,
    int16_t position);

bool thruster_move_rel(
    uint8_t id,
    uint8_t motorID,
    int16_t steps);

bool thruster_stop(
    uint8_t id,
    uint8_t motorID);

bool thruster_set_speed(
    uint8_t id,
    uint8_t motorID,
    int16_t speed);

bool thruster_set_accel(
    uint8_t id,
    uint8_t motorID,
    int16_t accel);

bool thruster_move_velocity(
    uint8_t id,
    uint8_t motorID,
    int16_t velocity);

#endif