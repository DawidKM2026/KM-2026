/* #pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MOTOR_SURGE_1,
    MOTOR_SURGE_2,
    MOTOR_SWAY,
    MOTOR_COUNT
} motor_id_t;

//Komendy możliwe do wywołania z CJOY'a
typedef enum
{
    MOVE_TO,
    MOVE_BY
} motion_type_t;


void init_stepper_motor_timers(void);


void motor_send_command(
    motion_type_t type,
    int32_t x,
    int32_t y);


void motor_start(
    motor_id_t motor_id,
    bool direction,
    uint32_t target_rpm);


void motor_stop(
    motor_id_t motor_id);


void motor_set_speed(
    motor_id_t motor_id,
    uint32_t motor_rpm);

void motor_set_direction(
    motor_id_t motor_id,
    bool direction);

void motor_button_on_off(void);

void motor_move_to(
    int32_t x_mm,
    int32_t y_mm);

void motor_move_by(
    int32_t wychylenie_x,
    int32_t wychylenie_y);

bool motor_homing(void);

void motor_limit_switch_x(void); */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/gptimer.h"

typedef enum {
    MOTOR_SURGE_1 = 0,
    MOTOR_SURGE_2,
    MOTOR_SWAY,
    MOTOR_COUNT
} motor_id_t;

typedef enum {
    MOVE_TO = 0,
    MOVE_BY
} motion_type_t;

//Inicjalizacja sterowania silnikami
void init_stepper_motor_timers(void);

//Wysłanie komendy ruchu
void motor_send_command(
    motion_type_t type,
    int32_t x,
    int32_t y);

//Ruch do pozycji w milimetrach
void motor_move_to(
    int32_t x_mm,
    int32_t y_mm);

//Ruch ręczny joystickiem
void motor_move_by(
    int32_t wychylenie_x,
    int32_t wychylenie_y);

//Uruchomienie silnika
void motor_start(
    motor_id_t motor_id,
    bool direction,
    uint32_t target_rpm);

//Zatrzymanie silnika
void motor_stop(
    motor_id_t motor_id);

//Ustawienie prędkości silnika
void motor_set_speed(
    motor_id_t motor_id,
    uint32_t motor_rpm);

//Ustawienie kierunku silnika
void motor_set_direction(
    motor_id_t motor_id,
    bool direction);

//Bazowanie platformy
bool motor_homing(void);

//Aktualizacja aktualnej pozycji
void update_current_position(void);

//Obsługa przycisku silników
void motor_button_on_off(void);