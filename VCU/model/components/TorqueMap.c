#include "TorqueMap.h"
#include "util.h"
#include <math.h>
#include <string.h>

static float lpf_alpha_from_tau(float dt_s, float tau_s) {
  if (tau_s <= 0.0f) {
    return 1.0f;
  }
  return clamp_f(dt_s / (tau_s + dt_s), 0.0f, 1.0f);
}

static float lpf_step(float previous, float input, float alpha) {
  return alpha * input + (1.0f - alpha) * previous;
}

static bool torque_map_efficiency_map_is_valid(const float map[11]) {
  for (int i = 0; i < 11; i++) {
    if (!isfinite(map[i]) || map[i] <= 0.0f || map[i] > 1.0f) {
      return false;
    }
  }
  return true;
}

static bool torque_map_params_are_valid(const vcu_parameters_t *params) {
  return isfinite(params->torque_map.max_torque_nm) &&
         params->torque_map.max_torque_nm > 0.0f &&
         isfinite(params->torque_map.pedal_exponential_factor) &&
         params->torque_map.pedal_exponential_factor >= 0.0f &&
         isfinite(params->torque_map.power_limit_w) &&
         params->torque_map.power_limit_w > 0.0f &&
         isfinite(params->torque_map.current_limit_a) &&
         params->torque_map.current_limit_a > 0.0f &&
         isfinite(params->torque_map.hard_current_cut_a) &&
         params->torque_map.hard_current_cut_a >=
             params->torque_map.current_limit_a &&
         isfinite(params->torque_map.hard_power_cut_w) &&
         params->torque_map.hard_power_cut_w >= params->torque_map.power_limit_w &&
         isfinite(params->torque_map.ocv_cell_count) &&
         params->torque_map.ocv_cell_count > 0.0f &&
         isfinite(params->torque_map.ocv_lpf_time_constant_s) &&
         params->torque_map.ocv_lpf_time_constant_s >= 0.0f &&
         isfinite(params->torque_map.current_lpf_time_constant_s) &&
         params->torque_map.current_lpf_time_constant_s >= 0.0f &&
         isfinite(params->torque_map.measured_power_lpf_time_constant_s) &&
         params->torque_map.measured_power_lpf_time_constant_s >= 0.0f &&
         isfinite(params->torque_map.power_limit_min_rpm) &&
         params->torque_map.power_limit_min_rpm > 0.0f &&
         isfinite(params->torque_map.power_limit_trim_limit_nm) &&
         params->torque_map.power_limit_trim_limit_nm >= 0.0f &&
         isfinite(params->torque_map.power_limit_kp) &&
         params->torque_map.power_limit_kp >= 0.0f &&
         isfinite(params->torque_map.power_limit_ki) &&
         params->torque_map.power_limit_ki >= 0.0f &&
         isfinite(params->torque_map.power_limit_kd) &&
         params->torque_map.power_limit_kd >= 0.0f &&
         torque_map_efficiency_map_is_valid(
             params->torque_map.power_limit_motor_efficiency);
}

static float motor_efficiency_at_rpm(const vcu_parameters_t *params,
                                     float motor_rpm) {
  const float *map = params->torque_map.power_limit_motor_efficiency;

  float rpm = clamp_f(motor_rpm, 0.0f, 5500.0f);
  float position = rpm / 550.0f;
  int index = (int)position;
  if (index >= 10) {
    return clamp_f(map[10], 0.50f, 1.00f);
  }

  float pct = position - (float)index;
  return clamp_f(linear_interp(map[index], map[index + 1], pct), 0.50f,
                 1.00f);
}

