#include "Cooling.h"

void cooling_init(cooling_state_t *state, const vcu_parameters_t *params) {
  // TODO: implement state once there is a state
}

void cooling_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                      cooling_state_t *state, const vcu_parameters_t *params,
                      uint32_t dt_ms) {
  // For now, turn on pumps and fans always
  out->pumps_on = true;
  out->rad_fans_pct = 0.50f;
  out->bat_fans_pct = 0.50f;
}