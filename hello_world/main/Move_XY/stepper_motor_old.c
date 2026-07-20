/*
 * Timer GPTimer:
 * resolution_hz = 1 000 000 Hz
 * 1 krok silnika = 2 alarmy
 * Silnik: 1.8° = 200 kroków/obrót
 * Śruba: 12.5 mm/obrót
 
 alarm_count | RPS  | RPM | mm/s
------------+------+-----+-------
    10000   | 0.25 |  15 |   3.1
     5000   | 0.50 |  30 |   6.3
     2500   | 1.00 |  60 |  12.5
     2000   | 1.25 |  75 |  15.6
     1250   | 2.00 | 120 |  25.0
     1000   | 2.50 | 150 |  31.3
      625   | 4.00 | 240 |  50.0 
      500   | 5.00 | 300 |  62.5
      250   |10.00 | 600 | 125.0
 
 * Wzory:
 * kroki_s     = 1000000 / (2 * alarm_count)
 * RPM         = kroki_s * 60 / 200
 * predkosc_mm = RPM * 12.5 / 60
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/pulse_cnt.h"

#include "esp_err.h"

#include "gpio_config.h"
#include "esp_now_comm.h"
#include "stepper_motor.h"
#include "encoders.h"


static bool motor_x1_running = false;
static bool motor_x2_running = false;
static bool motor_y_running  = false;


typedef struct{
    motion_type_t type;
    int32_t x;
    int32_t y;
} motor_command_t;

//Parametry pojedynczego silnika
typedef struct{
    gptimer_handle_t timer;
    gpio_num_t step_pin;
    gpio_num_t dir_pin;
    gpio_num_t enable_pin;

    volatile bool step_state;
    bool timer_running;
    bool direction_inverted;
} motor_t;

//Obsługa kolejki żądań
static QueueHandle_t motor_command_queue = NULL;

//Wywoływanie komend
void motor_send_command(motion_type_t type, int32_t x, int32_t y){
    if (motor_command_queue == NULL) {
        printf("Kolejka silnikow nie jest zainicjalizowana\n");
        return;
    }

    motor_command_t command = {
        .type = type,
        .x = x,
        .y = y
    };
    xQueueOverwrite(motor_command_queue, &command);
}


//Struktura silników
static motor_t motors[MOTOR_COUNT] ={
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

        /* Silnik X2 jest mechanicznie ustawiony odwrotnie */
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



//Wątek na pracę silnika 
static TaskHandle_t motor_task_handle = NULL;

//Przycisk START/STOP
static bool enabled = false;
static bool last_state = true;
static volatile bool stop_request = false;

//Obecna pozcyja
extern int32_t current_x;
extern int32_t current_y;

//Fizyczne wymiary pola, na którym porusza się makieta statku
extern int max_x_limit; 
extern int min_x_limit;
extern int max_y_limit;
extern int min_y_limit;


//Funkcja pomocnicza zabezpieczające przed uruchamianiem włączonego timera
static esp_err_t motor_timer_start(motor_id_t motor_id){
    if (motor_id >= MOTOR_COUNT){
        return ESP_ERR_INVALID_ARG;
    }
    motor_t *motor = &motors[motor_id];
    if (motor->timer == NULL){
        return ESP_ERR_INVALID_STATE;
    }
    if (motor->timer_running){
        return ESP_OK;
    }
    esp_err_t err = gptimer_start(motor->timer);
    if (err == ESP_OK){
        motor->timer_running = true;
    }
    return err;
}

//Funkcja pomocnicza zabezpieczające przed wyłączeniem wyłączonego timera
static esp_err_t motor_timer_stop(motor_id_t motor_id){
    if (motor_id >= MOTOR_COUNT){
        return ESP_ERR_INVALID_ARG;
    }
    motor_t *motor = &motors[motor_id];
    if (motor->timer == NULL){
        return ESP_ERR_INVALID_STATE;
    }
    if (!motor->timer_running){
        return ESP_OK;
    }
    esp_err_t err = gptimer_stop(motor->timer);
    if (err == ESP_OK){
        motor->timer_running = false;

        motor->step_state = false;
        gpio_set_level(motor->step_pin, 0);
    }
    return err;
}

