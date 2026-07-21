#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "esp_err.h"
#include "esp_rom_sys.h"

#include "gpio_config.h"
#include "encoders.h"
#include "stepper_motor.h"
#include "bresenham.h"

//Parametry mechaniczne
#define TIMER_RESOLUTION_HZ            1000000U
#define MOTOR_FULL_STEPS_PER_REV       200U
#define MOTOR_MICROSTEPS               16U
#define MOTOR_STEPS_PER_MM             256U
#define ENCODER_COUNTS_PER_MM          64U

//Parametry ruchu
#define AUTO_MOVE_SPEED_RPM            15U
#define AUTO_MOVE_TIMEOUT_MS           10000U
#define POSITION_TOLERANCE_COUNTS      8
#define X_SYNC_WARNING_COUNTS          16
#define X_SYNC_FAULT_COUNTS            64
#define STEP_PULSE_HIGH_US             5U
#define MANUAL_MAX_RPM                 45U
#define MANUAL_DEAD_ZONE               5

//Komenda silników
typedef struct {
    motion_type_t type;
    int32_t x;
    int32_t y;
} motor_command_t;

//Parametry pojedynczego silnika
typedef struct {
    gptimer_handle_t timer;
    gpio_num_t step_pin;
    gpio_num_t dir_pin;
    gpio_num_t enable_pin;
    volatile bool step_state;
    bool timer_running;
    bool direction_inverted;
} motor_t;

//Kolejka komend
static QueueHandle_t motor_command_queue = NULL;

//Wątek silników
static TaskHandle_t motor_task_handle = NULL;

//Stan przycisku
static bool enabled = false;
static bool last_state = true;
static volatile bool stop_request = false;

//Pozycja w impulsach enkodera
extern int32_t current_x;
extern int32_t current_x1;
extern int32_t current_x2;
extern int32_t current_y;

//Limity pozycji w milimetrach
extern int max_x_limit;
extern int min_x_limit;
extern int max_y_limit;
extern int min_y_limit;

//Konfiguracja silników
static motor_t motors[MOTOR_COUNT] = {
    [MOTOR_SURGE_1] = {
        .timer = NULL,
        .step_pin = STEP_X_1_PIN,
        .dir_pin = DIR_X_1_PIN,
        .enable_pin = EN_X_1_PIN,
        .step_state = false,
        .timer_running = false,
        .direction_inverted = false
    },
    [MOTOR_SURGE_2] = {
        .timer = NULL,
        .step_pin = STEP_X_2_PIN,
        .dir_pin = DIR_X_2_PIN,
        .enable_pin = EN_X_2_PIN,
        .step_state = false,
        .timer_running = false,
        .direction_inverted = true
    },
    [MOTOR_SWAY] = {
        .timer = NULL,
        .step_pin = STEP_Y_PIN,
        .dir_pin = DIR_Y_PIN,
        .enable_pin = EN_Y_PIN,
        .step_state = false,
        .timer_running = false,
        .direction_inverted = false
    }
};

//Ograniczenie pozycji
static int32_t clamp_position(
    int32_t value,
    int32_t minimum,
    int32_t maximum)
{
    if (value > maximum) {
        return maximum;
    }

    if (value < minimum) {
        return minimum;
    }

    return value;
}

//Obliczenie kierunku ruchu
static int32_t get_move_direction(
    int32_t current_position,
    int32_t target_position)
{
    if (target_position > current_position) {
        return 1;
    }

    if (target_position < current_position) {
        return -1;
    }

    return 0;
}

//Mapowanie kierunku X1 (-1/1 -> false/true)
static bool get_x1_motor_direction(
    int32_t logical_direction)
{
    return logical_direction > 0;
}

//Mapowanie kierunku X2
static bool get_x2_motor_direction(
    int32_t logical_direction)
{
    return logical_direction > 0;
}

//Mapowanie kierunku Y
static bool get_y_motor_direction(
    int32_t logical_direction)
{
    return logical_direction > 0 ? false : true;
}

