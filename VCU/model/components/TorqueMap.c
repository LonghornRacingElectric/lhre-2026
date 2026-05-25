#include "TorqueMap.h"
#include "util.h"

#include <math.h>
#include <stdbool.h>
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

static bool power_limit_torque_map_is_valid(const vcu_parameters_t *params) {
  float previous_rpm = -1.0f;

  for (int i = 0; i < VCU_POWER_LIMIT_TORQUE_MAP_POINTS; i++) {
    float rpm = params->torque_map.power_limit_torque_rpm[i];
    float torque_nm = params->torque_map.power_limit_torque_nm[i];

    if (!isfinite(rpm) || rpm < 0.0f || rpm <= previous_rpm) {
      return false;
    }

    if (!isfinite(torque_nm) || torque_nm < 0.0f ||
        torque_nm > params->torque_map.max_torque_nm) {
      return false;
    }

    previous_rpm = rpm;
  }

  return true;
}

static bool torque_map_params_are_valid(const vcu_parameters_t *params) {
  return isfinite(params->torque_map.max_torque_nm) &&
         params->torque_map.max_torque_nm > 0.0f &&
         isfinite(params->torque_map.pedal_exponent) &&
         params->torque_map.pedal_exponent >= 0.1f &&
         params->torque_map.pedal_exponent <= 5.0f &&
         isfinite(params->torque_map.power_limit_w) &&
         params->torque_map.power_limit_w > 0.0f &&
         isfinite(params->torque_map.current_limit_a) &&
         params->torque_map.current_limit_a > 0.0f &&
         isfinite(params->torque_map.hard_current_cut_a) &&
         params->torque_map.hard_current_cut_a >=
             params->torque_map.current_limit_a &&
         isfinite(params->torque_map.hard_power_cut_w) &&
         params->torque_map.hard_power_cut_w >=
             params->torque_map.power_limit_w &&
         isfinite(params->torque_map.ocv_cell_count) &&
         params->torque_map.ocv_cell_count > 0.0f &&
         isfinite(params->torque_map.ocv_lpf_time_constant_s) &&
         params->torque_map.ocv_lpf_time_constant_s >= 0.0f &&
         isfinite(params->torque_map.power_limit_trim_limit_nm) &&
         params->torque_map.power_limit_trim_limit_nm >= 0.0f &&
         isfinite(params->torque_map.power_limit_kp) &&
         params->torque_map.power_limit_kp >= 0.0f &&
         isfinite(params->torque_map.power_limit_ki) &&
         params->torque_map.power_limit_ki >= 0.0f &&
         isfinite(params->torque_map.power_limit_kd) &&
         params->torque_map.power_limit_kd >= 0.0f &&
         power_limit_torque_map_is_valid(params);
}

static float power_limit_torque_at_rpm(const vcu_parameters_t *params,
                                       float motor_rpm) {
  const float *rpm_map = params->torque_map.power_limit_torque_rpm;
  const float *torque_map = params->torque_map.power_limit_torque_nm;
  const int last_index = VCU_POWER_LIMIT_TORQUE_MAP_POINTS - 1;
  float rpm = fmaxf(motor_rpm, 0.0f);

  if (rpm <= rpm_map[0]) {
    return torque_map[0];
  }

  for (int i = 0; i < last_index; i++) {
    if (rpm <= rpm_map[i + 1]) {
      float pct = (rpm - rpm_map[i]) / (rpm_map[i + 1] - rpm_map[i]);
      return linear_interp(torque_map[i], torque_map[i + 1], pct);
    }
  }

  return torque_map[last_index];
}

static float compute_low_voltage_derate(float ocv_estimate_v,
                                        const vcu_parameters_t *params) {
  float cell_ocv_v = ocv_estimate_v / params->torque_map.ocv_cell_count;
  return clamp_f((cell_ocv_v - 3.3f) / 0.1f, 0.0f, 1.0f);
}

static float shape_pedal(float pedal, float exponent) {
  float clamped_pedal = clamp_f(pedal, 0.0f, 1.0f);

  if (clamped_pedal <= 0.0f) {
    return 0.0f;
  }

  if (clamped_pedal >= 1.0f) {
    return 1.0f;
  }

  return clamp_f(powf(clamped_pedal, exponent), 0.0f, 1.0f);
}

void torque_map_init(torque_map_state_t *state,
                     const vcu_parameters_t *params) {
  (void)params;
  memset(state, 0, sizeof(torque_map_state_t));
}

