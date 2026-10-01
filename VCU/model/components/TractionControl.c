#include "TractionControl.h"

#include <math.h>
#include <string.h>

static float bound(float x, float low, float high) {
  return fminf(fmaxf(x, low), high);
}

static bool range(float x, float low, float high) {
  return isfinite(x) && x >= low && x <= high;
}

static bool config_valid(const vcu_parameters_t *p) {
#define P p->traction_control
  if (!P.calibrated || !range(P.wheelbase_m, 0.1f, 10.0f) ||
      !range(P.front_track_m, 0.1f, 10.0f) ||
      !range(P.rear_track_m, 0.1f, 10.0f)) return false;
  for (unsigned i = 0; i < 4; ++i)
    if (!range(P.wheel_radius_m[i], 0.05f, 1.0f)) return false;
  return P.max_sample_age_us > 0 && P.max_sample_age_us <= 1000000u &&
      P.max_sample_skew_us <= P.max_sample_age_us &&
      P.max_step_us > 0 && P.max_step_us <= 100000u &&
      range(P.max_wheel_speed_rad_s, 1.0f, 10000.0f) &&
      range(P.max_yaw_rate_rad_s, 0.01f, 100.0f) &&
      range(P.max_steering_rad, 0.01f, 1.2f) &&
      range(P.max_front_disagreement_m_s, 0.001f, 20.0f) &&
      range(P.reverse_tolerance_m_s, 0.0f, 1.0f) &&
      range(P.slip_target_ratio, 0.0f, 1.0f) &&
      range(P.low_speed_slip_target_m_s, 0.0f, 20.0f) &&
      range(P.slip_blend_speed_m_s, 0.01f, 100.0f) &&
      range(P.slip_kp_nm_per_m_s, 0.0f, 100000.0f) &&
      range(P.slip_ki_nm_per_m, 0.0f, 100000.0f) &&
      range(P.integral_limit_nm, 0.0f, 10000.0f) &&
      range(P.motor_torque_cap_nm, 0.01f, 10000.0f) &&
      range(P.recovery_rate_nm_s, 0.01f, 1000000.0f) &&
      range(P.fault_torque_cap_nm, 0.0f, P.motor_torque_cap_nm) &&
      range(P.launch_initial_torque_nm, 0.0f, P.motor_torque_cap_nm) &&
      range(P.launch_rise_rate_nm_s, 0.0f, 1000000.0f) &&
      range(P.launch_torque_cap_nm, P.launch_initial_torque_nm,
            P.motor_torque_cap_nm) &&
      range(P.launch_end_speed_m_s, 0.01f, 100.0f);
#undef P
}

void traction_control_init(traction_control_state_t *state,
                           vcu_parameters_t *params) {
  (void)params;
  memset(state, 0, sizeof(*state));
  state->launch_pending = true;
}

/* No acceleration extrapolation is invented here. A bounded skew is a
 * precondition; max_front_disagreement includes radius, timing and tire error.
 * This kinematic estimate assumes negligible lateral speed at the rear axle.
 * Disable/derate the estimator if that assumption cannot be qualified. */
