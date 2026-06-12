#include "PowerLimit.h"
#include "Lookup2D.h"
#include "util.h"

#include <stdbool.h>
#include <math.h>

#define PI_F 3.14159265f

void power_limit_init(power_limit_state_t *state, vcu_parameters_t *params) {
  (void)params;
  state->integral = 0.0f;
  // ema_filter_init(&state->power_filter);
}

void power_limit_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         power_limit_state_t *state, vcu_parameters_t *params,
                         uint32_t dt_ms) {
  float dt_s = (float)dt_ms * 0.001f;

  float power = in->battery_voltage_v * in->battery_current_a;
  // power = ema_filter_evaluate(&state->power_filter, power,
  //   params->power_limit.power_filter_alpha);

  // PI error: positive when over the power limit
  float error = power - params->power_limit.power_limit_w;

  state->integral += error * dt_s;
  state->integral = clamp_f(state->integral, 0.0f,
                             params->power_limit.power_limit_trim_integral_max);

  float torque_trim = params->power_limit.power_limit_trim_kp * error
                     + params->power_limit.power_limit_trim_ki * state->integral;

  // Trim is one-sided: only ever reduce torque, never below zero
  torque_trim = clamp_f(torque_trim, 0.0f, out->torque_lookup_output);

  out->torque_power_limited = out->torque_lookup_output - torque_trim;
}
