#include "TractionControl.h"
#include "util.h"
#include <math.h>
#include <string.h>

static bool traction_control_params_are_valid(
    const vcu_traction_control_parameters_t *tc) {
  return isfinite(tc->tc_wheel_radius_m) && tc->tc_wheel_radius_m > 0.0f &&
         isfinite(tc->tc_final_drive_ratio) &&
         tc->tc_final_drive_ratio > 0.0f &&
         isfinite(tc->tc_base_target_slip) &&
         tc->tc_base_target_slip >= tc->tc_min_target_slip &&
         isfinite(tc->tc_min_target_slip) && tc->tc_min_target_slip > 0.0f &&
         isfinite(tc->tc_max_target_slip) &&
         tc->tc_max_target_slip >= tc->tc_base_target_slip &&
         isfinite(tc->tc_slip_hysteresis) &&
         tc->tc_slip_hysteresis >= 0.0f &&
         isfinite(tc->tc_lateral_accel_limit_mps2) &&
         tc->tc_lateral_accel_limit_mps2 > 0.0f &&
         isfinite(tc->tc_aero_lateral_accel_gain_per_mps2) &&
         tc->tc_aero_lateral_accel_gain_per_mps2 >= 0.0f &&
         isfinite(tc->tc_lateral_slip_reduction_gain) &&
         tc->tc_lateral_slip_reduction_gain >= 0.0f &&
         isfinite(tc->tc_min_vehicle_speed_mps) &&
         tc->tc_min_vehicle_speed_mps > 0.0f &&
         isfinite(tc->tc_min_torque_nm) && tc->tc_min_torque_nm >= 0.0f &&
         isfinite(tc->tc_max_wheel_speed_mps) &&
         tc->tc_max_wheel_speed_mps > 0.0f &&
         isfinite(tc->tc_max_reference_accel_mps2) &&
         tc->tc_max_reference_accel_mps2 > 0.0f &&
         isfinite(tc->tc_front_disagreement_mps) &&
         tc->tc_front_disagreement_mps > 0.0f &&
         isfinite(tc->tc_rear_disagreement_mps) &&
         tc->tc_rear_disagreement_mps > 0.0f &&
         isfinite(tc->tc_motor_rear_disagreement_mps) &&
         tc->tc_motor_rear_disagreement_mps > 0.0f &&
         isfinite(tc->tc_speed_lpf_time_constant_s) &&
         tc->tc_speed_lpf_time_constant_s >= 0.0f &&
         isfinite(tc->tc_slip_lpf_time_constant_s) &&
         tc->tc_slip_lpf_time_constant_s >= 0.0f &&
         isfinite(tc->tc_feedback_lpf_time_constant_s) &&
         tc->tc_feedback_lpf_time_constant_s >= 0.0f &&
         isfinite(tc->tc_reference_accel_blend) &&
         tc->tc_reference_accel_blend >= 0.0f &&
         tc->tc_reference_accel_blend <= 1.0f &&
         isfinite(tc->tc_kp_nm_per_slip) && tc->tc_kp_nm_per_slip >= 0.0f &&
         isfinite(tc->tc_ki_nm_per_slip_s) &&
         tc->tc_ki_nm_per_slip_s >= 0.0f &&
         isfinite(tc->tc_kd_nm_per_slip_rate) &&
         tc->tc_kd_nm_per_slip_rate >= 0.0f &&
         isfinite(tc->tc_driven_accel_gain_nm_per_mps2) &&
         tc->tc_driven_accel_gain_nm_per_mps2 >= 0.0f &&
         isfinite(tc->tc_integral_limit_nm) &&
         tc->tc_integral_limit_nm >= 0.0f &&
         isfinite(tc->tc_max_torque_reduction_nm) &&
         tc->tc_max_torque_reduction_nm >= 0.0f &&
         isfinite(tc->tc_cut_slew_nm_per_s) &&
         tc->tc_cut_slew_nm_per_s > 0.0f &&
         isfinite(tc->tc_recovery_slew_nm_per_s) &&
         tc->tc_recovery_slew_nm_per_s > 0.0f;
}

static float lpf_alpha_from_tau(float dt_s, float tau_s) {
  if (tau_s <= 0.0f) {
    return 1.0f;
  }
  return clamp_f(dt_s / (tau_s + dt_s), 0.0f, 1.0f);
}

static float lpf_step(float previous, float input, float alpha) {
  return alpha * input + (1.0f - alpha) * previous;
}