void motor_task(void *pvParameters){
    motor_command_t command;

    while (1) {
        if (xQueueReceive(
                motor_command_queue,
                &command,
                portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (command.type) {
            case MOVE_TO:
                printf("MOVE_TO: x=%" PRId32 ", y=%" PRId32 "\n", command.x, command.y);
                motor_move_to(command.x, command.y);
                break;

            case MOVE_BY:
                printf("MOVE_BY: x=%" PRId32 ", y=%" PRId32 "\n", command.x, command.y);
                motor_move_by(command.x, command.y);
                break;

            default:
                break;
        }
    }
}

//Zatrzymanie silnika
void motor_stop(motor_id_t motor_id){
    if (motor_id >= MOTOR_COUNT){
        return;
    }

    motor_t *motor = &motors[motor_id];
    esp_err_t err = motor_timer_stop(motor_id);

    if (err != ESP_OK){
        printf(
            "Blad zatrzymania silnika %d: %s\n",
            motor_id,
            esp_err_to_name(err));
    }

    motor->step_state = false;

    gpio_set_level(motor->step_pin, 0);
    gpio_set_level(motor->enable_pin, 1);
}

static void motor_stop_all(void)
{
    motor_stop(MOTOR_SURGE_1);
    motor_stop(MOTOR_SURGE_2);
    motor_stop(MOTOR_SWAY);

    motor_x1_running = false;
    motor_x2_running = false;
    motor_y_running  = false;
}

//Ustawianie prędkości obrotowej silnika
void motor_set_speed(
    motor_id_t motor_id,
    uint32_t motor_rpm){
    if (motor_id >= MOTOR_COUNT){
        return;
    }

    motor_t *motor = &motors[motor_id];

    if (motor_rpm == 0){
        motor_stop(motor_id);
        return;
    }

    uint32_t alarm_count =
        150000U / (motor_rpm * 16U); //Wynika z tego, że silniki są ustawione na 1/16 kroku

    if (alarm_count == 0){
        alarm_count = 1;
    }

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = alarm_count,
        .flags.auto_reload_on_alarm = true,
    };

    if (motor->timer == NULL) {
        printf("Timer silnika %d nie jest zainicjalizowany\n", (int)motor_id);
        return;
    }

    esp_err_t err = gptimer_set_alarm_action(
        motor->timer,
        &alarm_config);

    if (err != ESP_OK){
        printf(
            "Blad ustawiania predkosci silnika %d: %s\n",
            motor_id,
            esp_err_to_name(err));

        return;
    }

    gpio_set_level(motor->enable_pin, 0);

    printf(
        "Silnik %d, RPM=%" PRIu32 ", alarm=%" PRIu32 "\n",
        motor_id,
        motor_rpm,
        alarm_count);
}

//Start silnika
void motor_start(motor_id_t motor_id,bool direction,uint32_t target_rpm){
    if (motor_id >= MOTOR_COUNT || target_rpm == 0) {
        return;
    }

    motor_set_direction(motor_id, direction);

    uint32_t rpm_step = target_rpm / 4U;

    if (rpm_step == 0) {
        rpm_step = 1;
    }

    motor_set_speed(motor_id, rpm_step);

    ESP_ERROR_CHECK(
        motor_timer_start(motor_id));

    for (uint32_t rpm = rpm_step*2U; rpm < target_rpm; rpm += rpm_step) {
        motor_set_speed(motor_id, rpm);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    motor_set_speed(motor_id, target_rpm);
}


//Wywoływanie alarmu po upływie określonego czasu
static bool IRAM_ATTR step_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *event_data,
    void *user_ctx){
    motor_t *motor = (motor_t *)user_ctx;

    motor->step_state = !motor->step_state;

    gpio_set_level(
        motor->step_pin,
        motor->step_state);

    return false;
}

//Inicjalizacja timera
static void motor_timer_init(motor_id_t motor_id){
    if (motor_id >= MOTOR_COUNT)
    {
        return;
    }

    motor_t *motor = &motors[motor_id];

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    ESP_ERROR_CHECK(
        gptimer_new_timer(
            &timer_config,
            &motor->timer));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = step_timer_callback,
    };

    ESP_ERROR_CHECK(
        gptimer_register_event_callbacks(
            motor->timer,
            &callbacks,
            motor));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 5000,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            motor->timer,
            &alarm_config));

    ESP_ERROR_CHECK(
        gptimer_enable(motor->timer));

    gpio_set_level(motor->step_pin, 0);
    gpio_set_level(motor->enable_pin, 1);

    motor->step_state = false;
    motor->timer_running = false;
}

