#ifndef ACCELERATION_PARAMS_H
#define ACCELERATION_PARAMS_H

#include "default_params.h"

static const vcu_parameters_t acceleration_params = {
    VCU_DEFAULT_PARAMS,
    .traction_control = { \
        .enabled = true, \
        .J_drivetrain_kg_m2 = 0.16, \
        .J_vehicle_kg_m2 = 11.0f, \
        .accel_filter_alpha = 0.2f, \
        .min_accel_rad_s2 = 100.0f, \
        .slip_J_threshold_kg_m2 = 0.0f, \
    }, \
    .event_mode = 1, \
};

#endif /* ACCELERATION_PARAMS_H */
