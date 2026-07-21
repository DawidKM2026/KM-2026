#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t x; //Aktualna pozycja, przyjmowana zawsze jako zero
    int32_t y;
    int32_t target_x; //Docelowa liczba kroków
    int32_t target_y;
    int32_t dx; //Bezwględna liczba  (wartość bezwględna targetu)
    int32_t dy;
    int32_t sx; //Kierunki ruchu
    int32_t sy;
    int32_t error; //Odchylenie od idealnej lini, po której chcemy, żeby układ się poruszał
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
