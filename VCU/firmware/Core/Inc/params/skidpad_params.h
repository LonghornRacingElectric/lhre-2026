#ifndef SKIDPAD_PARAMS_H
#define SKIDPAD_PARAMS_H

#include "default_params.h"

// clang-format off
static const vcu_parameters_t skidpad_params = {
    VCU_DEFAULT_PARAMS,
    .torque_map.pedal_map = {
        /* apps= 0.0 */ 0.00f,
        /* apps= 0.1 */ 0.30f,
        /* apps= 0.2 */ 0.50f,
        /* apps= 0.3 */ 0.70f,
        /* apps= 0.4 */ 0.70f,
        /* apps= 0.5 */ 0.70f,
        /* apps= 0.6 */ 0.70f,
        /* apps= 0.7 */ 0.70f,
        /* apps= 0.8 */ 0.70f,
        /* apps= 0.9 */ 0.70f,
        /* apps= 1.0 */ 0.70f,
    },
    .torque_map.pedal_curve_exponent = 1.0f,
};
// clang-format on

#endif /* SKIDPAD_PARAMS_H */
