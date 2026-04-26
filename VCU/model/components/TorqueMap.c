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

static bool torque_map_efficiency_map_is_valid(const vcu_parameters_t *params) {
  float previous_rpm = -1.0f;
  for (int i = 0; i < VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS; i++) {
    float rpm = params->torque_map.power_limit_motor_efficiency_rpm[i];
    float efficiency = params->torque_map.power_limit_motor_efficiency[i];
    if (!isfinite(rpm) || rpm < 0.0f || rpm <= previous_rpm) {
      return false;
    }
    if (!isfinite(efficiency) || efficiency <= 0.0f || efficiency > 1.0f) {
      return false;
    }
    previous_rpm = rpm;
  }
  return true;
}

static bool torque_map_params_are_valid(const vcu_parameters_t *params) {
  bool launch_params_valid = true;
  if (!params->torque_map.launch_mode_disable) {
    launch_params_valid =
        isfinite(params->torque_map.launch_enter_rpm) &&
        params->torque_map.launch_enter_rpm >= 0.0f &&
        isfinite(params->torque_map.launch_exit_rpm) &&
        params->torque_map.launch_exit_rpm >
            params->torque_map.launch_enter_rpm &&
        isfinite(params->torque_map.launch_pedal_min) &&
        params->torque_map.launch_pedal_min >= 0.0f &&
        params->torque_map.launch_pedal_min <= 1.0f &&
        isfinite(params->torque_map.launch_pedal_max) &&
        params->torque_map.launch_pedal_max >=
            params->torque_map.launch_pedal_min &&
        params->torque_map.launch_pedal_max <= 1.0f &&
        isfinite(params->torque_map.launch_brake_min_psi) &&
        params->torque_map.launch_brake_min_psi >= 0.0f &&
        isfinite(params->torque_map.launch_preload_torque_nm) &&
        params->torque_map.launch_preload_torque_nm >= 0.0f &&
        isfinite(params->torque_map.launch_preload_ramp_rate_nm_per_s) &&
        params->torque_map.launch_preload_ramp_rate_nm_per_s > 0.0f &&
        isfinite(params->torque_map.launch_release_ramp_rate_nm_per_s) &&
        params->torque_map.launch_release_ramp_rate_nm_per_s > 0.0f;
  }

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
         torque_map_efficiency_map_is_valid(params) && launch_params_valid;
}

