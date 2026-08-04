#include "spm_motors.h"

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "esp_rom_sys.h"

#include "gpio_config.h"

#include "spm_ik.h"

#define SPM_STEP_PULSE_US          5U
#define MOTOR_FULL_STEPS_PER_REV   200U
#define MOTOR_MICROSTEPS           16U
#define STEP_PULSE_US              5U

static QueueHandle_t spm_motors_queue = NULL;
static TaskHandle_t spm_motors_task_handle = NULL;

/* Faktycznie wykonana pozycja */
static int32_t motor1_position_steps = 0;
static int32_t motor2_position_steps = 0;
static int32_t motor3_position_steps = 0;

/* Ostatnia zadana pozycja */
static int32_t motor1_target_steps = 0;
static int32_t motor2_target_steps = 0;
static int32_t motor3_target_steps = 0;

/* -------------------------------------------------------------------------- */

static int32_t degrees_to_steps(float degrees)
{
    float steps_per_rev =
        MOTOR_FULL_STEPS_PER_REV *
        MOTOR_MICROSTEPS;

    return (int32_t)(
        degrees *
        steps_per_rev /
        360.0f);
}

static uint32_t calculate_step_period_us(float speed_rps)
{
    if (speed_rps <= 0.0f)
    {
        speed_rps = 0.1f;
    }

    float microsteps_per_second =
        speed_rps *
        MOTOR_FULL_STEPS_PER_REV *
        MOTOR_MICROSTEPS;

    uint32_t period_us =
        (uint32_t)(
            1000000.0f /
            microsteps_per_second);

    if (period_us <= STEP_PULSE_US)
    {
        period_us = STEP_PULSE_US + 1;
    }

    return period_us;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
    int32_t motor1_steps;
    int32_t motor2_steps;
    int32_t motor3_steps;

    float speed_rps;

} spm_motors_command_t;

/* -------------------------------------------------------------------------- */

static void spm_enable_all(void)
{
    gpio_set_level(SPM_EN_PIN, 1);
}

static void spm_disable_all(void)
{
    gpio_set_level(SPM_EN_PIN, 0);
}

static void generate_step(gpio_num_t step_pin)
{
    gpio_set_level(step_pin, 1);

    esp_rom_delay_us(
        SPM_STEP_PULSE_US);

    gpio_set_level(step_pin, 0);
}

/* -------------------------------------------------------------------------- */

static int32_t get_max3(
    int32_t a,
    int32_t b,
    int32_t c)
{
    int32_t max = a;

    if (b > max)
    {
        max = b;
    }

    if (c > max)
    {
        max = c;
    }

    return max;
}

/* -------------------------------------------------------------------------- */

static void execute_move(
    int32_t motor1_steps,
    int32_t motor2_steps,
    int32_t motor3_steps,
    float speed_rps)
{
    uint32_t step_period_us =
        calculate_step_period_us(
            speed_rps);

    gpio_set_level(
        SPM_DIR_1_PIN,
        motor1_steps >= 0);

    gpio_set_level(
        SPM_DIR_2_PIN,
        motor2_steps >= 0);

    gpio_set_level(
        SPM_DIR_3_PIN,
        motor3_steps >= 0);

    int32_t steps1 = abs(motor1_steps);
    int32_t steps2 = abs(motor2_steps);
    int32_t steps3 = abs(motor3_steps);

    int32_t max_steps =
        get_max3(
            steps1,
            steps2,
            steps3);

    if (max_steps == 0)
    {
        return;
    }

    spm_enable_all();

    esp_rom_delay_us(10);

    int32_t accumulator1 = 0;
    int32_t accumulator2 = 0;
    int32_t accumulator3 = 0;

    for (int32_t i = 0; i < max_steps; i++)
    {
        accumulator1 += steps1;
        accumulator2 += steps2;
        accumulator3 += steps3;

        if (accumulator1 >= max_steps)
        {
            generate_step(
                SPM_STEP_1_PIN);

            accumulator1 -= max_steps;
        }

        if (accumulator2 >= max_steps)
        {
            generate_step(
                SPM_STEP_2_PIN);

            accumulator2 -= max_steps;
        }

        if (accumulator3 >= max_steps)
        {
            generate_step(
                SPM_STEP_3_PIN);

            accumulator3 -= max_steps;
        }

        esp_rom_delay_us(
            step_period_us -
            STEP_PULSE_US);
    }

    gpio_set_level(SPM_STEP_1_PIN, 0);
    gpio_set_level(SPM_STEP_2_PIN, 0);
    gpio_set_level(SPM_STEP_3_PIN, 0);

    /* Aktualizacja pozycji */
    motor1_position_steps += motor1_steps;
    motor2_position_steps += motor2_steps;
    motor3_position_steps += motor3_steps;

    spm_disable_all();
}

/* -------------------------------------------------------------------------- */

