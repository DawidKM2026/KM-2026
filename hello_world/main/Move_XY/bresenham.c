#include "bresenham.h"
#include <stdlib.h>

//Przygotowanie ruchu po linii
void bresenham_init(
    bresenham_t *state,
    int32_t steps_x,
    int32_t steps_y)
{
    if (state == NULL) {
        return;
    }

    state->x = 0;
    state->y = 0;
    state->target_x = steps_x;
    state->target_y = steps_y;
    state->dx = abs(steps_x);
    state->dy = abs(steps_y);
    state->sx = steps_x > 0 ? 1 : steps_x < 0 ? -1 : 0;
    state->sy = steps_y > 0 ? 1 : steps_y < 0 ? -1 : 0;
    state->error = state->dx - state->dy;
    state->finished = steps_x == 0 && steps_y == 0;
}

//Wyznaczenie następnego kroku
bool bresenham_next(
    bresenham_t *state,
    bool *step_x,
    bool *step_y)
{
    if (state == NULL || step_x == NULL || step_y == NULL) {
        return false;
    }

    *step_x = false;
    *step_y = false;

    if (state->finished) {
        return false;
    }

    int64_t error_double = (int64_t)state->error * 2;

    if (error_double > -(int64_t)state->dy) {
        state->error -= state->dy;
        state->x += state->sx;
        *step_x = true;
    }

    if (error_double < (int64_t)state->dx) {
        state->error += state->dx;
        state->y += state->sy;
        *step_y = true;
    }

    if (state->x == state->target_x &&
        state->y == state->target_y) {
        state->finished = true;
    }

    return true;
}