static float motor_efficiency_at_rpm(const vcu_parameters_t *params,
                                     float motor_rpm) {
  const float *rpm_map = params->torque_map.power_limit_motor_efficiency_rpm;
  const float *efficiency_map = params->torque_map.power_limit_motor_efficiency;
  const int last_index = VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS - 1;

  float rpm = fmaxf(motor_rpm, 0.0f);
  if (rpm <= rpm_map[0]) {
    return clamp_f(efficiency_map[0], 0.50f, 1.00f);
  }

  for (int i = 0; i < last_index; i++) {
    if (rpm <= rpm_map[i + 1]) {
      float pct = (rpm - rpm_map[i]) / (rpm_map[i + 1] - rpm_map[i]);
      return clamp_f(linear_interp(efficiency_map[i], efficiency_map[i + 1],
                                   pct),
                     0.50f, 1.00f);
    }
  }

  return clamp_f(efficiency_map[last_index], 0.50f, 1.00f);
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

static float torque_from_power(float power_w, float motor_rpm,
                               float min_rpm) {
  const float two_pi = 6.28318530718f;
  float limited_rpm = fmaxf(fabsf(motor_rpm), fmaxf(min_rpm, 1.0f));
  float motor_angular_velocity_rad_s = limited_rpm * two_pi / 60.0f;
  if (motor_angular_velocity_rad_s <= 0.0f) {
    return 0.0f;
  }

  return power_w / motor_angular_velocity_rad_s;
}

static void launch_mode_reset(torque_map_state_t *state) {
  state->launch_state = TORQUE_MAP_LAUNCH_STATE_INACTIVE;
  state->launch_output_torque_nm = 0.0f;
}

static float rate_limit_rising(float previous, float target, float rate,
                               float dt_s) {
  if (target <= previous || dt_s <= 0.0f) {
    return target;
  }

  float max_step = rate * dt_s;
  return previous + clamp_f(target - previous, 0.0f, max_step);
}

static float apply_launch_mode(const vcu_inputs_t *in, vcu_outputs_t *out,
                               torque_map_state_t *state,
                               const vcu_parameters_t *params,
                               float requested_torque_nm,
                               float raw_pedal_fraction, float dt_s) {
  float abs_motor_rpm = fabsf(in->motor_speed_rpm);
  bool pedal_in_launch_window =
      raw_pedal_fraction >= params->torque_map.launch_pedal_min &&
      raw_pedal_fraction <= params->torque_map.launch_pedal_max;
  bool brake_ready = out->brake_pressed &&
                     out->bse_psi >= params->torque_map.launch_brake_min_psi;
  bool low_speed_entry = abs_motor_rpm <= params->torque_map.launch_enter_rpm;
  bool low_speed_continue = abs_motor_rpm < params->torque_map.launch_exit_rpm;
  bool launch_requested = brake_ready && pedal_in_launch_window &&
                          low_speed_entry && requested_torque_nm > 0.0f;

  out->debug.launch_raw_torque_cmd_nm = requested_torque_nm;

  if (params->torque_map.launch_mode_disable) {
    launch_mode_reset(state);
    out->debug.launch_state = (uint8_t)state->launch_state;
    out->debug.launch_torque_cmd_nm = requested_torque_nm;
    return requested_torque_nm;
  }

  if (raw_pedal_fraction < 0.01f || requested_torque_nm <= 0.0f ||
      !low_speed_continue) {
    launch_mode_reset(state);
    out->debug.launch_state = (uint8_t)state->launch_state;
    out->debug.launch_torque_cmd_nm = requested_torque_nm;
    return requested_torque_nm;
  }

  if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_INACTIVE &&
      launch_requested) {
    state->launch_state = TORQUE_MAP_LAUNCH_STATE_PRELOAD;
    state->launch_output_torque_nm = 0.0f;
  }

  if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_PRELOAD) {
    if (brake_ready && pedal_in_launch_window) {
      float preload_target_nm =
          fminf(requested_torque_nm, params->torque_map.launch_preload_torque_nm);
      state->launch_output_torque_nm =
          rate_limit_rising(state->launch_output_torque_nm, preload_target_nm,
                            params->torque_map.launch_preload_ramp_rate_nm_per_s,
                            dt_s);
    } else {
      state->launch_state = TORQUE_MAP_LAUNCH_STATE_RELEASE;
    }
  }

  if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_RELEASE) {
    state->launch_output_torque_nm =
        rate_limit_rising(state->launch_output_torque_nm, requested_torque_nm,
                          params->torque_map.launch_release_ramp_rate_nm_per_s,
                          dt_s);
    if (fabsf(state->launch_output_torque_nm - requested_torque_nm) < 0.05f) {
      launch_mode_reset(state);
      out->debug.launch_state = (uint8_t)state->launch_state;
      out->debug.launch_torque_cmd_nm = requested_torque_nm;
      return requested_torque_nm;
    }
  }

  float output_torque_nm = requested_torque_nm;
  if (state->launch_state != TORQUE_MAP_LAUNCH_STATE_INACTIVE) {
    output_torque_nm = clamp_f(state->launch_output_torque_nm, 0.0f,
                               requested_torque_nm);
  }

  out->debug.launch_active =
      state->launch_state != TORQUE_MAP_LAUNCH_STATE_INACTIVE;
  out->debug.launch_state = (uint8_t)state->launch_state;
  out->debug.launch_torque_cmd_nm = output_torque_nm;

  return output_torque_nm;
}

