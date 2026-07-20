#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t target_x;
    int32_t target_y;
    int32_t dx;
    int32_t dy;
    int32_t sx;
    int32_t sy;
    int32_t error;
    bool finished;
} bresenham_t;

//Przygotowanie ruchu po linii
void bresenham_init(
    bresenham_t *state,
    int32_t steps_x,
    int32_t steps_y);

//Wyznaczenie następnego kroku
bool bresenham_next(
    bresenham_t *state,
    bool *step_x,
    bool *step_y);