typedef struct {
  float vehicle_speed_mps;
  float driven_speed_mps;
  float front_disagreement_mps;
  float rear_disagreement_mps;
  float motor_rear_disagreement_mps;
  uint32_t sensor_fault_flags;
  bool vehicle_speed_valid;
  bool driven_speed_valid;
} tc_speed_estimate_t;

static float rate_limit(float previous, float target, float rising_rate,
                        float falling_rate, float dt_s) {
  if (dt_s <= 0.0f) {
    return target;
  }

  if (target > previous) {
    float max_step = rising_rate * dt_s;
    return previous + clamp_f(target - previous, 0.0f, max_step);
  }

  float max_step = falling_rate * dt_s;
  return previous - clamp_f(previous - target, 0.0f, max_step);
}

static bool speed_is_plausible(float speed_mps, float max_speed_mps) {
  return isfinite(speed_mps) && speed_mps >= 0.0f &&
         speed_mps <= max_speed_mps;
}

static bool reference_accel_is_plausible(bool has_previous, float current_mps,
                                         float previous_mps, float max_accel,
                                         float dt_s) {
  if (!has_previous || dt_s <= 0.0f || max_accel <= 0.0f) {
    return true;
  }

  float max_delta_mps = max_accel * dt_s + 0.25f;
  return fabsf(current_mps - previous_mps) <= max_delta_mps;
}

static float choose_closer_to_prediction(float a, float b,
                                         float prediction_mps) {
  return fabsf(a - prediction_mps) <= fabsf(b - prediction_mps) ? a : b;
}

