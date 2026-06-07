#include "TractionControl.h"

void traction_control_init(traction_control_state_t *state,
                           vcu_parameters_t *params) {
  (void)state;
  (void)params;
}

void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *state,
                               vcu_parameters_t *params, uint32_t dt_ms) {
  (void)in;
  (void)state;
  (void)params;
  (void)dt_ms;

  out->torque_cmd = out->torque_power_limited;
}
