#ifndef VCU_PARAMETERS_H
#define VCU_PARAMETERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS 11

typedef struct {
  // Master kill switch for traction control. Keep true for bring-up.
  bool tc_disable;

  // Optional acceleration usage. Leave false until accel axes are validated.
  bool tc_use_accel;
  bool tc_aero_lateral_limit_enable;

  // Vehicle geometry / drivetrain conversion.
  float tc_wheel_radius_m;
  float tc_final_drive_ratio;

  // Driver and setup knobs. Longitudinal adjust changes target slip. Lateral
  // adjust changes how aggressively lateral acceleration tightens the target.
  float tc_longitudinal_adjust; // -1.0 conservative, +1.0 permissive
  float tc_lateral_adjust;      // -1.0 ignores lateral more, +1.0 tighter

  // Slip envelope.
  float tc_base_target_slip;
  float tc_min_target_slip;
  float tc_max_target_slip;
  float tc_slip_hysteresis;
  float tc_lateral_accel_limit_mps2;
  float tc_aero_lateral_accel_gain_per_mps2;
  float tc_lateral_slip_reduction_gain;

  // Enable gates.
  float tc_min_vehicle_speed_mps;
  float tc_min_torque_nm;

  // Sensor plausibility and disagreement thresholds.
  float tc_max_wheel_speed_mps;
  float tc_max_reference_accel_mps2;
  float tc_front_disagreement_mps;
  float tc_rear_disagreement_mps;
  float tc_motor_rear_disagreement_mps;

  // Signal conditioning.
  float tc_speed_lpf_time_constant_s;
  float tc_slip_lpf_time_constant_s;
  float tc_feedback_lpf_time_constant_s;
  float tc_reference_accel_blend;

  // Slip controller gains.
  float tc_kp_nm_per_slip;
  float tc_ki_nm_per_slip_s;
  float tc_kd_nm_per_slip_rate;
  float tc_driven_accel_gain_nm_per_mps2;
  float tc_integral_limit_nm;
  float tc_max_torque_reduction_nm;

  // Output shaping. Cut fast, recover slowly.
  float tc_cut_slew_nm_per_s;
  float tc_recovery_slew_nm_per_s;
} vcu_traction_control_parameters_t;

typedef struct {
  float brake_enable_threshold;
  uint16_t buzzer_duration_ms;

  struct {
    float apps1_min_adc_v;
    float apps1_max_adc_v;
    float apps2_min_adc_v;
    float apps2_max_adc_v;
    float min_travel_threshold;         // the amount of travel required to be
                                        // considered pressed for implausibility
    float max_travel_restore_threshold; // the amount of travel required to
                                        // restore APPS from implausible state
    float max_allowable_diff; // maximum difference allowed between both APPS
                              // sensors

    float max_travel_deadzone;         // the amount of travel required to be
                                       // considered fully pressed (100%)
    float min_travel_deadzone;         // the amount of travel required to be
                                       // considered fully released (0%)
    uint16_t implaus_debounce_time_ms; // how long the APPS must be implausible
                                       // before torque must be cut
    float pedal_ema_alpha; // EMA filter alpha parameter mapping [0.0 - 1.0]
  } apps;

  struct {
    // Software Hystersis for BSE
    float bse_off_psi; // pressure at which brake is considered off
    float bse_on_psi;  // pressure at which brake is considered on

    float bse1_adc_at_min_psi_v; // ADC reading at lowest BSE1 value
    float bse1_adc_at_max_psi_v; // ADC reading at highest BSE1 value
    float bse2_adc_at_min_psi_v; // ADC reading at lowest BSE2 value
    float bse2_adc_at_max_psi_v; // ADC reading at highest BSE2 value

    float bse_max_psi;                 // maximum pressure reading possible
    float max_pedal_while_braking;     // maximum pedal allowed while braking
    float max_pedal_restore_threshold; // maximum pedal allowed to restore BSE

    float min_psi_deadzone; // starting psi range clamping strictly to 0.0
    float max_psi_deadzone; // top psi threshold tracking out of bounds

    float
        bse_ema_alpha; // smoothing factor for internal filters mapping bse EMA

    float brake_light_min_pct; // minimum brake light percentage
    float brake_light_max_pct; // maximum brake light percentage
  } bse;

  struct {
    float max_torque_nm; // maximum torque request allowed in Nm
    float pedal_exponential_factor;

    float power_limit_w;
    float current_limit_a;
    float hard_current_cut_a;
    float hard_power_cut_w;
    float ocv_cell_count;
    float ocv_lpf_time_constant_s;
    float current_lpf_time_constant_s;
    float measured_power_lpf_time_constant_s;

    float power_limit_min_rpm;
    float power_limit_trim_limit_nm;
    float power_limit_kp;
    float power_limit_ki;
    float power_limit_kd;

    // Motor efficiency map as rpm/efficiency pairs.
    float power_limit_motor_efficiency_rpm[VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS];
    float power_limit_motor_efficiency[VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS];
  } torque_map;

  vcu_traction_control_parameters_t traction_control;
} vcu_parameters_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_PARAMETERS_H */
