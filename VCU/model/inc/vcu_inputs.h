#ifndef VCU_INPUTS_H
#define VCU_INPUTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    /* Raw ADC readings */
    uint16_t apps1_raw;   /* APPS1 ADC (ADC3 CH9)  */
    uint16_t apps2_raw;   /* APPS2 ADC (ADC3 CH10) */
    uint16_t bse_raw;     /* BSE  ADC (ADC2 CH3)   */

    /*  can add more later:
     *  float dc_bus_voltage;
     *  float motor_speed_rpm;
     *  bool inverter_enabled;
     */
} vcu_inputs_t;

#endif /* VCU_INPUTS_H */
