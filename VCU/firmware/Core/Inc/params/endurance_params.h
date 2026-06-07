#ifndef ENDURANCE_PARAMS_H
#define ENDURANCE_PARAMS_H

#include "default_params.h"

static const vcu_parameters_t endurance_params = {
    VCU_DEFAULT_PARAMS,
    .regen_linelock = { \
        .disable                             = false, \
        .pressure_only_test_mode             = false, \
        .dc_bus_current_regen_is_negative    = true, \
        .rear_pressure_zero_torque_psi       = 0.0f, \
        .rear_pressure_reference_psi         = 500.0f, \
        .rear_pressure_min_engage_psi        = 10.0f, \
        .regen_torque_at_reference_pressure_nm = 67.0f, \
        .absolute_regen_torque_cap_nm        = 230.0f, \
        .pedal_torque_open_pulse_threshold_nm = 70.0f, \
        .pedal_torque_open_pulse_cancel_threshold_nm = 50.0f, \
        .linelock_open_pulse_ms              = 250u, \
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
    .event_mode = 4, \
};

#endif /* ENDURANCE_PARAMS_H */
