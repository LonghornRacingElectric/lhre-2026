#include "TractionControl.h"
#include "util.h"
#include <math.h>

#define RPM_TO_RAD_S (3.14159265f / 30.0f)

void traction_control_init(traction_control_state_t *state,
                           vcu_parameters_t *params) {
  (void)params;
  state->prev_motor_speed_rpm = 0.0f;
  state->mtte_nm = 0.0f;
  state->pi_integral = 0.0f;
  state->prev_alpha_raw = 0.0f;
  ema_filter_init(&state->accel_filter);
}

void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *state,
                               vcu_parameters_t *params, uint32_t dt_ms) {
  float dt_s = (float)dt_ms * 0.001f;

  // Differentiate motor speed to get angular acceleration at the motor shaft
  float omega     = in->motor_speed_rpm * RPM_TO_RAD_S;
  float omega_prev = state->prev_motor_speed_rpm * RPM_TO_RAD_S;
  state->prev_motor_speed_rpm = in->motor_speed_rpm;

  float alpha_raw = (dt_s > 0.0f) ? (omega - omega_prev) / dt_s
                                  : state->prev_alpha_raw;
  state->prev_alpha_raw = alpha_raw;
  float alpha = ema_filter_evaluate(&state->accel_filter, alpha_raw,
                                    params->traction_control.accel_filter_alpha);
  out->traction_control.accel_rad_s2 = alpha;

  float trim = 0.0f;

  float error = alpha - params->traction_control.alpha_threshold_rad_s2;
  if (error > 0.0f) {
    state->pi_integral += error * dt_s;
    trim = params->traction_control.pi_kp * error
         + params->traction_control.pi_ki * state->pi_integral;
  } else {
    state->pi_integral = 0.0f;
  }

  if(trim > params->traction_control.max_torque_trim_nm) {
    trim = params->traction_control.max_torque_trim_nm;
  }
  if(trim > out->torque_cmd) {
    trim = out->torque_cmd;
  }
  out->traction_control.trim_torque_nm = trim;
  out->torque_cmd = out->torque_power_limited - trim;
}
