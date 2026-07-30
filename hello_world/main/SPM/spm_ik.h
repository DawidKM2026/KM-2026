#ifndef SPM_IK_H
#define SPM_IK_H

#include <stdbool.h>

typedef struct
{
    float theta1_deg;
    float theta2_deg;
    float theta3_deg;
} spm_angles_t;

bool spm_calculate_ik(
    float roll_deg,
    float pitch_deg,
    float yaw_deg,
    spm_angles_t *angles);

#endif