void init_stepper_motor_timers(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++) {
        motor_timer_init((motor_id_t)i);
    }

    //Kolejka do nadpisywania komend    
    motor_command_queue = xQueueCreate(1, sizeof(motor_command_t));

    if (motor_command_queue == NULL) {
        printf("Blad tworzenia kolejki silnikow\n");
        return;
    }


    //Wątek na pracę silnika
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

//Wybierz kierunek ruchu
void motor_set_direction(motor_id_t motor_id,bool direction){
    if (motor_id >= MOTOR_COUNT){
        return;
    }

    motor_t *motor = &motors[motor_id];

    bool gpio_direction = direction;

    if (motor->direction_inverted){
        gpio_direction = !gpio_direction;
    }

    gpio_set_level(
        motor->dir_pin,
        gpio_direction);
}


//Stałe do sterowania silnikami
    #define ENCODER_COUNTS_PER_MM        64
    #define AUTO_MOVE_SPEED              15
    #define AUTO_MOVE_TIMEOUT_MS         10000
    #define AUTO_CONTROL_PERIOD_MS       2
    #define POSITION_TOLERANCE_COUNTS    8
    #define LINE_TOLERANCE_COUNTS        24
    #define X_SYNC_TOLERANCE_COUNTS      16

//Ograniczenie przemieszczenia
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

//Ustawianie kierunku ruchu
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

//Sprawdzanie czy cel został osiągnięty
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

//Sprawdzanie czy nie ma nowej komendy do wykonania lub czy nie ma timeout'u
static bool motor_move_should_stop(TickType_t start_tick)
{
    if (stop_request) {
        return true;
    }

    if (uxQueueMessagesWaiting(motor_command_queue) > 0) {
        return true;
    }

    TickType_t elapsed = xTaskGetTickCount() - start_tick;

    if (elapsed > pdMS_TO_TICKS(AUTO_MOVE_TIMEOUT_MS)) {
        return true;
    }

    return false;
}

//Mapowanie kierunków obrotu na GPIO
static uint8_t get_x1_motor_direction(int32_t logical_direction){
    return logical_direction > 0 ? 1 : 0;
}

static uint8_t get_x2_motor_direction(int32_t logical_direction){
    return logical_direction > 0 ? 1 : 0;
}

static uint8_t get_y_motor_direction(int32_t logical_direction){
    return logical_direction > 0 ? 0 : 1;
}

static void set_motor_running(int motor, uint8_t direction, bool *running){
    if (!(*running)) {
        motor_start(
            motor,
            direction,
            AUTO_MOVE_SPEED
        );

        *running = true;
    }
}

static void set_motor_stopped(int motor, bool *running){
    if (*running) {
        motor_stop(motor);
        *running = false;
    }
}