static uint32_t estimate(const tc_inputs_t *in, vcu_outputs_t *out,
                         traction_control_state_t *s,
                         const vcu_parameters_t *p) {
#define P p->traction_control
  uint32_t faults = 0;
  uint32_t min_age = UINT32_MAX, max_age = 0;
  float wheel_v[4];
  for (unsigned i = 0; i < 4; ++i) {
    const tc_wheel_input_t *w = &in->wheel[i];
    uint32_t age = in->now_us - w->timestamp_us;
    if (!w->valid || !w->synchronized || age > P.max_sample_age_us ||
        !range(w->angular_speed_rad_s, -P.max_wheel_speed_rad_s,
               P.max_wheel_speed_rad_s)) faults |= TC_FAULT_WHEEL;
    if (age < min_age) min_age = age;
    if (age > max_age) max_age = age;
    wheel_v[i] = w->angular_speed_rad_s * P.wheel_radius_m[i];
    if (wheel_v[i] < -P.reverse_tolerance_m_s) faults |= TC_FAULT_REVERSE;
    if (s->seen_wheel[i]) {
      uint16_t delta_seq = (uint16_t)(w->sequence - s->last_sequence[i]);
      uint32_t delta_time = w->timestamp_us - s->last_timestamp_us[i];
      if ((delta_seq == 0 && delta_time != 0) ||
          (delta_seq != 0 && (delta_seq >= 0x8000u || delta_time == 0 ||
                             delta_time >= 0x80000000u)))
        faults |= TC_FAULT_SEQUENCE;
    }
    /* Commit only individually plausible observations. A sensor reboot needs
     * a park/re-arm cycle, rather than quietly accepting its clock epoch. */
    if (!(faults & (TC_FAULT_WHEEL | TC_FAULT_SEQUENCE))) {
      s->last_sequence[i] = w->sequence;
      s->last_timestamp_us[i] = w->timestamp_us;
      s->seen_wheel[i] = true;
    }
  }
  uint32_t motion_age = in->now_us - in->motion_timestamp_us;
  if (motion_age < min_age) min_age = motion_age;
  if (motion_age > max_age) max_age = motion_age;
  if (max_age - min_age > P.max_sample_skew_us) faults |= TC_FAULT_TIMING;
  if (!in->geometry_valid || !in->motion_valid ||
      motion_age > P.max_sample_age_us ||
      !range(in->yaw_rate_rad_s, -P.max_yaw_rate_rad_s, P.max_yaw_rate_rad_s) ||
      !range(in->front_steering_rad[0], -P.max_steering_rad, P.max_steering_rad) ||
      !range(in->front_steering_rad[1], -P.max_steering_rad, P.max_steering_rad))
    faults |= TC_FAULT_MOTION;
  if (faults != 0) return faults;

  float front_v[2];
  for (unsigned i = 0; i < 2; ++i) {
    float y = (i == TC_FL ? 0.5f : -0.5f) * P.front_track_m;
    float delta = in->front_steering_rad[i];
    front_v[i] = (wheel_v[i] - in->yaw_rate_rad_s * P.wheelbase_m * sinf(delta)) /
                 cosf(delta) + in->yaw_rate_rad_s * y;
  }
  if (fabsf(front_v[0] - front_v[1]) > P.max_front_disagreement_m_s)
    return TC_FAULT_FRONT_DISAGREEMENT;
  float vx = 0.5f * (front_v[0] + front_v[1]);
  if (vx < -P.reverse_tolerance_m_s) return TC_FAULT_REVERSE;
  out->traction_control.reference_speed_m_s = fmaxf(vx, 0.0f);
  for (unsigned i = 0; i < 2; ++i) {
    float y = (i == 0 ? 0.5f : -0.5f) * P.rear_track_m;
    out->traction_control.slip_velocity_m_s[i] =
        wheel_v[i + 2] - (vx - in->yaw_rate_rad_s * y);
  }
  out->traction_control.estimate_valid = true;
  return 0;
#undef P
}

