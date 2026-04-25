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
  if (!params->torque_map.launch_compensation_disable) {
    launch_params_valid =
        isfinite(params->torque_map.launch_start_rpm) &&
        params->torque_map.launch_start_rpm >= 0.0f &&
        isfinite(params->torque_map.launch_exit_rpm) &&
        params->torque_map.launch_exit_rpm >
            params->torque_map.launch_start_rpm &&
        isfinite(params->torque_map.launch_preload_torque_nm) &&
        params->torque_map.launch_preload_torque_nm >= 0.0f &&
        isfinite(params->torque_map.launch_preload_timeout_ms) &&
        params->torque_map.launch_preload_timeout_ms >= 0.0f &&
        isfinite(params->torque_map.launch_rpm_lpf_time_constant_s) &&
        params->torque_map.launch_rpm_lpf_time_constant_s >= 0.0f &&
        isfinite(params->torque_map.launch_contact_positive_accel_rpm_per_s) &&
        params->torque_map.launch_contact_positive_accel_rpm_per_s >= 0.0f &&
        isfinite(params->torque_map.launch_contact_decel_rpm_per_s) &&
        params->torque_map.launch_contact_decel_rpm_per_s >= 0.0f &&
        isfinite(params->torque_map.launch_contact_decel_confirm_ms) &&
        params->torque_map.launch_contact_decel_confirm_ms >= 0.0f &&
        isfinite(params->torque_map.launch_ramp_rate_nm_per_s) &&
        params->torque_map.launch_ramp_rate_nm_per_s > 0.0f;
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

static void launch_compensation_reset(torque_map_state_t *state) {
  state->launch_state = TORQUE_MAP_LAUNCH_STATE_IDLE;
  state->launch_preload_elapsed_ms = 0.0f;
  state->launch_decel_elapsed_ms = 0.0f;
  state->launch_rpm_initialized = false;
  state->launch_has_rpm_history = false;
  state->launch_motor_accel_rpm_per_s = 0.0f;
  state->launch_saw_positive_accel = false;
  state->launch_contact_detected = false;
  state->launch_output_torque_nm = 0.0f;
}

static void update_launch_rpm_estimate(const vcu_inputs_t *in,
                                       torque_map_state_t *state,
                                       const vcu_parameters_t *params,
                                       float dt_s) {
  float motor_rpm = in->motor_speed_rpm;
  float alpha = lpf_alpha_from_tau(
      dt_s, params->torque_map.launch_rpm_lpf_time_constant_s);

  if (!state->launch_rpm_initialized) {
    state->launch_filtered_motor_rpm = motor_rpm;
    state->launch_previous_filtered_motor_rpm = motor_rpm;
    state->launch_rpm_initialized = true;
    state->launch_has_rpm_history = true;
    state->launch_motor_accel_rpm_per_s = 0.0f;
    return;
  }

  state->launch_previous_filtered_motor_rpm =
      state->launch_filtered_motor_rpm;
  state->launch_filtered_motor_rpm =
      lpf_step(state->launch_filtered_motor_rpm, motor_rpm, alpha);

  if (dt_s > 0.0f && state->launch_has_rpm_history) {
    state->launch_motor_accel_rpm_per_s =
        (state->launch_filtered_motor_rpm -
         state->launch_previous_filtered_motor_rpm) /
        dt_s;
  } else {
    state->launch_motor_accel_rpm_per_s = 0.0f;
    state->launch_has_rpm_history = true;
  }
}

static float rate_limit_rising(float previous, float target, float rate,
                               float dt_s) {
  if (target <= previous || dt_s <= 0.0f) {
    return target;
  }

  float max_step = rate * dt_s;
  return previous + clamp_f(target - previous, 0.0f, max_step);
}

static float apply_launch_compensation(const vcu_inputs_t *in,
                                       vcu_outputs_t *out,
                                       torque_map_state_t *state,
                                       const vcu_parameters_t *params,
                                       float requested_torque_nm,
                                       float raw_pedal_fraction,
                                       float dt_s, uint32_t dt_ms) {
  const float pedal_release_threshold = 0.01f;
  float abs_motor_rpm = fabsf(in->motor_speed_rpm);

  out->debug.launch_raw_torque_cmd_nm = requested_torque_nm;

  if (params->torque_map.launch_compensation_disable) {
    launch_compensation_reset(state);
    out->debug.launch_comp_state = (uint8_t)state->launch_state;
    return requested_torque_nm;
  }

  update_launch_rpm_estimate(in, state, params, dt_s);

  if (raw_pedal_fraction < pedal_release_threshold ||
      requested_torque_nm <= 0.0f ||
      abs_motor_rpm >= params->torque_map.launch_exit_rpm) {
    launch_compensation_reset(state);
    out->debug.launch_filtered_motor_rpm = state->launch_filtered_motor_rpm;
    out->debug.launch_motor_accel_rpm_per_s =
        state->launch_motor_accel_rpm_per_s;
    out->debug.launch_comp_state = (uint8_t)state->launch_state;
    return requested_torque_nm;
  }

  if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_IDLE &&
      abs_motor_rpm <= params->torque_map.launch_start_rpm) {
    state->launch_state = TORQUE_MAP_LAUNCH_STATE_PRELOAD;
    state->launch_preload_elapsed_ms = 0.0f;
    state->launch_decel_elapsed_ms = 0.0f;
    state->launch_saw_positive_accel = false;
    state->launch_contact_detected = false;
    state->launch_output_torque_nm =
        fminf(requested_torque_nm, params->torque_map.launch_preload_torque_nm);
  }

  if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_PRELOAD) {
    state->launch_preload_elapsed_ms += (float)dt_ms;

    if (state->launch_motor_accel_rpm_per_s >=
        params->torque_map.launch_contact_positive_accel_rpm_per_s) {
      state->launch_saw_positive_accel = true;
    }

    bool decel_detected =
        state->launch_saw_positive_accel &&
        state->launch_motor_accel_rpm_per_s <=
            -params->torque_map.launch_contact_decel_rpm_per_s;
    if (decel_detected) {
      state->launch_decel_elapsed_ms += (float)dt_ms;
    } else {
      state->launch_decel_elapsed_ms = 0.0f;
    }

    bool contact_detected =
        decel_detected &&
        state->launch_decel_elapsed_ms >=
            params->torque_map.launch_contact_decel_confirm_ms;
    bool timed_out = state->launch_preload_elapsed_ms >=
                     params->torque_map.launch_preload_timeout_ms;
    state->launch_output_torque_nm =
        fminf(requested_torque_nm, params->torque_map.launch_preload_torque_nm);

    if (contact_detected || timed_out) {
      state->launch_contact_detected = contact_detected;
      state->launch_state = TORQUE_MAP_LAUNCH_STATE_RAMP;
    }
  }

  if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_RAMP) {
    state->launch_output_torque_nm =
        rate_limit_rising(state->launch_output_torque_nm, requested_torque_nm,
                          params->torque_map.launch_ramp_rate_nm_per_s, dt_s);
    if (fabsf(state->launch_output_torque_nm - requested_torque_nm) < 0.05f) {
      state->launch_state = TORQUE_MAP_LAUNCH_STATE_COMPLETE;
      state->launch_output_torque_nm = requested_torque_nm;
    }
  } else if (state->launch_state == TORQUE_MAP_LAUNCH_STATE_COMPLETE) {
    state->launch_output_torque_nm = requested_torque_nm;
  }

  float output_torque_nm = requested_torque_nm;
  if (state->launch_state != TORQUE_MAP_LAUNCH_STATE_IDLE) {
    output_torque_nm = clamp_f(state->launch_output_torque_nm, 0.0f,
                               requested_torque_nm);
  }

  out->debug.launch_comp_active =
      state->launch_state == TORQUE_MAP_LAUNCH_STATE_PRELOAD ||
      state->launch_state == TORQUE_MAP_LAUNCH_STATE_RAMP;
  out->debug.launch_comp_state = (uint8_t)state->launch_state;
  out->debug.launch_contact_detected = state->launch_contact_detected;
  out->debug.launch_filtered_motor_rpm = state->launch_filtered_motor_rpm;
  out->debug.launch_motor_accel_rpm_per_s =
      state->launch_motor_accel_rpm_per_s;
  out->debug.launch_torque_limit_nm = output_torque_nm;

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
  out->torque_cmd =
      apply_launch_compensation(in, out, state, params, requested_torque_nm,
                                raw_pedal_fraction, dt_s, dt_ms);
  out->debug.power_limit_feedback_p_nm = proportional_nm;
  out->debug.power_limit_feedback_i_nm = integral_trim_nm;
  out->debug.power_limit_feedback_d_nm = derivative_nm;
  out->debug.power_limit_feedback_torque_nm = trim_torque_nm;
}