//Przesunięcie platformy w osi X i osi Y do zadanego punktu za pomocą algorytmu Bresenham'sa
void motor_move_to(int32_t x_mm, int32_t y_mm){

    x_mm = clamp_position(x_mm,min_x_limit,max_x_limit);
    y_mm = clamp_position(y_mm,min_y_limit,max_y_limit);

    int32_t target_x = x_mm * ENCODER_COUNTS_PER_MM;
    int32_t target_y = y_mm * ENCODER_COUNTS_PER_MM;

    int32_t start_x1 = motor_encoder_get_count(ENCODER_X_1);
    int32_t start_x2 = motor_encoder_get_count(ENCODER_X_2);
    int32_t start_x = (start_x1 + start_x2) / 2;
    int32_t start_y = motor_encoder_get_count(ENCODER_Y);

    int32_t direction_x = get_move_direction(start_x, target_x);
    int32_t direction_y = get_move_direction(start_y, target_y);

    int32_t distance_x = abs(target_x - start_x);
    int32_t distance_y = abs(target_y - start_y);

    if (distance_x <= POSITION_TOLERANCE_COUNTS && distance_y <= POSITION_TOLERANCE_COUNTS) {
        current_x = start_x;
        current_y = start_y;
        return;
    }

    motor_stop_all();

    TickType_t start_tick = xTaskGetTickCount();

    while (true) {
        if (motor_move_should_stop(start_tick)) {
            motor_stop_all();
            return;
        }

        int32_t encoder_x1 = motor_encoder_get_count(ENCODER_X_1);
        int32_t encoder_x2 = motor_encoder_get_count(ENCODER_X_2);
        int32_t encoder_x = (encoder_x1 + encoder_x2) / 2;
        int32_t encoder_y = motor_encoder_get_count(ENCODER_Y);
        current_x = encoder_x;
        current_y = encoder_y;

        bool x1_finished = encoder_target_reached(encoder_x1, target_x, direction_x);
        bool x2_finished =encoder_target_reached(encoder_x2, target_x, direction_x);
        bool y_finished =encoder_target_reached(encoder_y, target_y, direction_y);
        bool x_finished = x1_finished && x2_finished;

        if (x_finished && y_finished) {
            break;
        }

        int32_t progress_x = direction_x * (encoder_x - start_x);
        int32_t progress_y = direction_y * (encoder_y - start_y);

        if (progress_x < 0) {
            progress_x = 0;
        }

        if (progress_y < 0) {
            progress_y = 0;
        }

        if (progress_x > distance_x) {
            progress_x = distance_x;
        }

        if (progress_y > distance_y) {
            progress_y = distance_y;
        }


        //Synchronizacja ruchu w osiach X i Y, żeby docierały do mety w tym samym momencie
        int64_t line_error = (int64_t)progress_x * distance_y - (int64_t)progress_y * distance_x;
        int32_t dominant_distance = distance_x > distance_y
                ? distance_x
                : distance_y;

        int64_t line_tolerance = (int64_t)LINE_TOLERANCE_COUNTS * dominant_distance;

        bool allow_x = !x_finished;
        bool allow_y = !y_finished;

        if (distance_x == 0) {
            allow_x = false;
        } else if (distance_y == 0) {
            allow_y = false;
        } else {
            if (line_error > line_tolerance) {
                allow_x = false;
            }
            if (line_error < -line_tolerance) {
                allow_y = false;
            }
        }

        //Synchronizacja silników w osi X
        int32_t progress_x1 = direction_x * (encoder_x1 - start_x1);
        int32_t progress_x2 = direction_x * (encoder_x2 - start_x2);

        int32_t x_difference = progress_x1 - progress_x2;
        bool allow_x1 = allow_x && !x1_finished;
        bool allow_x2 = allow_x && !x2_finished;

    
        //X1 jest przed X2
        if (x_difference >
            X_SYNC_TOLERANCE_COUNTS) {
            allow_x1 = false;
        }

        //X2 jest przed X1 
        if (x_difference <
            -X_SYNC_TOLERANCE_COUNTS) {
            allow_x2 = false;
        }

        //Sterowanie silnikiem X1
        if (allow_x1 && direction_x != 0) {
            set_motor_running(
                MOTOR_SURGE_1,
                get_x1_motor_direction(direction_x),
                &motor_x1_running
            );
        } else {
            set_motor_stopped(
                MOTOR_SURGE_1,
                &motor_x1_running
            );
        }

        //Sterowanie silnikiem X2
        if (allow_x2 && direction_x != 0) {
            set_motor_running(
                MOTOR_SURGE_2,
                get_x2_motor_direction(direction_x),
                &motor_x2_running
            );
        } else {
            set_motor_stopped(
                MOTOR_SURGE_2,
                &motor_x2_running
            );
        }

        //Sterowanie silnikiem Y
        if (allow_y && direction_y != 0) {
            set_motor_running(
                MOTOR_SWAY,
                get_y_motor_direction(direction_y),
                &motor_y_running
            );
        } else {
            set_motor_stopped(
                MOTOR_SWAY,
                &motor_y_running
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(AUTO_CONTROL_PERIOD_MS)
        );
    }

    motor_stop_all();
    int32_t final_x1 = motor_encoder_get_count(ENCODER_X_1);
    int32_t final_x2 = motor_encoder_get_count(ENCODER_X_2);

    current_x = (final_x1 + final_x2) / 2;
    current_y = motor_encoder_get_count(ENCODER_Y);
}

//Ręczne przesuwanie platformy w osi X i osi Y
void motor_move_by(int32_t wychylenie_x, int32_t wychylenie_y)
{
    if(wychylenie_x>100){
        wychylenie_x=100;
    }
    if(wychylenie_x<-100){
        wychylenie_x=-100;
    }
    if(wychylenie_y>100){
        wychylenie_y=100;
    }
    if(wychylenie_y<-100){
        wychylenie_y=-100;
    }

    //Joystick nieruchomo
    if (wychylenie_x == 0 && wychylenie_y == 0){
        motor_stop_all();
        return;
    }

    // Surge Forward
    if (wychylenie_x > 0){
        motor_start(MOTOR_SURGE_1,1,((wychylenie_x*45)/100));   
        motor_start(MOTOR_SURGE_2,1,((wychylenie_x*45)/100));
    }

    // Surge Backward
    else if (wychylenie_x < 0){
        motor_start(MOTOR_SURGE_1,0,(abs(wychylenie_x*45)/100));
        motor_start(MOTOR_SURGE_2,0,(abs(wychylenie_x*45)/100));
    }

    // Sway Forward
    if (wychylenie_y > 0){
        motor_start(MOTOR_SWAY,1,((wychylenie_y*45)/100));
    }

    // Sway Backward
    else if (wychylenie_y < 0){
        motor_start(MOTOR_SWAY,0,(abs(wychylenie_y*45)/100));   
}
}


//Bazowanie statku
bool motor_homing(void){
    motor_start(MOTOR_SURGE_1,1,15);
    motor_start(MOTOR_SURGE_2,1,15);   
    motor_start(MOTOR_SWAY,1,15);   
    TickType_t start = xTaskGetTickCount();
    while(gpio_get_level(LIMIT_SWITCH_X_1_PIN)==1 || gpio_get_level(LIMIT_SWITCH_X_2_PIN)==1 || gpio_get_level(LIMIT_SWITCH_Y_PIN)==1){

        if ((xTaskGetTickCount() - start) >pdMS_TO_TICKS(20000)){
            return false;
        }
        if(gpio_get_level(LIMIT_SWITCH_X_1_PIN)==0){
            motor_set_speed(MOTOR_SURGE_1,0);
        }
        if(gpio_get_level(LIMIT_SWITCH_X_2_PIN)==0){
            motor_set_speed(MOTOR_SURGE_2,0);
        }
        if(gpio_get_level(LIMIT_SWITCH_Y_PIN)==0){
            motor_set_speed(MOTOR_SWAY,0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    } 

    //Reset układu współrzędnych   
    motor_encoder_reset_position(ENCODER_X_1);
    motor_encoder_reset_position(ENCODER_X_2);
    motor_encoder_reset_position(ENCODER_Y);
    vTaskDelay(pdMS_TO_TICKS(1000));
    motor_move_to(max_x_limit, max_y_limit);
    motor_encoder_reset_position(ENCODER_X_1);
    motor_encoder_reset_position(ENCODER_X_2);
    motor_encoder_reset_position(ENCODER_Y);
    current_x=0;
    current_y=0;

    return true;
}


//Switch do właczania/wyłączania silnika 
void motor_button_on_off(void){
    bool state = gpio_get_level(BUTTON_PIN);

    if (last_state == 1 && state == 0){
        enabled = !enabled;

        if (enabled){
            stop_request=false;
            gpio_set_level(EN_X_1_PIN, 0);
            gpio_set_level(EN_X_2_PIN, 0);
            gpio_set_level(EN_Y_PIN, 0);
            printf("Silniki ON\n");
        }
        else{
            stop_request=true;

            motor_stop_all();

            printf("Silniki OFF\n");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_state = state;
}

//Switch krańcówka 
void motor_limit_switch_x(void){
    bool state_x_1 = gpio_get_level(LIMIT_SWITCH_X_1_PIN);
    bool state_x_2 = gpio_get_level(LIMIT_SWITCH_X_2_PIN);
    bool state_y = gpio_get_level(LIMIT_SWITCH_Y_PIN);

    if (state_x_1 == 0){
        motor_stop(MOTOR_SURGE_1);
    }

    if (state_x_2 == 0){
        motor_stop(MOTOR_SURGE_2);
    }

    if (state_y == 0){
        motor_stop(MOTOR_SWAY);
    }
}