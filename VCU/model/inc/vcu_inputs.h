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

  float motor_speed_rpm;
  float torque_feedback_nm;
  float min_cell_voltage_v;
  float max_cell_voltage_v;

  float battery_voltage_v;
  float battery_current_a;
  float battery_soc_pct;
  float min_cell_temp_c;
  float max_cell_temp_c;

  bool motor_speed_valid;
  bool inverter_voltage_valid;
  bool inverter_current_valid;
  bool battery_pack_status_valid;

  /*  can add more later:
   *  float dc_bus_voltage;
   *  bool inverter_enabled;
   */
} vcu_inputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_INPUTS_H */
