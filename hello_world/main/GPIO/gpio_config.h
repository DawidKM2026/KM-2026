#pragma once

#include "driver/gpio.h"

//Przyciski
#define BUTTON_PIN GPIO_NUM_0
#define EMERGENCY_STOP_PIN GPIO_NUM_33

//Sterowanie platformą w osi XY
#define STEP_X_1_PIN   GPIO_NUM_1
#define DIR_X_1_PIN    GPIO_NUM_2

#define STEP_X_2_PIN   GPIO_NUM_4
#define DIR_X_2_PIN    GPIO_NUM_5

#define EN_X_PIN     GPIO_NUM_6

#define STEP_Y_PIN   GPIO_NUM_8
#define DIR_Y_PIN    GPIO_NUM_7
#define EN_Y_PIN     GPIO_NUM_9

#define ENCODER_A_X_1_PIN GPIO_NUM_10
#define ENCODER_B_X_1_PIN GPIO_NUM_11
#define LIMIT_SWITCH_X_1_PIN GPIO_NUM_12

#define ENCODER_A_X_2_PIN GPIO_NUM_13
#define ENCODER_B_X_2_PIN GPIO_NUM_14
#define LIMIT_SWITCH_X_2_PIN GPIO_NUM_15

#define ENCODER_A_Y_PIN GPIO_NUM_16
#define ENCODER_B_Y_PIN GPIO_NUM_17
#define LIMIT_SWITCH_Y_PIN GPIO_NUM_18

//Sterowanie pochyleniem i obrotem platformy (pitch, roll, heading)
#define SPM_STEP_1_PIN GPIO_NUM_34
#define SPM_DIR_1_PIN GPIO_NUM_35

#define SPM_EN_PIN GPIO_NUM_36

#define SPM_STEP_2_PIN GPIO_NUM_37
#define SPM_DIR_2_PIN GPIO_NUM_38

#define SPM_STEP_3_PIN GPIO_NUM_39
#define SPM_DIR_3_PIN GPIO_NUM_40

//Sterowanie thrusterami statku + pomiar z żyroskopu i akcelerometru
#define UART_1 GPIO_NUM_41
#define UART_2 GPIO_NUM_42

//Wolne piny:
/* 
Bezpośrednio lutowane:
 - GP3 (TP4)
 - GP21 (TP5)

Wyprowadzone piny:
 - brak wolnych
 */


void init_gpio(void);