void torque_map_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                         torque_map_state_t *state,
                         const vcu_parameters_t *params, uint32_t dt_ms) {
  const float dt_s = (float)dt_ms * 0.001f;

  if (!torque_map_params_are_valid(params)) {
    out->torque_lookup_output = 0.0f;
    out->torque_derated = 0.0f;
    out->torque_power_limited = 0.0f;
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }

  if (!in->inverter_power_valid || !in->inverter_speed_valid ||
      !isfinite(in->inverter_dc_bus_voltage_v) ||
      in->inverter_dc_bus_voltage_v <= 0.0f ||
      !isfinite(in->inverter_dc_bus_current_a) ||
      !isfinite(in->motor_speed_rpm)) {
    out->torque_lookup_output = 0.0f;
    out->torque_derated = 0.0f;
    out->torque_power_limited = 0.0f;
    out->torque_cmd = 0.0f;
    out->faults.power_limit_input_fault = true;
    return;
  }

  float max_torque_nm = params->torque_map.max_torque_nm;
  float pedal_raw_pct = clamp_f(out->accel_pedal_travel, 0.0f, 1.0f);
  float pedal_shaped_pct =
      shape_pedal(pedal_raw_pct, params->torque_map.pedal_exponent);
  float motor_rpm = fabsf(in->motor_speed_rpm);

  float voltage_estimate_v =
      in->battery_status_valid && isfinite(in->battery_voltage_v) &&
              in->battery_voltage_v > 0.0f
          ? in->battery_voltage_v
          : in->inverter_dc_bus_voltage_v;

  if (!state->ocv_initialized) {
    state->ocv_estimate_v = voltage_estimate_v;
    state->ocv_initialized = true;
  } else if (fabsf(in->inverter_dc_bus_current_a) < 1.0f) {
    state->ocv_estimate_v =
        lpf_step(state->ocv_estimate_v, voltage_estimate_v,
                 lpf_alpha_from_tau(
                     dt_s, params->torque_map.ocv_lpf_time_constant_s));
  }

  float measured_power_w =
      in->inverter_dc_bus_voltage_v * in->inverter_dc_bus_current_a;
  float measured_power_rate_w_s = 0.0f;
  if (dt_s > 0.0f && state->has_power_history) {
    measured_power_rate_w_s =
        (measured_power_w - state->previous_measured_power_w) / dt_s;
  }
  state->previous_measured_power_w = measured_power_w;
  state->has_power_history = true;

  float current_based_power_limit_w =
      in->inverter_dc_bus_voltage_v * params->torque_map.current_limit_a;
  float active_power_limit_w =
      fmaxf(fminf(params->torque_map.power_limit_w,
                 current_based_power_limit_w),
            0.0f);
  float power_limit_scale =
      clamp_f(active_power_limit_w / params->torque_map.power_limit_w, 0.0f,
              1.0f);

  float voltage_derate = compute_low_voltage_derate(state->ocv_estimate_v,
                                                    params);
  float voltage_derated_torque_limit_nm = max_torque_nm * voltage_derate;

  float table_torque_nm =
      clamp_f(power_limit_torque_at_rpm(params, motor_rpm), 0.0f,
              max_torque_nm);
  float scaled_table_torque_nm = table_torque_nm * power_limit_scale;
  float derated_table_torque_nm =
      clamp_f(scaled_table_torque_nm * voltage_derate, 0.0f,
              voltage_derated_torque_limit_nm);

  out->torque_lookup_output = table_torque_nm;
  out->derate_factor_cell_voltage = voltage_derate;
  out->derate_factor_cell_temp = 1.0f;
  out->torque_derated = derated_table_torque_nm;

  out->debug.ocv_estimate_v = state->ocv_estimate_v;
  out->debug.active_power_limit_w = active_power_limit_w;
  out->debug.measured_power_w = measured_power_w;
  out->debug.low_voltage_derate_pct = voltage_derate * 100.0f;
  out->debug.power_limit_feedforward_torque_nm = derated_table_torque_nm;
  out->debug.pedal_shaped_pct = pedal_shaped_pct;

  if (in->inverter_dc_bus_current_a > params->torque_map.hard_current_cut_a ||
      measured_power_w > params->torque_map.hard_power_cut_w) {
    state->integral_w_s = 0.0f;
    out->torque_power_limited = 0.0f;
    out->torque_cmd = 0.0f;
    out->faults.current_safety_cut =
        in->inverter_dc_bus_current_a > params->torque_map.hard_current_cut_a;
    out->faults.power_safety_cut =
        measured_power_w > params->torque_map.hard_power_cut_w;
    return;
  }

  float power_error_w = active_power_limit_w - measured_power_w;
  if (pedal_raw_pct < 0.01f || derated_table_torque_nm <= 0.0f) {
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
  float trim_limit_nm = params->torque_map.power_limit_trim_limit_nm;

  bool saturating_high =
      unsaturated_trim_nm > trim_limit_nm && power_error_w > 0.0f;
  bool saturating_low =
      unsaturated_trim_nm < -trim_limit_nm && power_error_w < 0.0f;
  if (!(saturating_high || saturating_low)) {
    state->integral_w_s = candidate_integral_w_s;
  }

  float integral_trim_nm =
      params->torque_map.power_limit_ki * state->integral_w_s;
  float trim_torque_nm = proportional_nm + integral_trim_nm + derivative_nm;
  trim_torque_nm = clamp_f(trim_torque_nm, -trim_limit_nm, trim_limit_nm);

  float available_torque_nm = derated_table_torque_nm + trim_torque_nm;
  available_torque_nm =
      clamp_f(available_torque_nm, 0.0f, voltage_derated_torque_limit_nm);

  out->debug.power_limit_feedback_p_nm = proportional_nm;
  out->debug.power_limit_feedback_i_nm = integral_trim_nm;
  out->debug.power_limit_feedback_d_nm = derivative_nm;
  out->debug.power_limit_feedback_torque_nm = trim_torque_nm;
  out->debug.power_limit_available_torque_nm = available_torque_nm;

  out->torque_power_limited =
      clamp_f(pedal_shaped_pct * available_torque_nm, 0.0f,
              available_torque_nm);
  out->torque_cmd = out->torque_power_limited;
}
