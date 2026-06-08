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

  // // Estimate effective inertia: J_eff = T / alpha.
  // // When gripping, J_eff ≈ J_vehicle (large — mass of car reflected to motor
  // // shaft). When free-spinning, J_eff ≈ J_drivetrain (small — just motor +
  // // wheel inertia). Only compute when alpha is large enough to be meaningful.
  // bool alpha_valid = alpha > params->traction_control.min_accel_rad_s2;
  // if (alpha_valid) {
  //   float j_eff = in->torque_feedback_nm / alpha;
  //   out->traction_control.effective_inertia_kg_m2 = j_eff;

  //   // Ground reaction torque: the torque the drivetrain actually pushes into
  //   // the road. When the wheel is slipping, this goes to zero or negative
  //   // because the drivetrain inertia is absorbing most of the applied torque.
  //   float t_ground = in->torque_feedback_nm
  //                    - params->traction_control.J_drivetrain_kg_m2 * alpha;
  //   t_ground = fmaxf(t_ground, 0.0f);
  //   out->traction_control.ground_torque_nm = t_ground;

  //   // MTTE: if inertia looks like the vehicle is gripping, the current ground
  //   // torque is achievable — update the estimate upward. If inertia is near
  //   // drivetrain-only (slip), hold the last known good value.
  //   bool gripping = j_eff >= params->traction_control.slip_J_threshold_kg_m2;
  //   if (gripping) {
  //     state->mtte_nm = t_ground;
  //   }
  // }

  // out->traction_control.mtte_nm = state->mtte_nm;

  // if (!params->traction_control.enabled) {
  //   out->torque_cmd = out->torque_power_limited;
  //   out->traction_control.trim_torque_nm = 0.0f;
  //   return;
  // }

  // // Trim torque_cmd down to MTTE when we have a valid estimate
  // float trim = 0.0f;
  // if (alpha_valid && state->mtte_nm > 0.0f) {
  //   trim = fmaxf(out->torque_power_limited - state->mtte_nm, 0.0f);
  // }
  if(trim > out->torque_cmd) {
    trim = out->torque_cmd;
  }
  out->traction_control.trim_torque_nm = trim;
  out->torque_cmd = out->torque_power_limited - trim;
}
