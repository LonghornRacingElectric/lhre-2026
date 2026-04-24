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

  /* Powertrain inputs from CAN */
  float battery_voltage_v;
  float battery_current_a;
  float battery_soc_pct;
  float motor_speed_rpm;
  bool battery_status_valid;
  bool inverter_speed_valid;

  /* Traction-control inputs from CAN */
  float wheel_speed_fl_rad_s;
  float wheel_speed_fr_rad_s;
  float wheel_speed_rl_rad_s;
  float wheel_speed_rr_rad_s;
  bool wheel_speeds_valid;

  float longitudinal_accel_mps2;
  float lateral_accel_mps2;
  bool accel_valid;

  float tc_longitudinal_adjust;
  float tc_lateral_adjust;
  bool tc_driver_adjust_valid;
} vcu_inputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_INPUTS_H */
