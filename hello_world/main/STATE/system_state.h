#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stepper_motor.h"

typedef enum
{
    SYSTEM_BOOT = 0,
    SYSTEM_HOMING,
    SYSTEM_READY,
    SYSTEM_RUNNING,
    SYSTEM_ERROR,
    SYSTEM_EMERGENCY_STOP

} system_state_t;

typedef enum
{
    ACTION_NONE,

    ACTION_MOVE_TO,
    ACTION_MOVE_BY,

    ACTION_SPM

} system_action_t;

typedef struct
{
    system_action_t action;

    int32_t x;
    int32_t y;

    float roll;
    float pitch;
    float yaw;

} system_command_t;

/* Aktualny stan */
void system_set_state(system_state_t state);
system_state_t system_get_state(void);

/* Task maszyny stanów */
void system_state_task(void *pvParameters);

/* COMMAND HANDLING */
bool system_set_command(const system_command_t *cmd);

#endif /* SYSTEM_STATE_H */