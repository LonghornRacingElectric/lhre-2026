#ifndef ACCELERATION_PARAMS_H
#define ACCELERATION_PARAMS_H

#include <stdbool.h>
#include "default_params.h"

static const vcu_parameters_t acceleration_params = {
    VCU_DEFAULT_PARAMS,
    .traction_control.enable = true,
};

#endif /* ACCELERATION_PARAMS_H */
