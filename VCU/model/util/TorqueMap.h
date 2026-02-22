#ifndef TORQUE_MAP_H
#define TORQUE_MAP_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculates torque request given a set of parameters and inputs
 * @note the torque command is in Nm
 */
void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         vcu_parameters_t *params, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // TORQUE_MAP_H