static tc_speed_estimate_t estimate_speeds(const vcu_inputs_t *in,
                                           traction_control_state_t *state,
                                           const vcu_parameters_t *params,
                                           float dt_s) {
  const vcu_traction_control_parameters_t *tc = &params->traction_control;
  tc_speed_estimate_t estimate = {0};

  float wheel_radius_m = tc->tc_wheel_radius_m;
  float final_drive_ratio = tc->tc_final_drive_ratio;
  float max_speed_mps = tc->tc_max_wheel_speed_mps;
  float max_reference_accel_mps2 = tc->tc_max_reference_accel_mps2;

  float fl = fabsf(in->wheel_speed_fl_rad_s) * wheel_radius_m;
  float fr = fabsf(in->wheel_speed_fr_rad_s) * wheel_radius_m;
  float rl = fabsf(in->wheel_speed_rl_rad_s) * wheel_radius_m;
  float rr = fabsf(in->wheel_speed_rr_rad_s) * wheel_radius_m;

  bool fl_range_valid = speed_is_plausible(fl, max_speed_mps);
  bool fr_range_valid = speed_is_plausible(fr, max_speed_mps);
  bool fl_accel_valid =
      reference_accel_is_plausible(state->has_previous_fl_speed, fl,
                                   state->previous_fl_mps,
                                   max_reference_accel_mps2, dt_s);
  bool fr_accel_valid =
      reference_accel_is_plausible(state->has_previous_fr_speed, fr,
                                   state->previous_fr_mps,
                                   max_reference_accel_mps2, dt_s);
  bool fl_valid = fl_range_valid && fl_accel_valid;
  bool fr_valid = fr_range_valid && fr_accel_valid;
  bool rl_valid = speed_is_plausible(rl, max_speed_mps);
  bool rr_valid = speed_is_plausible(rr, max_speed_mps);

  float motor_mps = 0.0f;
  bool motor_valid = in->inverter_speed_valid && final_drive_ratio > 0.0f;
  if (motor_valid) {
    float motor_rad_s = fabsf(in->motor_speed_rpm) * 6.28318530718f / 60.0f;
    motor_mps = motor_rad_s * wheel_radius_m / final_drive_ratio;
    motor_valid = speed_is_plausible(motor_mps, max_speed_mps);
  }

  if (!fl_valid) {
    estimate.sensor_fault_flags |= TC_SENSOR_FRONT_LEFT_INVALID;
  }
  if (!fr_valid) {
    estimate.sensor_fault_flags |= TC_SENSOR_FRONT_RIGHT_INVALID;
  }
  if ((fl_range_valid && !fl_accel_valid) ||
      (fr_range_valid && !fr_accel_valid)) {
    estimate.sensor_fault_flags |= TC_SENSOR_REFERENCE_ACCEL_IMPLAUS;
  }
  if (!rl_valid) {
    estimate.sensor_fault_flags |= TC_SENSOR_REAR_LEFT_INVALID;
  }
  if (!rr_valid) {
    estimate.sensor_fault_flags |= TC_SENSOR_REAR_RIGHT_INVALID;
  }
  if (!motor_valid) {
    estimate.sensor_fault_flags |= TC_SENSOR_MOTOR_SPEED_INVALID;
  }

  float front_disagreement_limit = tc->tc_front_disagreement_mps;
  float prediction_mps = state->vehicle_speed_initialized
                             ? state->filtered_vehicle_speed_mps
                             : 0.5f * (fl + fr);
  if (fl_valid && fr_valid) {
    estimate.front_disagreement_mps = fabsf(fl - fr);
    if (estimate.front_disagreement_mps > front_disagreement_limit) {
      estimate.sensor_fault_flags |= TC_SENSOR_FRONT_DISAGREE;
      estimate.vehicle_speed_mps = choose_closer_to_prediction(fl, fr,
                                                               prediction_mps);
    } else {
      estimate.vehicle_speed_mps = 0.5f * (fl + fr);
    }
    estimate.vehicle_speed_valid = true;
  } else if (fl_valid) {
    estimate.vehicle_speed_mps = fl;
    estimate.vehicle_speed_valid = true;
  } else if (fr_valid) {
    estimate.vehicle_speed_mps = fr;
    estimate.vehicle_speed_valid = true;
  } else {
    estimate.sensor_fault_flags |= TC_SENSOR_NO_REFERENCE_SPEED;
  }

  float rear_from_wheels = 0.0f;
  bool rear_from_wheels_valid = false;
  float rear_disagreement_limit = tc->tc_rear_disagreement_mps;
  float motor_rear_disagreement_limit = tc->tc_motor_rear_disagreement_mps;

  if (rl_valid && rr_valid) {
    estimate.rear_disagreement_mps = fabsf(rl - rr);
    if (estimate.rear_disagreement_mps > rear_disagreement_limit) {
      estimate.sensor_fault_flags |= TC_SENSOR_REAR_DISAGREE;
      float high_rear = fmaxf(rl, rr);
      float low_rear = fminf(rl, rr);
      bool high_rear_is_motor_outlier =
          motor_valid &&
          fabsf(high_rear - motor_mps) > motor_rear_disagreement_limit &&
          fabsf(low_rear - motor_mps) <= motor_rear_disagreement_limit;
      rear_from_wheels = high_rear_is_motor_outlier ? low_rear : high_rear;
    } else {
      rear_from_wheels = fmaxf(rl, rr);
    }
    rear_from_wheels_valid = true;
  } else if (rl_valid) {
    rear_from_wheels = rl;
    rear_from_wheels_valid = true;
  } else if (rr_valid) {
    rear_from_wheels = rr;
    rear_from_wheels_valid = true;
  }

  if (rear_from_wheels_valid && motor_valid) {
    estimate.motor_rear_disagreement_mps = fabsf(motor_mps - rear_from_wheels);
    if (estimate.motor_rear_disagreement_mps > motor_rear_disagreement_limit) {
      estimate.sensor_fault_flags |= TC_SENSOR_MOTOR_REAR_DISAGREE;
    }
    estimate.driven_speed_mps = fmaxf(rear_from_wheels, motor_mps);
    estimate.driven_speed_valid = true;
  } else if (rear_from_wheels_valid) {
    estimate.driven_speed_mps = rear_from_wheels;
    estimate.driven_speed_valid = true;
  } else if (motor_valid) {
    estimate.driven_speed_mps = motor_mps;
    estimate.driven_speed_valid = true;
  } else {
    estimate.sensor_fault_flags |= TC_SENSOR_NO_DRIVEN_SPEED;
  }

  if (fl_valid) {
    state->previous_fl_mps = fl;
    state->has_previous_fl_speed = true;
  }
  if (fr_valid) {
    state->previous_fr_mps = fr;
    state->has_previous_fr_speed = true;
  }

  return estimate;
}

static float blend_accel_prediction(float measured_speed_mps,
                                    float previous_speed_mps,
                                    float longitudinal_accel_mps2,
                                    bool has_history, bool accel_valid,
                                    bool use_accel, float blend, float dt_s) {
  if (!has_history || !accel_valid || !use_accel || dt_s <= 0.0f) {
    return measured_speed_mps;
  }

  float predicted_speed_mps =
      fmaxf(previous_speed_mps + longitudinal_accel_mps2 * dt_s, 0.0f);
  return linear_interp(measured_speed_mps, predicted_speed_mps,
                       clamp_f(blend, 0.0f, 1.0f));
}

