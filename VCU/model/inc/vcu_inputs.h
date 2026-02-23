#ifndef VCU_INPUTS_H
#define VCU_INPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  /* Raw ADC readings */
  float apps1_raw; /* APPS1 ADC (ADC3 CH9)  */
  float apps2_raw; /* APPS2 ADC (ADC3 CH10) */
  float bse1_raw;  /* BSE1 ADC */
  float bse2_raw;  /* BSE2 ADC */

  bool drive_switch;
  bool contactors_closed;

  /*  can add more later:
   *  float dc_bus_voltage;
   *  float motor_speed_rpm;
   *  bool inverter_enabled;
   */
} vcu_inputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_INPUTS_H */
