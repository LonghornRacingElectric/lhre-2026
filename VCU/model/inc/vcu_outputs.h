#ifndef VCU_OUTPUTS_H
#define VCU_OUTPUTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    /* Normalized pedal travel [0.0, 1.0] */
    float apps1_travel;     /* APPS1 normalized */
    float apps2_travel;     /* APPS2 normalized */
    float pedal;            /* avg of APPS1/2 after plausibility */
    float pedal_filtered;   /* filtered pedal command */

    /* Torque command [Nm] */
    float torque_cmd;

    /* Status flags */
    bool apps_implaus;      /* APPS implausibility active */
    bool brake_active;      /* BSE above threshold this step */
    bool brake_latched;     /* brake/throttle override latched */

    /* Debug / observability */
    uint8_t apps_implaus_counter;
    float apps_diff;        /* |APPS1 - APPS2| in normalized units */
} vcu_outputs_t;

#endif /* VCU_OUTPUTS_H */
