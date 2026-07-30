#include "spm_ik.h"

#include <math.h>

#define DEG2RAD(x) ((x) * 0.01745329251994f)
#define RAD2DEG(x) ((x) * 57.29577951308f)

/*
 * Geometria mechanizmu
 * Na razie wpisana na stałe.
 */

#define SPM_L1          40.0f
#define SPM_L2          80.0f
#define SPM_R_PLATFORM  35.0f
#define SPM_Z_HEIGHT    70.0f
#define SPM_TILT_DEG    30.0f

static void generate_rotation_matrix(
    float roll,
    float pitch,
    float yaw,
    float R[3][3])
{
    float sr = sinf(roll);
    float cr = cosf(roll);

    float sp = sinf(pitch);
    float cp = cosf(pitch);

    float sy = sinf(yaw);
    float cy = cosf(yaw);

    R[0][0] = cy * cp;
    R[0][1] = cy * sp * sr - sy * cr;
    R[0][2] = cy * sp * cr + sy * sr;

    R[1][0] = sy * cp;
    R[1][1] = sy * sp * sr + cy * cr;
    R[1][2] = sy * sp * cr - cy * sr;

    R[2][0] = -sp;
    R[2][1] = cp * sr;
    R[2][2] = cp * cr;
}

bool spm_calculate_ik(
    float roll_deg,
    float pitch_deg,
    float yaw_deg,
    spm_angles_t *angles)
{
    if (angles == NULL)
    {
        return false;
    }

    float roll  = DEG2RAD(roll_deg);
    float pitch = DEG2RAD(pitch_deg);
    float yaw   = DEG2RAD(yaw_deg);

    float tilt = DEG2RAD(SPM_TILT_DEG);

    float Rxy = SPM_L1 * cosf(tilt);
    float Za  = SPM_L1 * sinf(tilt);

    float Rm[3][3];

    generate_rotation_matrix(
        roll,
        pitch,
        yaw,
        Rm);

    const float beta_deg[3] =
    {
        90.0f,
        210.0f,
        330.0f
    };

    float theta[3];

    for (int i = 0; i < 3; i++)
    {
        float beta = DEG2RAD(beta_deg[i]);

        float px =
            SPM_R_PLATFORM * cosf(beta);

        float py =
            SPM_R_PLATFORM * sinf(beta);

        float P[3];

        P[0] =
            Rm[0][0] * px +
            Rm[0][1] * py;

        P[1] =
            Rm[1][0] * px +
            Rm[1][1] * py;

        P[2] =
            SPM_Z_HEIGHT +
            Rm[2][0] * px +
            Rm[2][1] * py;

        float E =
            2.0f * Rxy * P[0];

        float F =
            2.0f * Rxy * P[1];

        float G =
            P[0] * P[0]
            + P[1] * P[1]
            + Rxy * Rxy
            + (P[2] - Za) * (P[2] - Za)
            - SPM_L2 * SPM_L2;

        float Rval =
            sqrtf(E * E + F * F);

        if (fabsf(G) > Rval)
        {
            return false;
        }

        float alpha =
            atan2f(F, E);

        theta[i] =
            alpha -
            acosf(
                fminf(
                    1.0f,
                    fmaxf(
                        -1.0f,
                        G / Rval)));
    }

    angles->theta1_deg =
        RAD2DEG(theta[0]);

    angles->theta2_deg =
        RAD2DEG(theta[1]);

    angles->theta3_deg =
        RAD2DEG(theta[2]);

    return true;
}