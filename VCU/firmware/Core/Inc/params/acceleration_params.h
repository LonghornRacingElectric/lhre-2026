#ifndef ACCELERATION_PARAMS_H
#define ACCELERATION_PARAMS_H

#include "default_params.h"

static const vcu_parameters_t acceleration_params = {
    VCU_DEFAULT_PARAMS,
    .traction_control = { \
        .enabled = false, \
        .accel_filter_alpha = 0.6f, \
        .alpha_threshold_rad_s2 = 30.0f, \
        .pi_kp = 0.5f, \
        .pi_ki = 5.0f, \
        .max_torque_trim_nm = 50.0f, \
    }, \
    .torque_map = { \
        .power_limit_torque = { \
          /* rpm=    0 */ 220.0f, \
          /* rpm=  600 */ 220.0f, \
          /* rpm= 1200 */ 220.0f, \
          /* rpm= 1800 */ 220.0f, \
          /* rpm= 2400 */ 220.0f, \
          /* rpm= 3000 */ 200.0f, \
          /* rpm= 3600 */ 175.0f, \
          /* rpm= 4200 */ 155.0f, \
          /* rpm= 4800 */ 135.0f, \
          /* rpm= 5400 */ 115.0f, \
          /* rpm= 6000 */ 105.0f, \
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
        .pedal_curve_exponent    = 1.5f, \
        .low_cell_derate_start_v = 3.2f, \
        .low_cell_cutoff_v       = 2.8f, \
    }, \
    .event_mode = 1, \
};

#endif /* ACCELERATION_PARAMS_H */