static float compute_target_slip(const vcu_inputs_t *in,
                                 const vcu_parameters_t *params,
                                 float vehicle_speed_mps,
                                 float *lateral_usage_out,
                                 float *lateral_accel_limit_out,
                                 float *effective_lateral_accel_out) {
  const vcu_traction_control_parameters_t *tc = &params->traction_control;

  float base_target = tc->tc_base_target_slip;
  float min_target = tc->tc_min_target_slip;
  float max_target = tc->tc_max_target_slip;
  if (max_target < min_target) {
    max_target = min_target;
  }

  float long_adjust = clamp_f(tc->tc_longitudinal_adjust, -1.0f, 1.0f);
  if (in->tc_driver_adjust_valid) {
    long_adjust =
        clamp_f(long_adjust + in->tc_longitudinal_adjust, -1.0f, 1.0f);
  }

  float target = base_target;
  if (long_adjust >= 0.0f) {
    target = linear_interp(base_target, max_target, long_adjust);
  } else {
    target = linear_interp(base_target, min_target, -long_adjust);
  }

  float lateral_accel_mps2 = 0.0f;
  if (in->accel_valid && tc->tc_use_accel) {
    lateral_accel_mps2 = in->lateral_accel_mps2;
  }
  float lateral_limit = tc->tc_lateral_accel_limit_mps2;
  if (tc->tc_aero_lateral_limit_enable) {
    float aero_gain = tc->tc_aero_lateral_accel_gain_per_mps2;
    lateral_limit += fmaxf(aero_gain, 0.0f) * vehicle_speed_mps *
                     vehicle_speed_mps;
  }
  float lateral_usage =
      lateral_limit > 1.0e-6f ? fabsf(lateral_accel_mps2) / lateral_limit
                              : 0.0f;
  lateral_usage = clamp_f(lateral_usage, 0.0f, 1.0f);

  float lateral_adjust = clamp_f(tc->tc_lateral_adjust, -1.0f, 1.0f);
  if (in->tc_driver_adjust_valid) {
    lateral_adjust = clamp_f(lateral_adjust + in->tc_lateral_adjust, -1.0f,
                             1.0f);
  }

  float lateral_gain = tc->tc_lateral_slip_reduction_gain;
  lateral_gain *= (1.0f + lateral_adjust);
  lateral_gain = clamp_f(lateral_gain, 0.0f, 2.0f);

  float lateral_reduction = lateral_usage * lateral_usage * lateral_gain;
  target = linear_interp(target, min_target,
                         clamp_f(lateral_reduction, 0.0f, 1.0f));

  if (lateral_usage_out != NULL) {
    *lateral_usage_out = lateral_usage;
  }
  if (lateral_accel_limit_out != NULL) {
    *lateral_accel_limit_out = lateral_limit;
  }
  if (effective_lateral_accel_out != NULL) {
    *effective_lateral_accel_out = lateral_accel_mps2;
  }
  return clamp_f(target, min_target, max_target);
}

void traction_control_init(traction_control_state_t *state) {
  memset(state, 0, sizeof(traction_control_state_t));
}