static float shape_pedal_request(float pedal_fraction,
                                 float exponential_factor) {
  float clamped_pedal = clamp_f(pedal_fraction, 0.0f, 1.0f);
  if (fabsf(exponential_factor) < 1.0e-6f) {
    return clamped_pedal;
  }

  float denominator = expf(exponential_factor) - 1.0f;
  if (fabsf(denominator) < 1.0e-6f) {
    return clamped_pedal;
  }

  float numerator = expf(exponential_factor * clamped_pedal) - 1.0f;
  return clamp_f(numerator / denominator, 0.0f, 1.0f);
}

void torque_map_init(torque_map_state_t *state) {
  memset(state, 0, sizeof(torque_map_state_t));
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         torque_map_state_t *state, vcu_parameters_t *params,
                         uint32_t dt_ms) {
  const float dt_s = (float)dt_ms / 1000.0f;
  const float two_pi = 6.28318530718f;

  if (!torque_map_params_are_valid(params)) {
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }

  float max_torque_nm = params->torque_map.max_torque_nm;
  float mapped_pedal_torque =
      clamp_f(linear_interp(0.0f, max_torque_nm, out->accel_pedal_travel),
              0.0f, max_torque_nm);

  float pedal_fraction = 0.0f;
  if (max_torque_nm > 1.0e-6f) {
    pedal_fraction = mapped_pedal_torque / max_torque_nm;
  }
  pedal_fraction = shape_pedal_request(
      pedal_fraction, params->torque_map.pedal_exponential_factor);

  float requested_torque_nm = pedal_fraction * max_torque_nm;
  float derated_torque_limit_nm = max_torque_nm;

  if (!in->battery_status_valid || !in->inverter_speed_valid) {
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }

  float ocv_tau_s = params->torque_map.ocv_lpf_time_constant_s;
  float current_tau_s = params->torque_map.current_lpf_time_constant_s;
  float measured_power_tau_s =
      params->torque_map.measured_power_lpf_time_constant_s;

  if (!state->current_initialized) {
    state->filtered_current_a = in->battery_current_a;
    state->current_initialized = true;
  } else {
    state->filtered_current_a =
        lpf_step(state->filtered_current_a, in->battery_current_a,
                 lpf_alpha_from_tau(dt_s, current_tau_s));
  }

  if (!state->ocv_initialized) {
    state->ocv_estimate_v = in->battery_voltage_v;
    state->ocv_initialized = true;
  } else if (fabsf(in->battery_current_a) < 1.0f) {
    state->ocv_estimate_v =
        lpf_step(state->ocv_estimate_v, in->battery_voltage_v,
                 lpf_alpha_from_tau(dt_s, ocv_tau_s));
  }

  float power_limit_w = params->torque_map.power_limit_w;
  float current_limit_a = params->torque_map.current_limit_a;
  float hard_current_cut_a = params->torque_map.hard_current_cut_a;
  float hard_power_cut_w = params->torque_map.hard_power_cut_w;
  float ocv_cell_count = params->torque_map.ocv_cell_count;
  float min_rpm = params->torque_map.power_limit_min_rpm;
  float trim_limit_nm = params->torque_map.power_limit_trim_limit_nm;

  float discharge_voltage_v = state->ocv_estimate_v;
  if (in->battery_current_a > 2.0f) {
    discharge_voltage_v = fminf(state->ocv_estimate_v, in->battery_voltage_v);
  }
  float current_based_power_limit_w = discharge_voltage_v * current_limit_a;
  float active_power_limit_w =
      fmaxf(fminf(power_limit_w, current_based_power_limit_w), 0.0f);

  float voltage_derate =
      clamp_f((state->ocv_estimate_v / ocv_cell_count - 3.3f) / 0.1f, 0.0f,
              1.0f);
  derated_torque_limit_nm *= voltage_derate;
  requested_torque_nm =
      clamp_f(requested_torque_nm, 0.0f, derated_torque_limit_nm);

  float measured_power_w = in->battery_voltage_v * in->battery_current_a;
  if (!state->power_initialized) {
    state->filtered_power_w = measured_power_w;
    state->power_initialized = true;
  } else {
    state->filtered_power_w =
        lpf_step(state->filtered_power_w, measured_power_w,
                 lpf_alpha_from_tau(dt_s, measured_power_tau_s));
  }

  float measured_power_rate_w_s = 0.0f;
  if (dt_s > 0.0f && state->has_power_history) {
    measured_power_rate_w_s =
        (measured_power_w - state->previous_measured_power_w) / dt_s;
  }
  state->previous_measured_power_w = measured_power_w;
  state->has_power_history = true;

  out->debug.ocv_estimate_v = state->ocv_estimate_v;
  out->debug.active_power_limit_w = active_power_limit_w;
  out->debug.measured_power_w = measured_power_w;

  float motor_rpm = fabsf(in->motor_speed_rpm);
  float efficiency = motor_efficiency_at_rpm(params, motor_rpm);
  out->debug.motor_efficiency = efficiency;

  float limited_rpm = fmaxf(motor_rpm, fmaxf(min_rpm, 1.0f));
  float motor_angular_velocity_rad_s = limited_rpm * two_pi / 60.0f;
  float mechanical_power_limit_w = active_power_limit_w * efficiency;
  float feedforward_torque_nm = 0.0f;
  if (motor_angular_velocity_rad_s > 0.0f) {
    feedforward_torque_nm =
        mechanical_power_limit_w / motor_angular_velocity_rad_s;
  }
  feedforward_torque_nm =
      clamp_f(feedforward_torque_nm, 0.0f, derated_torque_limit_nm);

  if (state->filtered_current_a > hard_current_cut_a ||
      measured_power_w > hard_power_cut_w) {
    state->integral_w_s = 0.0f;
    out->torque_cmd = 0.0f;
    out->faults.current_safety_cut =
        state->filtered_current_a > hard_current_cut_a;
    out->faults.power_safety_cut = measured_power_w > hard_power_cut_w;
    return;
  }

  float power_error_w = active_power_limit_w - state->filtered_power_w;
  if (out->accel_pedal_travel < 0.01f || derated_torque_limit_nm <= 0.0f) {
    state->integral_w_s = 0.0f;
  }

  float proportional_nm = params->torque_map.power_limit_kp * power_error_w;
  float candidate_integral_w_s = state->integral_w_s;
  if (dt_s > 0.0f) {
    candidate_integral_w_s += power_error_w * dt_s;
  }
  float derivative_nm = -params->torque_map.power_limit_kd *
                        fmaxf(measured_power_rate_w_s, 0.0f);

  float integral_term_nm =
      params->torque_map.power_limit_ki * candidate_integral_w_s;
  float unsaturated_trim_nm =
      proportional_nm + integral_term_nm + derivative_nm;

  bool saturating_high =
      unsaturated_trim_nm > trim_limit_nm && power_error_w > 0.0f;
  bool saturating_low =
      unsaturated_trim_nm < -trim_limit_nm && power_error_w < 0.0f;
  if (!(saturating_high || saturating_low)) {
    state->integral_w_s = candidate_integral_w_s;
  }

  float integral_trim_nm = params->torque_map.power_limit_ki *
                           state->integral_w_s;
  float trim_torque_nm = proportional_nm + integral_trim_nm + derivative_nm;
  trim_torque_nm = clamp_f(trim_torque_nm, -trim_limit_nm, trim_limit_nm);

  float available_torque_nm = feedforward_torque_nm + trim_torque_nm;
  available_torque_nm =
      clamp_f(available_torque_nm, 0.0f, derated_torque_limit_nm);

  out->torque_cmd =
      clamp_f(requested_torque_nm, 0.0f, available_torque_nm);
  out->debug.power_limit_feedback_p_nm = proportional_nm;
  out->debug.power_limit_feedback_i_nm = integral_trim_nm;
  out->debug.power_limit_feedback_d_nm = derivative_nm;
  out->debug.power_limit_feedback_torque_nm = trim_torque_nm;
}
