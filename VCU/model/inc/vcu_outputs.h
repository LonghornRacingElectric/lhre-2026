#ifndef VCU_OUTPUTS_H
#define VCU_OUTPUTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    /* Normalized pedal travel */
    float apps1_travel; 
    float apps2_travel;
    float pedal;
    float pedal_filtered;

    /* Torque command */
    float torque_cmd;

    /* Status flags */
    bool apps_implaus;
    bool brake_active;
    bool brake_latched;

    /* Debug data */
    uint8_t apps_implaus_counter;
    float apps_diff;

    /* BSE pressure estimate (psi) */
    float bse_psi;

} vcu_outputs_t;

#endif /* VCU_OUTPUTS_H */