void traction_control_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                               traction_control_state_t *s,
                               vcu_parameters_t *p, uint32_t dt_ms) {
#define P p->traction_control
#define O out->traction_control
  memset(&O, 0, sizeof(O));
  bool demand_valid = isfinite(out->torque_power_limited) &&
                      out->torque_power_limited >= 0.0f;
  float demand = demand_valid ? out->torque_power_limited : 0.0f;
  out->torque_cmd = demand;
  if (P.mode == TC_MODE_DISABLED) {
    traction_control_init(s, p);
    O.state = TC_STATE_DISABLED;
    O.candidate_torque_nm = demand;
    O.torque_ceiling_nm = demand;
    if (!demand_valid) O.fault_flags = TC_FAULT_DEMAND;
    return;
  }
  if (s->previous_mode != P.mode) traction_control_init(s, p);
  s->previous_mode = P.mode;
  O.shadow = P.mode == TC_MODE_SHADOW;
  uint32_t faults = demand_valid ? 0 : TC_FAULT_DEMAND;
  bool config_ok = config_valid(p) &&
                   (P.mode == TC_MODE_SHADOW || P.mode == TC_MODE_ACTIVE);
  if (!config_ok) faults |= TC_FAULT_CONFIG;
  uint32_t elapsed_us = s->clock_initialized
      ? in->traction_control.now_us - s->last_now_us
      : (dt_ms <= 100u ? dt_ms * 1000u : UINT32_MAX);
  s->clock_initialized = true;
  s->last_now_us = in->traction_control.now_us;
  if (elapsed_us == 0 || elapsed_us > P.max_step_us) faults |= TC_FAULT_TIMING;
  float dt_s = (float)elapsed_us * 0.000001f;

  if (!in->traction_control.drive_qualified || in->traction_control.braking ||
      demand <= 0.0f) {
    /* Drop propulsion immediately. Regen arbitration executes separately. */
    s->integral_trim_nm = 0.0f;
    s->torque_ceiling_nm = 0.0f;
    s->ceiling_initialized = true;
    s->launch_elapsed_s = 0.0f;
    s->launch_pending = true;
    s->launch_active = false;
    if (!in->traction_control.drive_qualified)
      memset(s->seen_wheel, 0, sizeof(s->seen_wheel));
    O.state = TC_STATE_INHIBITED;
  } else {
    if (config_ok) faults |= estimate(&in->traction_control, out, s, p);
    if (faults != 0) {
      float fault_cap = config_ok ? P.fault_torque_cap_nm : 0.0f;
      s->torque_ceiling_nm = s->ceiling_initialized
          ? fminf(s->torque_ceiling_nm, fault_cap) : 0.0f;
      s->ceiling_initialized = true;
      O.state = TC_STATE_DEGRADED;
      /* Hold integral and launch clock; never recover torque on invalid data. */
    } else {
      float vx = O.reference_speed_m_s;
      if (s->launch_pending) {
        s->launch_active = vx < P.launch_end_speed_m_s;
        s->launch_pending = false;
      }
      if (vx >= P.launch_end_speed_m_s) s->launch_active = false;
      float feedforward = P.motor_torque_cap_nm;
      if (s->launch_active) {
        feedforward = fminf(P.launch_initial_torque_nm +
                            P.launch_rise_rate_nm_s * s->launch_elapsed_s,
                            P.launch_torque_cap_nm);
        /* Saturate elapsed time to avoid unbounded float state growth. */
        s->launch_elapsed_s = fminf(s->launch_elapsed_s + dt_s, 3600.0f);
      }
      O.feedforward_ceiling_nm = feedforward;
      float blend = bound(vx / P.slip_blend_speed_m_s, 0.0f, 1.0f);
      O.target_slip_velocity_m_s = (1.0f - blend) * P.low_speed_slip_target_m_s +
                                  blend * P.slip_target_ratio * vx;
      float slip = fmaxf(O.slip_velocity_m_s[0], O.slip_velocity_m_s[1]);
      float error = slip - O.target_slip_velocity_m_s;
      float base = fminf(demand, feedforward);
      float proportional = P.slip_kp_nm_per_m_s * error;
      float proposed_i = bound(s->integral_trim_nm + P.slip_ki_nm_per_m * error * dt_s,
                               0.0f, P.integral_limit_nm);
      /* Conditional integration: no positive windup when the command is
       * already at zero; negative error always permits unwinding. */
      if (error <= 0.0f || proportional + proposed_i < base)
        s->integral_trim_nm = proposed_i;
      float trim = bound(proportional + s->integral_trim_nm, 0.0f, base);
      float desired = base - trim;
      if (!s->ceiling_initialized) {
        /* A fresh active mode starts at zero and rises at the calibrated rate. */
        s->torque_ceiling_nm = 0.0f;
        s->ceiling_initialized = true;
      }
      float recovery_cap = s->torque_ceiling_nm + P.recovery_rate_nm_s * dt_s;
      s->torque_ceiling_nm = fminf(desired, recovery_cap);
      O.state = trim > 0.0f ? TC_STATE_LIMITING :
                s->launch_active ? TC_STATE_LAUNCH : TC_STATE_TRACKING;
    }
  }
  s->torque_ceiling_nm = bound(s->torque_ceiling_nm, 0.0f, demand);
  O.torque_ceiling_nm = s->torque_ceiling_nm;
  O.candidate_torque_nm = fminf(demand, s->torque_ceiling_nm);
  O.trim_torque_nm = demand - O.candidate_torque_nm;
  O.integral_trim_nm = s->integral_trim_nm;
  O.fault_flags = faults;
  if (!O.shadow) out->torque_cmd = O.candidate_torque_nm;
#undef P
#undef O
}