void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *state,
                               const vcu_parameters_t *params,
                               uint32_t dt_ms) {
  const vcu_traction_control_parameters_t *tc = &params->traction_control;
  float unregulated_torque_nm = fmaxf(out->torque_cmd, 0.0f);

  out->debug.tc_torque_limit_nm = unregulated_torque_nm;

  if (tc->tc_disable || tc->tc_wheel_radius_m <= 0.0f ||
      tc->tc_final_drive_ratio <= 0.0f) {
    state->torque_limit_nm = unregulated_torque_nm;
    state->torque_limit_initialized = true;
    return;
  }

  if (!traction_control_params_are_valid(tc)) {
    out->faults.tc_input_fault = true;
    state->slip_integral = 0.0f;
    state->torque_limit_nm = unregulated_torque_nm;
    state->torque_limit_initialized = true;
    return;
  }

  if (!in->wheel_speeds_valid) {
    out->faults.tc_input_fault = true;
    state->slip_integral = 0.0f;
    state->torque_limit_nm = unregulated_torque_nm;
    state->torque_limit_initialized = true;
    return;
  }

  float dt_s = (float)dt_ms / 1000.0f;
  tc_speed_estimate_t speed_estimate =
      estimate_speeds(in, state, params, dt_s);

  out->debug.tc_sensor_fault_flags = speed_estimate.sensor_fault_flags;
  out->debug.tc_front_disagreement_mps =
      speed_estimate.front_disagreement_mps;
  out->debug.tc_rear_disagreement_mps = speed_estimate.rear_disagreement_mps;
  out->debug.tc_motor_rear_disagreement_mps =
      speed_estimate.motor_rear_disagreement_mps;

  if (!speed_estimate.vehicle_speed_valid ||
      !speed_estimate.driven_speed_valid) {
    out->faults.tc_input_fault = true;
    state->slip_integral = 0.0f;
    state->torque_limit_nm = unregulated_torque_nm;
    state->torque_limit_initialized = true;
    return;
  }

  float raw_vehicle_speed_mps = speed_estimate.vehicle_speed_mps;
  raw_vehicle_speed_mps = blend_accel_prediction(
      raw_vehicle_speed_mps, state->previous_vehicle_speed_mps,
      in->longitudinal_accel_mps2, state->has_vehicle_speed_history,
      in->accel_valid, tc->tc_use_accel, tc->tc_reference_accel_blend, dt_s);

  float raw_driven_speed_mps = speed_estimate.driven_speed_mps;

  float speed_alpha =
      lpf_alpha_from_tau(dt_s, tc->tc_speed_lpf_time_constant_s);
  if (!state->vehicle_speed_initialized) {
    state->filtered_vehicle_speed_mps = raw_vehicle_speed_mps;
    state->vehicle_speed_initialized = true;
  } else {
    state->filtered_vehicle_speed_mps =
        lpf_step(state->filtered_vehicle_speed_mps, raw_vehicle_speed_mps,
                 speed_alpha);
  }

  if (!state->driven_speed_initialized) {
    state->filtered_driven_speed_mps = raw_driven_speed_mps;
    state->driven_speed_initialized = true;
  } else {
    state->filtered_driven_speed_mps =
        lpf_step(state->filtered_driven_speed_mps, raw_driven_speed_mps,
                 speed_alpha);
  }

  float min_slip_speed_mps = tc->tc_min_vehicle_speed_mps;
  float slip_denominator =
      fmaxf(state->filtered_vehicle_speed_mps, min_slip_speed_mps);
  float slip_ratio = (state->filtered_driven_speed_mps -
                      state->filtered_vehicle_speed_mps) /
                     slip_denominator;
  slip_ratio = fmaxf(slip_ratio, 0.0f);

  float slip_alpha =
      lpf_alpha_from_tau(dt_s, tc->tc_slip_lpf_time_constant_s);
  if (!state->slip_initialized) {
    state->filtered_slip_ratio = slip_ratio;
    state->slip_initialized = true;
  } else {
    state->filtered_slip_ratio =
        lpf_step(state->filtered_slip_ratio, slip_ratio, slip_alpha);
  }

  float lateral_usage = 0.0f;
  float lateral_accel_limit_mps2 = 0.0f;
  float effective_lateral_accel_mps2 = 0.0f;
  float target_slip =
      compute_target_slip(in, params, state->filtered_vehicle_speed_mps,
                          &lateral_usage, &lateral_accel_limit_mps2,
                          &effective_lateral_accel_mps2);
  float slip_error = state->filtered_slip_ratio - target_slip;
  float slip_hysteresis = tc->tc_slip_hysteresis;
  if (slip_error < slip_hysteresis) {
    slip_error = 0.0f;
  } else {
    slip_error -= slip_hysteresis;
  }

  float feedback_alpha =
      lpf_alpha_from_tau(dt_s, tc->tc_feedback_lpf_time_constant_s);
  if (!state->slip_error_initialized) {
    state->filtered_slip_error = slip_error;
    state->slip_error_initialized = true;
  } else {
    state->filtered_slip_error =
        lpf_step(state->filtered_slip_error, slip_error, feedback_alpha);
  }

  float slip_rate = 0.0f;
  if (dt_s > 0.0f && state->has_slip_history) {
    slip_rate = (state->filtered_slip_ratio - state->previous_slip_ratio) / dt_s;
  }
  state->previous_slip_ratio = state->filtered_slip_ratio;
  state->has_slip_history = true;

  float measured_longitudinal_accel_mps2 = 0.0f;
  if (in->accel_valid && tc->tc_use_accel) {
    measured_longitudinal_accel_mps2 = in->longitudinal_accel_mps2;
  } else if (dt_s > 0.0f && state->has_vehicle_speed_history) {
    measured_longitudinal_accel_mps2 =
        (state->filtered_vehicle_speed_mps -
         state->previous_vehicle_speed_mps) /
        dt_s;
  }
  float driven_accel_mps2 = 0.0f;
  if (dt_s > 0.0f && state->has_vehicle_speed_history) {
    driven_accel_mps2 =
        (state->filtered_driven_speed_mps - state->previous_driven_speed_mps) /
        dt_s;
  }
  float excess_accel_mps2 =
      fmaxf(driven_accel_mps2 - measured_longitudinal_accel_mps2, 0.0f);

  state->previous_vehicle_speed_mps = state->filtered_vehicle_speed_mps;
  state->previous_driven_speed_mps = state->filtered_driven_speed_mps;
  state->has_vehicle_speed_history = true;

  float min_torque_nm = tc->tc_min_torque_nm;
  bool above_enable_speed =
      state->filtered_vehicle_speed_mps >= min_slip_speed_mps;
  bool enough_torque = unregulated_torque_nm >= min_torque_nm;
  bool should_control = above_enable_speed && enough_torque;

  float torque_reduction_nm = 0.0f;
  if (should_control) {
    float integral_limit_nm = tc->tc_integral_limit_nm;
    if (dt_s > 0.0f) {
      state->slip_integral += state->filtered_slip_error * dt_s;
    }
    if (tc->tc_ki_nm_per_slip_s > 1.0e-6f) {
      float integral_bound = integral_limit_nm / tc->tc_ki_nm_per_slip_s;
      state->slip_integral =
          clamp_f(state->slip_integral, 0.0f, integral_bound);
    } else {
      state->slip_integral = 0.0f;
    }

    float p_nm = tc->tc_kp_nm_per_slip * state->filtered_slip_error;
    float i_nm = tc->tc_ki_nm_per_slip_s * state->slip_integral;
    float d_nm = tc->tc_kd_nm_per_slip_rate * fmaxf(slip_rate, 0.0f);
    float accel_nm =
        tc->tc_driven_accel_gain_nm_per_mps2 * excess_accel_mps2;
    torque_reduction_nm = p_nm + i_nm + d_nm + accel_nm;
  } else {
    state->slip_integral = 0.0f;
  }

  float max_reduction_nm = tc->tc_max_torque_reduction_nm;
  torque_reduction_nm =
      clamp_f(torque_reduction_nm, 0.0f,
              fminf(max_reduction_nm, unregulated_torque_nm));
  float target_torque_limit_nm = unregulated_torque_nm - torque_reduction_nm;

  if (!state->torque_limit_initialized) {
    state->torque_limit_nm = unregulated_torque_nm;
    state->torque_limit_initialized = true;
  }

  float cut_rate_nm_s = tc->tc_cut_slew_nm_per_s;
  float recovery_rate_nm_s = tc->tc_recovery_slew_nm_per_s;
  state->torque_limit_nm =
      rate_limit(state->torque_limit_nm, target_torque_limit_nm,
                 recovery_rate_nm_s, cut_rate_nm_s, dt_s);
  state->torque_limit_nm =
      clamp_f(state->torque_limit_nm, 0.0f, unregulated_torque_nm);

  out->torque_cmd = fminf(unregulated_torque_nm, state->torque_limit_nm);

  out->debug.tc_active =
      should_control && torque_reduction_nm > 1.0f &&
      out->torque_cmd < unregulated_torque_nm - 0.5f;
  out->debug.tc_vehicle_speed_mps = state->filtered_vehicle_speed_mps;
  out->debug.tc_driven_speed_mps = state->filtered_driven_speed_mps;
  out->debug.tc_slip_ratio = state->filtered_slip_ratio;
  out->debug.tc_target_slip_ratio = target_slip;
  out->debug.tc_slip_error = state->filtered_slip_error;
  out->debug.tc_torque_reduction_nm = unregulated_torque_nm - out->torque_cmd;
  out->debug.tc_torque_limit_nm = state->torque_limit_nm;
  out->debug.tc_lateral_usage = lateral_usage;
  out->debug.tc_lateral_accel_limit_mps2 = lateral_accel_limit_mps2;
  out->debug.tc_longitudinal_accel_mps2 = measured_longitudinal_accel_mps2;
  out->debug.tc_lateral_accel_mps2 = effective_lateral_accel_mps2;
}
