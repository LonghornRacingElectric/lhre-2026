#ifndef DEFAULT_PARAMS_H
#define DEFAULT_PARAMS_H

#include "vcu_model/inc/vcu_parameters.h"

#ifndef ADC_MAX_VAL
#define ADC_MAX_VAL ((1u << 12) - 1u)
#endif
#ifndef ADC_BSE_SCALE_V
#define ADC_BSE_SCALE_V 3.2837f
#endif

// clang-format off
#define VCU_DEFAULT_PARAMS \
    .apps = { \
        .apps1_min_adc_v          = 1.750f, \
        .apps1_max_adc_v          = 1.520f, \
        .apps2_min_adc_v          = 0.190f, \
        .apps2_max_adc_v          = -0.020f, \
        .implaus_debounce_time_ms = 100u, \
        .max_allowable_diff       = 0.15f, \
        .min_travel_deadzone      = 0.09f, \
        .max_travel_deadzone      = 0.88f, \
        .pedal_ema_alpha          = 0.35f, \
    }, \
    .bse = { \
        .bse_off_psi                 = 250.0f, \
        .bse_on_psi                  = 300.0f, \
        .bse_brake_light_psi         = 30.0f, \
        .bse1_adc_at_min_psi_v       = ((397.0f  * ADC_BSE_SCALE_V) / ADC_MAX_VAL), \
        .bse1_adc_at_max_psi_v       = ((2267.0f * ADC_BSE_SCALE_V) / ADC_MAX_VAL), \
        .bse2_adc_at_min_psi_v       = ((397.0f  * ADC_BSE_SCALE_V) / ADC_MAX_VAL), \
        .bse2_adc_at_max_psi_v       = ((2017.0f * ADC_BSE_SCALE_V) / ADC_MAX_VAL), \
        .bse_max_psi                 = 3000.0f, \
        .max_pedal_while_braking     = 0.25f, \
        .max_pedal_restore_threshold = 0.05f, \
        .min_psi_deadzone            = 0.4f, \
        .max_psi_deadzone            = 1.0f, \
        .bse_ema_alpha               = 0.10f, \
        .brake_light_min_pct         = 0.0f, \
        .brake_light_max_pct         = 0.30f, \
    }, \
    .torque_map = { \
        .power_limit_torque = { \
          /* rpm=    0 */ 210.0f, \
          /* rpm=  600 */ 210.0f, \
          /* rpm= 1200 */ 210.0f, \
          /* rpm= 1800 */ 210.0f, \
          /* rpm= 2400 */ 210.0f, \
          /* rpm= 3000 */ 192.0f, \
          /* rpm= 3600 */ 166.0f, \
          /* rpm= 4200 */ 145.0f, \
          /* rpm= 4800 */ 129.0f, \
          /* rpm= 5400 */ 115.0f, \
          /* rpm= 6000 */ 103.0f, \
        }, \
        .pedal_map = { \
          /* apps= 0.0 */ 0.00f, \
          /* apps= 0.1 */ 0.10f, \
          /* apps= 0.2 */ 0.20f, \
          /* apps= 0.3 */ 0.30f, \
          /* apps= 0.4 */ 0.40f, \
          /* apps= 0.5 */ 0.50f, \
          /* apps= 0.6 */ 0.60f, \
          /* apps= 0.7 */ 0.70f, \
          /* apps= 0.8 */ 0.80f, \
          /* apps= 0.9 */ 0.90f, \
          /* apps= 1.0 */ 1.00f, \
        }, \
        .pedal_curve_exponent    = 2.0f, \
        .low_cell_derate_start_v = 3.2f, \
        .low_cell_cutoff_v       = 2.8f, \
    }, \
    .power_limit = { \
        .power_limit_w                 = 70000.0f, \
        .power_limit_trim_kp           = 0.006f, \
        .power_limit_trim_ki           = 0.6f, \
        .power_limit_trim_integral_max = 20000.0f, \
    }, \
    .battery = { \
        .cell_voltage_ema_alpha = 0.005f, \
        .soe_from_cell_voltage  = { \
            0.0f,  1.0f,  3.0f,  6.0f, 10.0f, \
            17.0f, 28.0f, 46.0f, 68.0f, 86.0f, 100.0f \
        }, \
        .min_soe_cell_voltage = 2.8f, \
        .max_soe_cell_voltage = 4.2f, \
    }, \
    .regen_linelock = { \
        .disable                              = false, \
        .pressure_only_test_mode             = false, \
        .dc_bus_current_regen_is_negative    = true, \
        .rear_pressure_zero_torque_psi       = 0.0f, \
        .rear_pressure_reference_psi         = 500.0f, \
        .rear_pressure_min_engage_psi        = 10.0f, \
        .regen_torque_at_reference_pressure_nm = 5.0f, \
        .absolute_regen_torque_cap_nm        = 230.0f, \
        .pedal_torque_release_threshold_nm   = 100.0f, \
        .linelock_close_delay_ms             = 200u, \
        .pack_current_limit_a                = 45.0f, \
        .hard_cut_margin_pct                 = 0.20f, \
        .hard_cut_reset_pressure_psi         = 100.0f, \
        .pack_terminal_voltage_limit_v       = 546.0f, \
        .pack_resistance_ohm                 = 0.442f, \
        .pack_series_cell_count              = 130.0f, \
        .dynamic_voltage_reserve_v           = 6.0f, \
        .pack_ocv_enable_v                   = 520.11f, \
        .pack_ocv_disable_hysteresis_v       = 2.0f, \
        .max_cell_voltage_regen_disable_v    = 4.05f, \
        .min_cell_temp_c                     = 10.0f, \
        .max_cell_temp_c                     = 55.0f, \
        .min_motor_speed_rpm                 = 219.49f, \
    }, \
    .buzzer_duration_ms     = 1200u, \
    .brake_enable_threshold = 0.1f
// clang-format on

#endif /* DEFAULT_PARAMS_H */