void torque_map_init(torque_map_state_t *state) {
  memset(state, 0, sizeof(torque_map_state_t));
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         torque_map_state_t *state, vcu_parameters_t *params,
                         uint32_t dt_ms) {
  const float dt_s = (float)dt_ms / 1000.0f;

  if (!torque_map_params_are_valid(params)) {
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }

  float raw_pedal_fraction = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float shaped_pedal_fraction = shape_pedal_request(
      raw_pedal_fraction, params->torque_map.pedal_exponential_factor);

  if (!in->inverter_power_valid || !in->inverter_speed_valid) {
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }
  if (!isfinite(in->inverter_dc_bus_voltage_v) ||
      in->inverter_dc_bus_voltage_v <= 0.0f ||
      !isfinite(in->inverter_dc_bus_current_a)) {
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }

  float ocv_tau_s = params->torque_map.ocv_lpf_time_constant_s;
  float current_tau_s = params->torque_map.current_lpf_time_constant_s;
  float measured_power_tau_s =
      params->torque_map.measured_power_lpf_time_constant_s;
  float voltage_estimate_v =
      in->battery_status_valid ? in->battery_voltage_v
                               : in->inverter_dc_bus_voltage_v;

  if (!state->current_initialized) {
    state->filtered_current_a = in->inverter_dc_bus_current_a;
    state->current_initialized = true;
  } else {
    state->filtered_current_a =
        lpf_step(state->filtered_current_a, in->inverter_dc_bus_current_a,
                 lpf_alpha_from_tau(dt_s, current_tau_s));
  }

  if (!state->ocv_initialized) {
    state->ocv_estimate_v = voltage_estimate_v;
    state->ocv_initialized = true;
  } else if (fabsf(in->inverter_dc_bus_current_a) < 1.0f) {
    state->ocv_estimate_v =
        lpf_step(state->ocv_estimate_v, voltage_estimate_v,
                 lpf_alpha_from_tau(dt_s, ocv_tau_s));
  }

  float power_limit_w = params->torque_map.power_limit_w;
  float current_limit_a = params->torque_map.current_limit_a;
  float hard_current_cut_a = params->torque_map.hard_current_cut_a;
  float hard_power_cut_w = params->torque_map.hard_power_cut_w;
  float ocv_cell_count = params->torque_map.ocv_cell_count;
  float min_rpm = params->torque_map.power_limit_min_rpm;
  float trim_limit_nm = params->torque_map.power_limit_trim_limit_nm;
  float hardware_torque_limit_nm = params->torque_map.max_torque_nm;

  float electrical_power_reference_w =
      in->inverter_dc_bus_voltage_v * in->inverter_dc_bus_current_a;
  float current_based_power_limit_w =
      in->inverter_dc_bus_voltage_v * current_limit_a;
  float active_power_limit_w =
      fmaxf(fminf(power_limit_w, current_based_power_limit_w), 0.0f);

  float voltage_derate =
      clamp_f((state->ocv_estimate_v / ocv_cell_count - 3.3f) / 0.1f, 0.0f,
              1.0f);
  float voltage_derated_torque_limit_nm =
      hardware_torque_limit_nm * voltage_derate;

  float measured_power_w = electrical_power_reference_w;
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
  out->debug.low_voltage_derate_pct = voltage_derate * 100.0f;

  float motor_rpm = fabsf(in->motor_speed_rpm);
  float efficiency = motor_efficiency_at_rpm(params, motor_rpm);
  out->debug.motor_efficiency = efficiency;

  float mechanical_power_limit_w = active_power_limit_w * efficiency;
  float feedforward_torque_nm =
      torque_from_power(mechanical_power_limit_w, motor_rpm, min_rpm);
  feedforward_torque_nm = clamp_f(feedforward_torque_nm, 0.0f,
                                  voltage_derated_torque_limit_nm);

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
  if (raw_pedal_fraction < 0.01f || voltage_derated_torque_limit_nm <= 0.0f) {
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
      clamp_f(available_torque_nm, 0.0f, voltage_derated_torque_limit_nm);

  float requested_torque_nm =
      clamp_f(shaped_pedal_fraction * available_torque_nm, 0.0f,
              available_torque_nm);
  out->torque_cmd = apply_launch_mode(in, out, state, params,
                                      requested_torque_nm, raw_pedal_fraction,
                                      dt_s);
  out->debug.power_limit_feedback_p_nm = proportional_nm;
  out->debug.power_limit_feedback_i_nm = integral_trim_nm;
  out->debug.power_limit_feedback_d_nm = derivative_nm;
  out->debug.power_limit_feedback_torque_nm = trim_torque_nm;
}
