#include "system_state.h"

#include "system_boot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stepper_motor.h"
#include "gpio_config.h"

static system_state_t current_state = SYSTEM_BOOT;

/* -------------------------------------------------------------------------- */
/* PRIVATE FUNCTIONS */
/* -------------------------------------------------------------------------- */

static void system_boot_handler(void);
static void system_homing_handler(void);
static void system_ready_handler(void);
static void system_running_handler(void);
static void system_error_handler(void);
static void system_estop_handler(void);

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

    /*
     * Tutaj później:
     * - komendy ESP-NOW
     * - komendy WWW
     * - start programu ruchu
     */

    /*
    if(start_motion_request())
    {
        system_set_state(SYSTEM_RUNNING);
    }
    */
}

/* -------------------------------------------------------------------------- */

static void system_running_handler(void)
{
    /*
     * Obsługa ruchu.
     */

    /*
    if(motion_finished())
    {
        system_set_state(SYSTEM_READY);
    }
    */

    /*
    if(motion_error())
    {
        system_set_state(SYSTEM_ERROR);
    }
    */
}

/* -------------------------------------------------------------------------- */

static void system_error_handler(void)
{
    /*
     * Obsługa błędu.
     */

    /*
    stepper_stop_all();
    */

    /*
    if(error_reset())
    {
        system_set_state(SYSTEM_READY);
    }
    */
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