//Uruchomienie timera silnika do odliczania impulsów
static esp_err_t motor_timer_start(
    motor_id_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    motor_t *motor = &motors[motor_id];

    if (motor->timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (motor->timer_running) {
        return ESP_OK;
    }

    esp_err_t error = gptimer_start(motor->timer);

    if (error == ESP_OK) {
        motor->timer_running = true;
    }

    return error;
}

//Zatrzymanie timera silnika
static esp_err_t motor_timer_stop(
    motor_id_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    motor_t *motor = &motors[motor_id];

    if (motor->timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!motor->timer_running) {
        motor->step_state = false;
        gpio_set_level(motor->step_pin, 0);
        return ESP_OK;
    }

    esp_err_t error = gptimer_stop(motor->timer);

    if (error == ESP_OK) {
        motor->timer_running = false;
        motor->step_state = false;
        gpio_set_level(motor->step_pin, 0);
    }

    return error;
}

//Callback timera ruchu (wywoływane przy każdym alarmie [zmianie kroku 0->1 / 1->0])
static bool IRAM_ATTR step_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *event_data,
    void *user_ctx)
{
    motor_t *motor = (motor_t *)user_ctx;

    motor->step_state = !motor->step_state;
    gpio_set_level(motor->step_pin, motor->step_state);

    return false;
}

//Inicjalizacja timera silnika
static void motor_timer_init(
    motor_id_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return;
    }

    motor_t *motor = &motors[motor_id];

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RESOLUTION_HZ
    };

    ESP_ERROR_CHECK(gptimer_new_timer(
        &timer_config,
        &motor->timer));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = step_timer_callback
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(
        motor->timer,
        &callbacks,
        motor));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 5000,
        .flags.auto_reload_on_alarm = true
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(
        motor->timer,
        &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(motor->timer));

    gpio_set_level(motor->step_pin, 0);
    gpio_set_level(motor->enable_pin, 1);

    motor->step_state = false;
    motor->timer_running = false;
}

//Ustawienie kierunku silnika
void motor_set_direction(
    motor_id_t motor_id,
    bool direction)
{
    if (motor_id >= MOTOR_COUNT) {
        return;
    }

    motor_t *motor = &motors[motor_id];
    bool gpio_direction = direction;

    if (motor->direction_inverted) {
        gpio_direction = !gpio_direction;
    }

    gpio_set_level(motor->dir_pin, gpio_direction);
}

//Zatrzymanie silnika
void motor_stop(
    motor_id_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return;
    }

    esp_err_t error = motor_timer_stop(motor_id);

    if (error != ESP_OK) {
        printf(
            "Blad zatrzymania silnika %d: %s\n",
            motor_id,
            esp_err_to_name(error));
    }

    motors[motor_id].step_state = false;
    gpio_set_level(motors[motor_id].step_pin, 0);
    gpio_set_level(motors[motor_id].enable_pin, 1);
}

//Zatrzymanie wszystkich silników
static void motor_stop_all(void)
{
    motor_stop(MOTOR_SURGE_1);
    motor_stop(MOTOR_SURGE_2);
    motor_stop(MOTOR_SWAY);
}

