#ifndef TORQUE_MAP_H
#define TORQUE_MAP_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include "vcu_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float ocv_estimate_v;
  bool ocv_initialized;

  float filtered_current_a;
  bool current_initialized;

  float filtered_power_w;
  bool power_initialized;

  float integral_w_s;
  float previous_measured_power_w;
  bool has_power_history;
} torque_map_state_t;

void torque_map_init(torque_map_state_t *state);

/**
 * @brief Calculates torque request given a set of parameters and inputs
 * @note the torque command is in Nm
 */
void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         torque_map_state_t *state, vcu_parameters_t *params,
                         uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // TORQUE_MAP_H