static void spm_motors_task(void *parameters)
{
    spm_motors_command_t command;

    while (true)
    {
        if (xQueueReceive(
                spm_motors_queue,
                &command,
                portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        execute_move(
            command.motor1_steps,
            command.motor2_steps,
            command.motor3_steps,
            command.speed_rps);
    }
}

/* -------------------------------------------------------------------------- */

void spm_motors_init(void)
{
    gpio_set_level(SPM_STEP_1_PIN, 0);
    gpio_set_level(SPM_STEP_2_PIN, 0);
    gpio_set_level(SPM_STEP_3_PIN, 0);

    spm_disable_all();

    spm_motors_queue =
        xQueueCreate(
            10,
            sizeof(spm_motors_command_t));

    if (spm_motors_queue == NULL)
    {
        printf("SPM: blad tworzenia kolejki\n");
        return;
    }

    if (xTaskCreate(
            spm_motors_task,
            "spm_motors_task",
            4096,
            NULL,
            5,
            &spm_motors_task_handle)
        != pdPASS)
    {
        printf("SPM: blad tworzenia taska\n");
    }
}

/* -------------------------------------------------------------------------- */

bool spm_motors_move_steps(
    int32_t motor1_steps,
    int32_t motor2_steps,
    int32_t motor3_steps,
    float speed_rps)
{
    if (spm_motors_queue == NULL)
    {
        return false;
    }

    spm_motors_command_t command =
    {
        .motor1_steps = motor1_steps,
        .motor2_steps = motor2_steps,
        .motor3_steps = motor3_steps,
        .speed_rps = speed_rps
    };

    return xQueueSend(
        spm_motors_queue,
        &command,
        0) == pdTRUE;
}

/* -------------------------------------------------------------------------- */

bool spm_motors_move_rpy(
    float roll_deg,
    float pitch_deg,
    float yaw_deg,
    float speed_rps)
{
    spm_angles_t angles;

    if (!spm_calculate_ik(
            roll_deg,
            pitch_deg,
            yaw_deg,
            &angles))
    {
        return false;
    }

    int32_t new_target1 =
        degrees_to_steps(
            angles.theta1_deg);

    int32_t new_target2 =
        degrees_to_steps(
            angles.theta2_deg);

    int32_t new_target3 =
        degrees_to_steps(
            angles.theta3_deg);

    int32_t move1 =
        new_target1 -
        motor1_target_steps;

    int32_t move2 =
        new_target2 -
        motor2_target_steps;

    int32_t move3 =
        new_target3 -
        motor3_target_steps;

    motor1_target_steps =
        new_target1;

    motor2_target_steps =
        new_target2;

    motor3_target_steps =
        new_target3;

    return spm_motors_move_steps(
        move1,
        move2,
        move3,
        speed_rps);
}

/* -------------------------------------------------------------------------- */

void spm_motors_stop(void)
{
    if (spm_motors_queue != NULL)
    {
        xQueueReset(
            spm_motors_queue);
    }

    gpio_set_level(
        SPM_STEP_1_PIN,
        0);

    gpio_set_level(
        SPM_STEP_2_PIN,
        0);

    gpio_set_level(
        SPM_STEP_3_PIN,
        0);

    spm_disable_all();
}

/* -------------------------------------------------------------------------- */

void spm_motors_set_home(void)
{
    motor1_position_steps = 0;
    motor2_position_steps = 0;
    motor3_position_steps = 0;

    motor1_target_steps = 0;
    motor2_target_steps = 0;
    motor3_target_steps = 0;
}

/* -------------------------------------------------------------------------- */

void spm_motors_get_positions(
    int32_t *motor1_steps,
    int32_t *motor2_steps,
    int32_t *motor3_steps)
{
    if (motor1_steps)
    {
        *motor1_steps =
            motor1_position_steps;
    }

    if (motor2_steps)
    {
        *motor2_steps =
            motor2_position_steps;
    }

    if (motor3_steps)
    {
        *motor3_steps =
            motor3_position_steps;
    }
}

/* -------------------------------------------------------------------------- */

void spm_motors_get_actual_angles(
    float *theta1_deg,
    float *theta2_deg,
    float *theta3_deg)
{
    const float steps_per_rev =
        MOTOR_FULL_STEPS_PER_REV *
        MOTOR_MICROSTEPS;

    if (theta1_deg)
    {
        *theta1_deg =
            ((float)motor1_position_steps * 360.0f) /
            steps_per_rev;
    }

    if (theta2_deg)
    {
        *theta2_deg =
            ((float)motor2_position_steps * 360.0f) /
            steps_per_rev;
    }

    if (theta3_deg)
    {
        *theta3_deg =
            ((float)motor3_position_steps * 360.0f) /
            steps_per_rev;
    }
}

void spm_motors_get_target_angles(
    float *theta1_deg,
    float *theta2_deg,
    float *theta3_deg)
{
    const float steps_per_rev =
        MOTOR_FULL_STEPS_PER_REV *
        MOTOR_MICROSTEPS;

    if (theta1_deg)
    {
        *theta1_deg =
            ((float)motor1_target_steps * 360.0f) /
            steps_per_rev;
    }

    if (theta2_deg)
    {
        *theta2_deg =
            ((float)motor2_target_steps * 360.0f) /
            steps_per_rev;
    }

    if (theta3_deg)
    {
        *theta3_deg =
            ((float)motor3_target_steps * 360.0f) /
            steps_per_rev;
    }
}