//Ustawienie prędkości silnika
static esp_err_t motor_set_speed(
    motor_id_t motor_id,
    uint32_t motor_rpm)
{
    if (motor_id >= MOTOR_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (motor_rpm == 0) {
        motor_stop(motor_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!enabled || stop_request) {
            return ESP_ERR_INVALID_STATE;
        }

    uint64_t denominator =
        2ULL *
        MOTOR_FULL_STEPS_PER_REV *
        MOTOR_MICROSTEPS *
        motor_rpm;

    uint64_t alarm_64 =
        (TIMER_RESOLUTION_HZ * 60ULL) /
        denominator;

    if (alarm_64 == 0) {
        alarm_64 = 1;
    }

    if (alarm_64 > UINT32_MAX) {
        alarm_64 = UINT32_MAX;
    }

    uint32_t alarm_count = (uint32_t)alarm_64;

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = alarm_count,
        .flags.auto_reload_on_alarm = true
    };

    if (motors[motor_id].timer == NULL){
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = gptimer_set_alarm_action(
        motors[motor_id].timer,
        &alarm_config);

    if (error != ESP_OK) {
        printf(
            "Blad predkosci silnika %d: %s\n",
            motor_id,
            esp_err_to_name(error));
        return error;
    }

    gpio_set_level(motors[motor_id].enable_pin, 0);
    return ESP_OK;
}

//Uruchomienie silnika
esp_err_t motor_start(
    motor_id_t motor_id,
    bool direction,
    uint32_t target_rpm)
{
    if (motor_id >= MOTOR_COUNT || target_rpm == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    motor_set_direction(motor_id, direction);
    esp_err_t error = motor_set_speed(motor_id, target_rpm);

    
    if (error != ESP_OK) {
        return error;
    }
        
    if (!enabled || stop_request) {
            return ESP_ERR_INVALID_STATE;
        }


    return motor_timer_start(motor_id);
}

//Wysłanie komendy ruchu
void motor_send_command(
    motion_type_t type,
    int32_t x,
    int32_t y)
{
    if (!enabled || stop_request) {
        return;
    }
    if (motor_command_queue == NULL) {
        printf("Kolejka silnikow nie jest gotowa\n");
        return;
    }

    motor_command_t command = {
        .type = type,
        .x = x,
        .y = y
    };

    xQueueOverwrite(motor_command_queue, &command);
}

//Wątek komend silnika
static void motor_task(void *parameters)
{
    motor_command_t command;

    while (true) {
        if (xQueueReceive(
                motor_command_queue,
                &command,
                portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (command.type) {
            case MOVE_TO:
                motor_move_to(command.x, command.y);
                break;

            case MOVE_BY:
                motor_move_by(command.x, command.y);
                break;

            default:
                break;
        }
    }
}

//Inicjalizacja sterowania silnikami
void init_stepper_motor_timers(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++) {
        motor_timer_init((motor_id_t)i);
    }

    motor_command_queue = xQueueCreate(
        1,
        sizeof(motor_command_t));

    if (motor_command_queue == NULL) {
        printf("Blad tworzenia kolejki silnikow\n");
        return;
    }

    BaseType_t result = xTaskCreate(
        motor_task,
        "motor_task",
        4096,
        NULL,
        5,
        &motor_task_handle);

    if (result != pdPASS) {
        printf("Blad tworzenia motor_task\n");
        motor_task_handle = NULL;
    }
}

//Przygotowanie silników do Bresenhama (wyłączenie wszystkich silników)
static void prepare_motors_for_bresenham(void)
{
    motor_timer_stop(MOTOR_SURGE_1);
    motor_timer_stop(MOTOR_SURGE_2);
    motor_timer_stop(MOTOR_SWAY);

    gpio_set_level(STEP_X_1_PIN, 0);
    gpio_set_level(STEP_X_2_PIN, 0);
    gpio_set_level(STEP_Y_PIN, 0);
}

//Włączenie potrzebnych sterowników
static void enable_bresenham_drivers(
    bool use_x,
    bool use_y)
{
    if (use_x) {
        gpio_set_level(EN_X_1_PIN, 0);
        gpio_set_level(EN_X_2_PIN, 0);
    }

    if (use_y) {
        gpio_set_level(EN_Y_PIN, 0);
    }
}

//Zatrzymanie ruchu Bresenhama
static void stop_bresenham_motion(void)
{
    gpio_set_level(STEP_X_1_PIN, 0);
    gpio_set_level(STEP_X_2_PIN, 0);
    gpio_set_level(STEP_Y_PIN, 0);

    gpio_set_level(EN_X_1_PIN, 1);
    gpio_set_level(EN_X_2_PIN, 1);
    gpio_set_level(EN_Y_PIN, 1);
}

//Obliczenie okresu mikrokroku
static uint32_t calculate_step_period_us(
    uint32_t rpm)
{
    if (rpm == 0) {
        rpm = 1;
    }

    uint64_t steps_per_minute =
        (uint64_t)rpm *
        MOTOR_FULL_STEPS_PER_REV *
        MOTOR_MICROSTEPS;

    uint64_t period_us =
        60000000ULL / steps_per_minute;

    if (period_us <= STEP_PULSE_HIGH_US) {
        period_us = STEP_PULSE_HIGH_US + 1;
    }

    return (uint32_t)period_us;
}

//Przeliczenie enkodera na mikrokroki
static int32_t encoder_counts_to_motor_steps(
    int32_t encoder_counts)
{
    int64_t counts = encoder_counts;

    if (counts < 0) {
        counts = -counts;
    }

    int64_t steps =
        counts * MOTOR_STEPS_PER_MM;

    steps += ENCODER_COUNTS_PER_MM / 2;
    steps /= ENCODER_COUNTS_PER_MM;

    if (steps > INT32_MAX) {
        steps = INT32_MAX;
    }

    return (int32_t)steps;
}

//Sprawdzenie osiągnięcia celu
static bool encoder_target_reached(
    int32_t encoder_position,
    int32_t target_position,
    int32_t direction)
{
    if (direction > 0) {
        return encoder_position >=
               target_position - POSITION_TOLERANCE_COUNTS;
    }

    if (direction < 0) {
        return encoder_position <=
               target_position + POSITION_TOLERANCE_COUNTS;
    }

    return true;
}

//Sprawdzenie przerwania ruchu
static bool motor_move_should_stop(
    TickType_t start_tick)
{
    if (stop_request) {
        return true;
    }
        

    if (motor_command_queue != NULL &&
        uxQueueMessagesWaiting(motor_command_queue) > 0) {
        return true;
    }

    TickType_t elapsed =
        xTaskGetTickCount() - start_tick;

    return elapsed >=
           pdMS_TO_TICKS(AUTO_MOVE_TIMEOUT_MS);
}

//Generowanie jednego impulsu
static void generate_step_pulse(
    bool step_x1,
    bool step_x2,
    bool step_y,
    uint32_t step_period_us)
{
    if (step_x1) {
        gpio_set_level(STEP_X_1_PIN, 1);
    }

    if (step_x2) {
        gpio_set_level(STEP_X_2_PIN, 1);
    }

    if (step_y) {
        gpio_set_level(STEP_Y_PIN, 1);
    }

    esp_rom_delay_us(STEP_PULSE_HIGH_US);

    if (step_x1) {
        gpio_set_level(STEP_X_1_PIN, 0);
    }

    if (step_x2) {
        gpio_set_level(STEP_X_2_PIN, 0);
    }

    if (step_y) {
        gpio_set_level(STEP_Y_PIN, 0);
    }

    esp_rom_delay_us(
        step_period_us - STEP_PULSE_HIGH_US);
}

//Aktualizacja bieżącej pozycji
void update_current_position(void)
{
    int32_t encoder_x1 =
        motor_encoder_get_count(ENCODER_X_1);

    int32_t encoder_x2 =
        motor_encoder_get_count(ENCODER_X_2);

    current_x1 = encoder_x1;
    current_x2 = encoder_x2;
    current_x = (int32_t)(
        ((int64_t)encoder_x1 + encoder_x2) / 2);
    

    current_y =
        motor_encoder_get_count(ENCODER_Y);
}

//Ruch do pozycji algorytmem Bresenhama
esp_err_t motor_move_to(
    int32_t x_mm,
    int32_t y_mm)
{
    if (!enabled || stop_request) {
            return ESP_ERR_INVALID_STATE;
        }
    
    uint8_t correction_attempt = 0;

    repeat_move:


    //Ograniczanie pola pracy
    x_mm = clamp_position(
        x_mm,
        min_x_limit,
        max_x_limit);

    y_mm = clamp_position(
        y_mm,
        min_y_limit,
        max_y_limit);

    //Punkt startowy
    int32_t start_x1 =
        motor_encoder_get_count(ENCODER_X_1);

    int32_t start_x2 =
        motor_encoder_get_count(ENCODER_X_2);

    int32_t start_x = (int32_t)(
        ((int64_t)start_x1 + start_x2) / 2);

    int32_t start_y =
        motor_encoder_get_count(ENCODER_Y);

    int64_t target_x_64 =
        (int64_t)x_mm * ENCODER_COUNTS_PER_MM;

    int64_t target_y_64 =
        (int64_t)y_mm * ENCODER_COUNTS_PER_MM;

    if (target_x_64 > INT32_MAX ||
        target_x_64 < INT32_MIN ||
        target_y_64 > INT32_MAX ||
        target_y_64 < INT32_MIN) {
        printf("Cel ruchu jest poza zakresem\n");
        return ESP_ERR_INVALID_ARG;
    }

    int32_t target_x = (int32_t)target_x_64;
    int32_t target_y = (int32_t)target_y_64;

    int32_t direction_x =
        get_move_direction(start_x, target_x);

    int32_t direction_y =
        get_move_direction(start_y, target_y);

    int64_t delta_x =
        (int64_t)target_x - start_x;

    int64_t delta_y =
        (int64_t)target_y - start_y;

    int64_t absolute_x = delta_x < 0 ? -delta_x : delta_x;
    int64_t absolute_y = delta_y < 0 ? -delta_y : delta_y;

    if (absolute_x <= POSITION_TOLERANCE_COUNTS &&
        absolute_y <= POSITION_TOLERANCE_COUNTS) {
        update_current_position();
        return ESP_OK;
    }

    int32_t steps_x = encoder_counts_to_motor_steps(
        (int32_t)absolute_x);

    int32_t steps_y = encoder_counts_to_motor_steps(
        (int32_t)absolute_y);

    int32_t signed_steps_x =
        direction_x < 0 ? -steps_x : steps_x;

    int32_t signed_steps_y =
        direction_y < 0 ? -steps_y : steps_y;

    //Wyłączenie silników    
    prepare_motors_for_bresenham();

    if (direction_x != 0) {
        motor_set_direction(
            MOTOR_SURGE_1,
            get_x1_motor_direction(direction_x));

        motor_set_direction(
            MOTOR_SURGE_2,
            get_x2_motor_direction(direction_x));
    }

    if (direction_y != 0) {
        motor_set_direction(
            MOTOR_SWAY,
            get_y_motor_direction(direction_y));
    }

    enable_bresenham_drivers(
        direction_x != 0,
        direction_y != 0);

    esp_rom_delay_us(10);

    uint32_t step_period_us =
        calculate_step_period_us(AUTO_MOVE_SPEED_RPM);

    bresenham_t line;
    bresenham_init(
        &line,
        signed_steps_x,
        signed_steps_y);

    TickType_t start_tick = xTaskGetTickCount();
    bool movement_error = false;

    while (!line.finished) {
        if (motor_move_should_stop(start_tick)) {
            printf("Ruch Bresenhama przerwany\n");
            movement_error = true;
            break;
        }

        //Zaczytaj aktualną pozycję silników
        int32_t encoder_x1 =
            motor_encoder_get_count(ENCODER_X_1);

        int32_t encoder_x2 =
            motor_encoder_get_count(ENCODER_X_2);

        int32_t encoder_y =
            motor_encoder_get_count(ENCODER_Y);

        //O ile udało się przesunąć
        int32_t progress_x1 =
            direction_x * (encoder_x1 - start_x1);

        int32_t progress_x2 =
            direction_x * (encoder_x2 - start_x2);

        int64_t x_difference =
            (int64_t)progress_x1 - progress_x2;

        int64_t absolute_difference =
            x_difference < 0 ? -x_difference : x_difference;

        if (absolute_difference >= X_SYNC_FAULT_COUNTS) {
            printf(
                "Blad synchronizacji X: roznica=%" PRId64 "\n",
                x_difference);
            movement_error = true;
            break;
        }

        //Sprawdzenie czy silniki są wystarczająco blisko celu (w granicach tolerancji)
        bool x1_finished = encoder_target_reached(
            encoder_x1,
            target_x,
            direction_x);

        bool x2_finished = encoder_target_reached(
            encoder_x2,
            target_x,
            direction_x);

        bool y_finished = encoder_target_reached(
            encoder_y,
            target_y,
            direction_y);

        //Jeśli jest okej to zakończ ruch
        if (x1_finished && x2_finished && y_finished) {
            break;
        }

        bool step_x = false;
        bool step_y = false;

        if (!bresenham_next(
                &line,
                &step_x,
                &step_y)) {
            break;
        }

        bool step_x1 = step_x && !x1_finished;
        bool step_x2 = step_x && !x2_finished;

        //Korekta wyprzedzenia X1
        if (x_difference > X_SYNC_WARNING_COUNTS) {
            step_x1 = false;
        }

        //Korekta wyprzedzenia X2
        if (x_difference < -X_SYNC_WARNING_COUNTS) {
            step_x2 = false;
        }

        if (y_finished) {
            step_y = false;
        }

        generate_step_pulse(
            step_x1,
            step_x2,
            step_y,
            step_period_us);
    }

    stop_bresenham_motion();
    update_current_position();

    if(movement_error){
        return ESP_FAIL;
    }

    //Liczenie uchybu
    int32_t error_x1 = target_x - current_x1;
    int32_t error_x2 = target_x - current_x2;
    int32_t error_y = target_y - current_y;


    //Sprawdzenie strefy martwej
    if (abs(error_x1) > POSITION_TOLERANCE_COUNTS || abs(error_x2) > POSITION_TOLERANCE_COUNTS || abs(error_y) > POSITION_TOLERANCE_COUNTS){
        correction_attempt++;

        printf(
            "Korekta pozycji %d: EX1=%" PRId32 
            " EX2=%" PRId32
            " EY=%" PRId32 "\n",
            correction_attempt,
            error_x1,
            error_x2,
            error_y);

        if (correction_attempt < 3)
        {
            goto repeat_move;
        }
    }

    //Finalna pozcyja każdego silnika
    int32_t final_x1 =
        motor_encoder_get_count(ENCODER_X_1);

    int32_t final_x2 =
        motor_encoder_get_count(ENCODER_X_2);

    int32_t final_y =
        motor_encoder_get_count(ENCODER_Y);

    if (!movement_error) {
        printf(
            "Bresenham zakonczony: X1=%" PRId32
            ", X2=%" PRId32
            ", Y=%" PRId32 "\n",
            final_x1,
            final_x2,
            final_y);
    } else {
        printf(
            "Blad ruchu: X1=%" PRId32
            ", X2=%" PRId32
            ", Y=%" PRId32 "\n",
            final_x1,
            final_x2,
            final_y);
    }
    return ESP_OK;
}

//Ruch ręczny joystickiem
esp_err_t motor_move_by(
    int32_t wychylenie_x,
    int32_t wychylenie_y)
{
    
    if (!enabled || stop_request) {
            return ESP_ERR_INVALID_STATE;
        }
    update_current_position();

    int32_t current_x_mm =
        current_x / ENCODER_COUNTS_PER_MM;

    int32_t current_y_mm =
        current_y / ENCODER_COUNTS_PER_MM;

    if (wychylenie_x > 0 && current_x_mm >= max_x_limit){
        wychylenie_x = 0;
    }
    if (wychylenie_x < 0 && current_x_mm <= min_x_limit){
        wychylenie_x = 0;
    }
    if (wychylenie_y > 0 && current_y_mm >= max_y_limit){
        wychylenie_y = 0;
    }
    if (wychylenie_y < 0 && current_y_mm <= min_y_limit){
        wychylenie_y = 0;
    }

    wychylenie_x = clamp_position(
        wychylenie_x,
        -100,
        100);

    wychylenie_y = clamp_position(
        wychylenie_y,
        -100,
        100);

    if (abs(wychylenie_x) < MANUAL_DEAD_ZONE) {
        wychylenie_x = 0;
    }

    if (abs(wychylenie_y) < MANUAL_DEAD_ZONE) {
        wychylenie_y = 0;
    }

    if (wychylenie_x == 0) {
        motor_stop(MOTOR_SURGE_1);
        motor_stop(MOTOR_SURGE_2);
    } else {
        uint32_t rpm =
            ((uint32_t)abs(wychylenie_x) *
             MANUAL_MAX_RPM) / 100U;

        if (rpm == 0) {
            rpm = 1;
        }

        bool direction = wychylenie_x > 0;

        motor_start(MOTOR_SURGE_1, direction, rpm);
        motor_start(MOTOR_SURGE_2, direction, rpm);
    }

    if (wychylenie_y == 0) {
        motor_stop(MOTOR_SWAY);
    } else {
        uint32_t rpm =
            ((uint32_t)abs(wychylenie_y) *
             MANUAL_MAX_RPM) / 100U;

        if (rpm == 0) {
            rpm = 1;
        }

        bool direction = wychylenie_y > 0;

        motor_start(MOTOR_SWAY, direction, rpm);
    }
    return ESP_OK;
}

//Bazowanie platformy
bool motor_homing(void)
{
    if(gpio_get_level(LIMIT_SWITCH_X_1_PIN)){
        motor_start(MOTOR_SURGE_1, 1, 15);
    }
    if(gpio_get_level(LIMIT_SWITCH_X_2_PIN)){
        motor_start(MOTOR_SURGE_2, 1, 15);
    }
    if(gpio_get_level(LIMIT_SWITCH_Y_PIN)){
        motor_start(MOTOR_SWAY, 1, 15);
    }
    

    TickType_t start = xTaskGetTickCount();

    while (
        gpio_get_level(LIMIT_SWITCH_X_1_PIN) == 1 ||
        gpio_get_level(LIMIT_SWITCH_X_2_PIN) == 1 ||
        gpio_get_level(LIMIT_SWITCH_Y_PIN) == 1) {

        if ((xTaskGetTickCount() - start) >=
            pdMS_TO_TICKS(20000)) {
            motor_stop_all();
            return false;
        }

        if (stop_request) {
            motor_stop_all();
            return false;
        }

        if (gpio_get_level(LIMIT_SWITCH_X_1_PIN) == 0) {
            motor_stop(MOTOR_SURGE_1);
        }

        if (gpio_get_level(LIMIT_SWITCH_X_2_PIN) == 0) {
            motor_stop(MOTOR_SURGE_2);
        }

        if (gpio_get_level(LIMIT_SWITCH_Y_PIN) == 0) {
            motor_stop(MOTOR_SWAY);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    motor_stop_all();

    motor_encoder_reset_position(ENCODER_X_1);
    motor_encoder_reset_position(ENCODER_X_2);
    motor_encoder_reset_position(ENCODER_Y);

    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_err_t err =
    motor_move_to(max_x_limit, max_y_limit);

    if (err != ESP_OK){
        return false;
    }

    motor_encoder_reset_position(ENCODER_X_1);
    motor_encoder_reset_position(ENCODER_X_2);
    motor_encoder_reset_position(ENCODER_Y);

    current_x = 0;
    current_x1 = 0;
    current_x2 = 0;
    current_y = 0;

    return true;
}

//Obsługa przycisku silników
void motor_button_on_off(void)
{
    bool state = gpio_get_level(BUTTON_PIN);

    if (last_state == 1 && state == 0) {
        enabled = !enabled;

        if (enabled) {
            stop_request = false;
            gpio_set_level(EN_X_1_PIN, 0);
            gpio_set_level(EN_X_2_PIN, 0);
            gpio_set_level(EN_Y_PIN, 0);
            printf("Silniki ON\n");
        } else {
            stop_request = true;
            if (motor_command_queue != NULL) {
                xQueueReset(motor_command_queue);
            }
            motor_stop_all();
            printf("Silniki OFF\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_state = state;
}
