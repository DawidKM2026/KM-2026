#ifndef SPM_MOTORS_H
#define SPM_MOTORS_H

#include <stdint.h>
#include <stdbool.h>

void spm_motors_init(void);

bool spm_motors_move_steps(
    int32_t motor1_steps,
    int32_t motor2_steps,
    int32_t motor3_steps,
    float speed_rps);

void spm_motors_stop(void);

#endif