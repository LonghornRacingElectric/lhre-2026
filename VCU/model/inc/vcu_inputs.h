#ifndef VCU_INPUTS_H
#define VCU_INPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

enum { TC_FL = 0, TC_FR = 1, TC_RL = 2, TC_RR = 3, TC_WHEEL_COUNT = 4 };
typedef struct {
  float angular_speed_rad_s; /* signed, forward positive */
  uint32_t timestamp_us;     /* estimate validity time mapped to VCU clock */
  uint16_t sequence;
  bool valid;
  bool synchronized;
} tc_wheel_input_t;
typedef struct {
  uint32_t now_us;
  tc_wheel_input_t wheel[TC_WHEEL_COUNT];
  float front_steering_rad[2]; /* calibrated FL/FR road-wheel angles */
  float yaw_rate_rad_s;       /* body +Z left turn */
  uint32_t motion_timestamp_us;
  bool motion_valid;          /* yaw + steering qualified and time aligned */
  bool geometry_valid;        /* calibrated installation / rolling radii */
  bool drive_qualified;       /* model supplies current PRNDL/APPS gate */
  bool braking;               /* model supplies brake request gate */
} tc_inputs_t;

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
  tc_inputs_t traction_control;

  /*  can add more later:
   *  float dc_bus_voltage;
   *  bool inverter_enabled;
   */
} vcu_inputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_INPUTS_H */
