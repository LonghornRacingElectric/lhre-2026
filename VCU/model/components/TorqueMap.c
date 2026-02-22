#include "TorqueMap.h"
#include "util.h"

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         vcu_parameters_t *params, uint32_t dt_ms) {
  float torque = linear_interp(0.0f, params->torque_map.max_torque_nm,
                               out->pedal_filtered);
  out->torque_cmd = torque;
}
