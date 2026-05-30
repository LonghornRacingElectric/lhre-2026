#include "TractionControl.h"
#include "util.h"

// Below this front wheel speed, slip ratio is undefined — TC is inactive
#define TC_MIN_FRONT_SPEED_RADS 1.0f

void traction_control_init(traction_control_state_t *state,
                           vcu_parameters_t *params) {
  (void)params;
  state->integral = 0.0f;
}

void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *state,
                               vcu_parameters_t *params, uint32_t dt_ms) {
  out->torque_tc_limited = out->torque_cmd;

  float front_speed = (in->wheel_speed_fl_rads + in->wheel_speed_fr_rads) * 0.5f;
  float rear_speed  = (in->wheel_speed_rl_rads + in->wheel_speed_rr_rads) * 0.5f;

  if (!params->traction_control.enable ||
      front_speed < TC_MIN_FRONT_SPEED_RADS) {
    state->integral = 0.0f;
    return;
  }

  float dt_s = (float)dt_ms * 0.001f;

  float slip = (rear_speed - front_speed) / front_speed;

  // PI error: positive when slipping more than target
  float error = slip - params->traction_control.target_slip_ratio;

  state->integral += error * dt_s;
  state->integral =
      clamp_f(state->integral, 0.0f,
              params->traction_control.tc_trim_integral_max);

  float torque_trim = params->traction_control.tc_trim_kp * error +
                      params->traction_control.tc_trim_ki * state->integral;

  // Trim is one-sided: only ever reduce torque, never below zero
  torque_trim = clamp_f(torque_trim, 0.0f, out->torque_cmd);

  out->torque_tc_limited = out->torque_cmd - torque_trim;
  out->torque_cmd = out->torque_tc_limited;
}
