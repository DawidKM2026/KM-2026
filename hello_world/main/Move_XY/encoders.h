#ifndef ENCODERS_H
#define ENCODERS_H

#include <stdint.h>

typedef enum
{
    ENCODER_X_1 = 0,
    ENCODER_X_2,
    ENCODER_Y,
    ENCODER_COUNT
} encoder_id_t;


#define ENCODER_CPR             200
#define ENCODER_COUNTS_PER_REV (ENCODER_CPR * 4)

void motor_encoder_init(encoder_id_t encoder);

int32_t motor_encoder_get_count(encoder_id_t encoder);

float motor_encoder_get_angle(encoder_id_t encoder);

void motor_encoder_reset_position(encoder_id_t encoder);

#endif