#ifndef VCU_PARAMETERS_H
#define VCU_PARAMETERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Lookup1D.h"
#include "Lookup2D.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {

  uint8_t event_mode; // 0 = unassigned, 1 = acceleration, 2 = skidpad, 3 = autocross, 4 = endurance

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
    float bse_brake_light_psi; // pressure threshold to enable brake light
  } bse;

  struct {
    float power_limit_torque[LOOKUP1D_POINTS];
    float pedal_map[LOOKUP1D_POINTS];
    float pedal_curve_exponent;
    float low_cell_derate_start_v;
    float low_cell_cutoff_v;
  } torque_map;

  struct {
    float power_limit_w;
    float power_limit_trim_kp;
    float power_limit_trim_ki;
    float power_limit_trim_integral_max;
  } power_limit;

  struct {
    float cell_voltage_ema_alpha;
    float soe_from_cell_voltage[LOOKUP1D_POINTS];
    float min_soe_cell_voltage;
    float max_soe_cell_voltage;
  } battery;

  struct {
    bool disable;
    bool pressure_only_test_mode;
    bool dc_bus_current_regen_is_negative;

    float rear_pressure_zero_torque_psi;
    float rear_pressure_reference_psi;
    float rear_pressure_min_engage_psi;
    float regen_torque_at_reference_pressure_nm;
    float absolute_regen_torque_cap_nm;
    float pedal_torque_open_pulse_threshold_nm;
    float pedal_torque_open_pulse_cancel_threshold_nm;
    uint32_t linelock_open_pulse_ms;
    uint32_t linelock_close_delay_ms;

    float pack_current_limit_a;
    float hard_cut_margin_pct;
    float hard_cut_reset_pressure_psi;

    float pack_terminal_voltage_limit_v;
    float pack_resistance_ohm;
    float pack_series_cell_count;
    float dynamic_voltage_reserve_v;
    float pack_ocv_enable_v;
    float pack_ocv_disable_hysteresis_v;
    float max_cell_voltage_regen_disable_v;

    float min_cell_temp_c;
    float max_cell_temp_c;
    float min_motor_speed_rpm;
  } regen_linelock;

  struct {
    bool enabled;
    float J_drivetrain_kg_m2;  // rotational inertia of motor+drivetrain at motor shaft
    float J_vehicle_kg_m2;     // effective inertia when gripping (mass reflected to motor shaft)
    float accel_filter_alpha;  // EMA alpha for RPM derivative smoothing [0,1]
    float min_accel_rad_s2;    // minimum |alpha| to trust the inertia estimate
    float slip_J_threshold_kg_m2; // J_eff below this → slip detected
  } traction_control;
} vcu_parameters_t;

#ifdef __cplusplus
}
#endif

#endif /* VCU_PARAMETERS_H */
