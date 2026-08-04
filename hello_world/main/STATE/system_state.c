#include "system_state.h"

#include "system_boot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stepper_motor.h"
#include "gpio_config.h"
#include "spm_motors.h"

static system_state_t current_state = SYSTEM_BOOT;
static volatile bool command_pending =
    false;

static system_command_t pending_command;

/* -------------------------------------------------------------------------- */
/* PRIVATE FUNCTIONS */
/* -------------------------------------------------------------------------- */

static void system_boot_handler(void);
static void system_homing_handler(void);
static void system_ready_handler(void);
static void system_running_handler(void);
static void system_error_handler(void);
static void system_estop_handler(void);
static bool error_reset(void);

/* -------------------------------------------------------------------------- */
/* COMMAND HANDLING */
/* -------------------------------------------------------------------------- */

bool system_set_command(
    const system_command_t *cmd)
{
    if(current_state != SYSTEM_READY)
    {
        return false;
    }

    pending_command = *cmd;

    command_pending = true;

    return true;
}

/* -------------------------------------------------------------------------- */
/* STATE ACCESS */
/* -------------------------------------------------------------------------- */

void system_set_state(system_state_t state)
{
    current_state = state;
}

/* -------------------------------------------------------------------------- */

system_state_t system_get_state(void)
{
    return current_state;
}

/* -------------------------------------------------------------------------- */
/* STATE HANDLERS */
/* -------------------------------------------------------------------------- */

static void system_boot_handler(void)
{
    system_boot();

    system_set_state(SYSTEM_HOMING);
}

/* -------------------------------------------------------------------------- */

static void system_homing_handler(void)
{
    bool homing_done = motor_homing();

    if (homing_done)
    {
        system_set_state(SYSTEM_READY);
    }
    else
    {
        system_set_state(SYSTEM_ERROR);
    }
}

/* -------------------------------------------------------------------------- */

static void system_ready_handler(void)
{
    motor_button_on_off();

    if(command_pending)
    {
        system_set_state(
            SYSTEM_RUNNING);
    }
}

/* -------------------------------------------------------------------------- */

static void system_running_handler(void)
{
    bool success = false;

    switch(pending_command.action)
    {
        case ACTION_MOVE_TO:

            success =
                (motor_move_to(
                    pending_command.x,
                    pending_command.y)
                == ESP_OK);

            break;

        case ACTION_MOVE_BY:

            success =
                (motor_move_by(
                    pending_command.x,
                    pending_command.y)
                == ESP_OK);

            break;

        case ACTION_SPM:

            success =
                spm_motors_move_rpy(
                    pending_command.roll,
                    pending_command.pitch,
                    pending_command.yaw,
                    0.5f);

            break;

        default:

            success = false;

            break;
    }
    
    command_pending = false;

    pending_command.action =
        ACTION_NONE;

    if(success)
    {
        system_set_state(
            SYSTEM_READY);
    }
    else
    {
        system_set_state(
            SYSTEM_ERROR);
    }
}

/* -------------------------------------------------------------------------- */

static void system_error_handler(void)
{
    if(error_reset())
    {
        system_set_state(
            SYSTEM_READY);
    }
}

/* -------------------------------------------------------------------------- */

static void system_estop_handler(void)
{
    motor_emergency_stop();

    if (!is_emergency_stop_pressed())
    {
        system_set_state(SYSTEM_READY);
    }
}

/* -------------------------------------------------------------------------- */
/* MAIN STATE MACHINE TASK */
/* -------------------------------------------------------------------------- */

void system_state_task(void *pvParameters)
{
    while (1)
    {
        /*
         * E-STOP ma najwyższy priorytet.
         */

        if (is_emergency_stop_pressed())
        {
            system_set_state(SYSTEM_EMERGENCY_STOP);
        }

        switch (system_get_state())
        {
            case SYSTEM_BOOT:
                system_boot_handler();
                break;

            case SYSTEM_HOMING:
                system_homing_handler();
                break;

            case SYSTEM_READY:
                system_ready_handler();
                break;

            case SYSTEM_RUNNING:
                system_running_handler();
                break;

            case SYSTEM_ERROR:
                system_error_handler();
                break;

            case SYSTEM_EMERGENCY_STOP:
                system_estop_handler();
                break;

            default:
                system_set_state(SYSTEM_ERROR);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static bool error_reset(void)
{
    return true;
}
