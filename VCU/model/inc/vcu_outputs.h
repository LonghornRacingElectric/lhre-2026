#ifndef VCU_OUTPUTS_H
#define VCU_OUTPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  /* Normalized pedal travel */
  float apps1_travel;       // travel percentage of apps1
  float apps2_travel;       // travel percentage of apps2
  float accel_pedal_travel; // middle of apps1 and apps2 in percent

  /* Torque command */
  float torque_lookup_output; // raw torque from the lookup table (Nm)
  float torque_derated;       // torque from lookup table after derates are applied (Nm)
  float torque_power_limited; // torque after power limit trim loop (Nm)
  float torque_tc_limited;    // torque after traction control trim loop (Nm)
  float torque_cmd;           // final torque command (Nm)

  float derate_factor_cell_voltage;
  float derate_factor_cell_temp;

  float open_circuit_cell_voltage;
  float soe_pct;

  bool inverter_enable;

  /* Status flags */
  bool brake_pressed;    // true if brake is pressed
  float brake_light_pct; // percentage of brake light to apply

  uint8_t prndl_state; // current state of the PRNDL machine

  /* Buzzer */
  bool buzzer_active; // true if buzzer is active

  /* BSE pressure estimate (psi) */
  float bse1_psi;         // brake pressure for sensor 1
  float bse2_psi;         // brake pressure for sensor 2
  float bse_psi;          // average brake pressure
  float bse_psi_filtered; // filtered brake pressure

  /* Cooling */
  bool pumps_on;
  float rad_fans_pct;
  float bat_fans_pct;

  struct {
    bool apps1_under_range;
    bool apps1_over_range;
    bool apps2_under_range;
    bool apps2_over_range;
    bool apps_implaus;
    bool apps_any_fault;

    bool bse1_under_range;
    bool bse1_over_range;
    bool bse2_under_range;
    bool bse2_over_range;

    bool brake_latched;
    bool brake_any_fault;

    bool any_fault;
  } faults;

  /* Debug data */
  struct {
    uint32_t apps_implaus_ms; // how long the APPS has been implausible
    float apps_diff;          // difference between apps1 and apps2
    float apps1_travel;
    float apps2_travel;
  } debug;

} vcu_outputs_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_OUTPUTS_H */
