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

/* Aktualny stan */
void system_set_state(system_state_t state);
system_state_t system_get_state(void);

/* Task maszyny stanów */
void system_state_task(void *pvParameters);

#endif /* SYSTEM_STATE_H */