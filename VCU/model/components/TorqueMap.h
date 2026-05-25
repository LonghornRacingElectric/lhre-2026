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

  float integral_w_s;
  float previous_measured_power_w;
  bool has_power_history;
} torque_map_state_t;

void torque_map_init(torque_map_state_t *state, const vcu_parameters_t *params);

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         torque_map_state_t *state,
                         const vcu_parameters_t *params, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif // TORQUE_MAP_H
