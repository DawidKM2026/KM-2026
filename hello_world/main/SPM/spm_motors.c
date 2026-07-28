#include "spm_motors.h"

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "esp_rom_sys.h"

#include "gpio_config.h"

#define SPM_STEP_PULSE_US      5U
#define SPM_STEP_PERIOD_US     500U
#define MOTOR_FULL_STEPS_PER_REV 200U
#define MOTOR_MICROSTEPS         16U
#define STEP_PULSE_US            5U


//Struktura komend dla silników SPM
typedef struct
{
    //Ile kroków i w którym kierunku ma wykonać kazdy silnik
    int32_t motor1_steps;
    int32_t motor2_steps;
    int32_t motor3_steps;

    //Prędkość z jaką mają to wykonywać
    float speed_rps;
} spm_motors_command_t;

static QueueHandle_t spm_motors_queue = NULL;
static TaskHandle_t spm_motors_task_handle = NULL;

//Zamiana obrotów/sekundę -> pojedynczy okres
static uint32_t calculate_step_period_us(float speed_rps){
    if (speed_rps <= 0.0f) {
        speed_rps = 0.1f;
    }

    float microsteps_per_second = speed_rps * MOTOR_FULL_STEPS_PER_REV * MOTOR_MICROSTEPS;

    uint32_t period_us = (uint32_t)(1000000.0f / microsteps_per_second);

    if (period_us <= STEP_PULSE_US) {
        period_us = STEP_PULSE_US + 1;
    }

    return period_us;
}

//Załączanie sterowników dla wszystkich silników (trzymają wtedy moment i można nimi sterować)
static void spm_enable_all(void){
    gpio_set_level(SPM_EN_1_PIN, 1);
    gpio_set_level(SPM_EN_2_PIN, 1);
    gpio_set_level(SPM_EN_3_PIN, 1);
}

//Wyłączanie sterowników dla wszystkich silników (można swobodnie obracać wał)
static void spm_disable_all(void){
    gpio_set_level(SPM_EN_1_PIN, 0);
    gpio_set_level(SPM_EN_2_PIN, 0);
    gpio_set_level(SPM_EN_3_PIN, 0);
}

static void generate_step(gpio_num_t step_pin){
    gpio_set_level(step_pin, 1);

    esp_rom_delay_us(
        SPM_STEP_PULSE_US);

    gpio_set_level(step_pin, 0);
}

//Sprawdzenie który silnik musi wykonać najwięcej kroków
static int32_t get_max3(int32_t a, int32_t b, int32_t c){
    int32_t max = a;

    if (b > max) {
        max = b;
    }
    if (c > max) {
        max = c;
    }
    return max;
}

static void execute_move(int32_t motor1_steps,int32_t motor2_steps,int32_t motor3_steps,float speed_rps){
    uint32_t step_period_us =
    calculate_step_period_us(speed_rps);
    gpio_set_level(SPM_DIR_1_PIN,motor1_steps >= 0);

    gpio_set_level(SPM_DIR_2_PIN,motor2_steps >= 0);

    gpio_set_level(SPM_DIR_3_PIN,motor3_steps >= 0);

    int32_t steps1 = abs(motor1_steps);
    int32_t steps2 = abs(motor2_steps);
    int32_t steps3 = abs(motor3_steps);

    int32_t max_steps = get_max3(steps1,steps2,steps3);

    if (max_steps == 0) {
        return;
    }

    spm_enable_all();

    esp_rom_delay_us(10);

    int32_t accumulator1 = 0;
    int32_t accumulator2 = 0;
    int32_t accumulator3 = 0;

    for (int32_t i = 0; i < max_steps; i++) {

        accumulator1 += steps1;
        accumulator2 += steps2;
        accumulator3 += steps3;

        if (accumulator1 >= max_steps) {
            generate_step(SPM_STEP_1_PIN);
            accumulator1 -= max_steps;
        }

        if (accumulator2 >= max_steps) {
            generate_step(SPM_STEP_2_PIN);
            accumulator2 -= max_steps;
        }

        if (accumulator3 >= max_steps) {
            generate_step(SPM_STEP_3_PIN);
            accumulator3 -= max_steps;
        }

        esp_rom_delay_us(step_period_us - STEP_PULSE_US);
    }

    gpio_set_level(SPM_STEP_1_PIN, 0);
    gpio_set_level(SPM_STEP_2_PIN, 0);
    gpio_set_level(SPM_STEP_3_PIN, 0);

    spm_disable_all();
}

static void spm_motors_task(void *parameters){
    spm_motors_command_t command;

    while (true) {

        if (xQueueReceive(
                spm_motors_queue,
                &command,
                portMAX_DELAY) != pdTRUE) {
            continue;
        }

        execute_move(
            command.motor1_steps,
            command.motor2_steps,
            command.motor3_steps,
            command.speed_rps);
    }
}

void spm_motors_init(void){
    gpio_set_level(SPM_STEP_1_PIN, 0);
    gpio_set_level(SPM_STEP_2_PIN, 0);
    gpio_set_level(SPM_STEP_3_PIN, 0);

    spm_disable_all();

    spm_motors_queue =
        xQueueCreate(
            10,
            sizeof(spm_motors_command_t));

    if (spm_motors_queue == NULL) {
        printf(
            "SPM: blad tworzenia kolejki\n");
        return;
    }

    if (xTaskCreate(
            spm_motors_task,
            "spm_motors_task",
            4096,
            NULL,
            5,
            &spm_motors_task_handle)
        != pdPASS) {

        printf(
            "SPM: blad tworzenia taska\n");
    }
}

bool spm_motors_move_steps(
    int32_t motor1_steps,
    int32_t motor2_steps,
    int32_t motor3_steps,
    float speed_rps)
{
    if (spm_motors_queue == NULL) {
        return false;
    }

    spm_motors_command_t command = {
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

void spm_motors_stop(void)
{
    if (spm_motors_queue != NULL) {
        xQueueReset(
            spm_motors_queue);
    }

    gpio_set_level(
        SPM_STEP_1_PIN, 0);

    gpio_set_level(
        SPM_STEP_2_PIN, 0);

    gpio_set_level(
        SPM_STEP_3_PIN, 0);

    spm_disable_all();
}