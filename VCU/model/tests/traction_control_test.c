/* Deterministic model tests; values are synthetic, never vehicle calibration. */
#include "TractionControl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static vcu_inputs_t in;
static vcu_outputs_t out;
static vcu_parameters_t p;
static traction_control_state_t s;

static void reset(tc_mode_t mode) {
  memset(&in, 0, sizeof(in));
  memset(&out, 0, sizeof(out));
  memset(&p, 0, sizeof(p));
#define P p.traction_control
  P.mode = mode;
  P.calibrated = true;
  for (unsigned i = 0; i < 4; ++i) P.wheel_radius_m[i] = 0.25f;
  P.wheelbase_m = 1.6f;
  P.front_track_m = 1.2f;
  P.rear_track_m = 1.2f;
  P.max_sample_age_us = 20000;
  P.max_sample_skew_us = 2000;
  P.max_step_us = 20000;
  P.max_wheel_speed_rad_s = 500.0f;
  P.max_yaw_rate_rad_s = 3.0f;
  P.max_steering_rad = 0.8f;
  P.max_front_disagreement_m_s = 0.5f;
  P.reverse_tolerance_m_s = 0.05f;
  P.slip_target_ratio = 0.1f;
  P.low_speed_slip_target_m_s = 0.1f;
  P.slip_blend_speed_m_s = 2.0f;
  P.slip_kp_nm_per_m_s = 30.0f;
  P.slip_ki_nm_per_m = 10.0f;
  P.integral_limit_nm = 80.0f;
  P.motor_torque_cap_nm = 100.0f;
  P.recovery_rate_nm_s = 1000.0f;
  P.fault_torque_cap_nm = 10.0f;
  P.launch_initial_torque_nm = 30.0f;
  P.launch_rise_rate_nm_s = 100.0f;
  P.launch_torque_cap_nm = 80.0f;
  P.launch_end_speed_m_s = 3.0f;
#undef P
  in.traction_control.drive_qualified = true;
  in.traction_control.geometry_valid = true;
  in.traction_control.motion_valid = true;
  for (unsigned i = 0; i < 4; ++i) {
    in.traction_control.wheel[i].valid = true;
    in.traction_control.wheel[i].synchronized = true;
  }
  traction_control_init(&s, &p);
}

static void fresh(float front_m_s, float rear_m_s) {
  in.traction_control.now_us += 10000;
  in.traction_control.motion_timestamp_us = in.traction_control.now_us;
  for (unsigned i = 0; i < 4; ++i) {
    in.traction_control.wheel[i].timestamp_us = in.traction_control.now_us;
    ++in.traction_control.wheel[i].sequence;
    in.traction_control.wheel[i].angular_speed_rad_s =
        (i < 2 ? front_m_s : rear_m_s) / p.traction_control.wheel_radius_m[i];
  }
}

static void evaluate(float demand) {
  memset(&out, 0, sizeof(out));
  out.torque_power_limited = demand;
  traction_control_evaluate(&in, &out, &s, &p, 10);
  assert(isfinite(out.torque_cmd));
  assert(out.torque_cmd >= 0.0f);
  if (isfinite(demand) && demand >= 0.0f) assert(out.torque_cmd <= demand);
}

static void prime(float speed) {
  for (unsigned i = 0; i < 20; ++i) { fresh(speed, speed); evaluate(100.0f); }
}

static void modes_and_stationary(void) {
  reset(TC_MODE_DISABLED);
  memset(&p, 0, sizeof(p)); /* Existing zero-filled/default model parameters. */
  p.traction_control.enabled = true; /* Legacy switch cannot enable new TC. */
  evaluate(73.0f);
  assert(out.torque_cmd == 73.0f && out.traction_control.state == TC_STATE_DISABLED);
  reset(TC_MODE_ACTIVE);
  fresh(0, 0); evaluate(100);
  assert(out.traction_control.estimate_valid && out.traction_control.fault_flags == 0);
  assert(out.traction_control.state == TC_STATE_LAUNCH);
  assert(out.traction_control.feedforward_ceiling_nm == 30.0f);
  assert(fabsf(out.torque_cmd - 10.0f) < 0.001f);
  for (unsigned i = 0; i < 100; ++i) {
    fresh(0, 0); evaluate(100);
    assert(out.torque_cmd <= 80.0f);
  }
  reset(TC_MODE_SHADOW);
  fresh(0, 3); evaluate(73);
  assert(out.torque_cmd == 73 && out.traction_control.candidate_torque_nm == 0);
  assert(out.traction_control.shadow);
}

static void slip_recovery_and_release(void) {
  reset(TC_MODE_ACTIVE); prime(10);
  assert(out.torque_cmd == 100);
  fresh(10, 13); evaluate(100);
  float reduced = out.torque_cmd;
  assert(reduced < 50 && out.traction_control.state == TC_STATE_LIMITING);
  /* Held-but-fresh measurements must not erase slip like the old derivative. */
  in.traction_control.now_us += 10000;
  evaluate(100);
  assert(out.torque_cmd <= reduced);
  fresh(10, 10); evaluate(100);
  assert(out.torque_cmd <= reduced + 10.001f);
  fresh(10, 10); evaluate(2);
  assert(out.torque_cmd <= 2);
  fresh(10, 10); evaluate(0);
  assert(out.torque_cmd == 0 && s.integral_trim_nm == 0);
  fresh(10, 10); evaluate(100);
  assert(out.torque_cmd <= 10.001f);
  in.traction_control.braking = true;
  fresh(10, 10); evaluate(100);
  assert(out.torque_cmd == 0 && out.traction_control.state == TC_STATE_INHIBITED);
  in.traction_control.braking = false;
  in.traction_control.drive_qualified = false;
  fresh(10, 10); evaluate(100);
  assert(out.torque_cmd == 0 && s.launch_pending && !s.seen_wheel[0]);
}

static void stale_frozen_and_recovery(void) {
  reset(TC_MODE_ACTIVE); prime(10);
  for (unsigned i = 0; i < 3; ++i) {
    in.traction_control.now_us += 10000;
    evaluate(100);
  }
  assert(out.traction_control.state == TC_STATE_DEGRADED);
  assert(out.traction_control.fault_flags & TC_FAULT_WHEEL);
  assert(out.torque_cmd <= 10);
  float held = out.torque_cmd;
  in.traction_control.now_us += 10000; evaluate(100);
  assert(out.torque_cmd <= held);
  fresh(10, 10); evaluate(100);
  assert(out.torque_cmd <= held + 10.001f);
  fresh(10, 10);
  --in.traction_control.wheel[TC_RL].sequence;
  evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_SEQUENCE);
  assert(out.torque_cmd <= 10);
}

static void validity_and_finite(void) {
  reset(TC_MODE_ACTIVE); prime(10);
  fresh(10, 10); in.traction_control.wheel[TC_FL].valid = false; evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_WHEEL);
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  in.traction_control.wheel[TC_FR].synchronized = false; evaluate(100);
  assert(out.torque_cmd == 0 && !out.traction_control.estimate_valid);
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  in.traction_control.wheel[TC_FL].angular_speed_rad_s = NAN; evaluate(100);
  assert(out.torque_cmd == 0 && (out.traction_control.fault_flags & TC_FAULT_WHEEL));
  fresh(10, 10); evaluate(100);
  assert(out.traction_control.estimate_valid && isfinite(s.integral_trim_nm));
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  in.traction_control.yaw_rate_rad_s = NAN; evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_MOTION);
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  p.traction_control.wheel_radius_m[0] = NAN; evaluate(100);
  assert(out.torque_cmd == 0 && (out.traction_control.fault_flags & TC_FAULT_CONFIG));
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  p.traction_control.calibrated = false; evaluate(100);
  assert(out.torque_cmd == 0);
  reset(TC_MODE_SHADOW); fresh(10, 10);
  p.traction_control.calibrated = false; evaluate(77);
  assert(out.torque_cmd == 77 && (out.traction_control.fault_flags & TC_FAULT_CONFIG));
  reset(TC_MODE_ACTIVE); fresh(10, 10); evaluate(NAN);
  assert(out.torque_cmd == 0 && (out.traction_control.fault_flags & TC_FAULT_DEMAND));
}

static void timing_and_wrap(void) {
  reset(TC_MODE_ACTIVE);
  in.traction_control.now_us = UINT32_MAX - 15000u;
  for (unsigned i = 0; i < 4; ++i) in.traction_control.wheel[i].sequence = UINT16_MAX - 1u;
  fresh(10, 10); evaluate(100);
  fresh(10, 10); evaluate(100);
  assert(out.traction_control.fault_flags == 0 && in.traction_control.wheel[0].sequence == 0);
  evaluate(100); /* repeated clock must not create a recovery step */
  assert(out.traction_control.fault_flags & TC_FAULT_TIMING);
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  in.traction_control.wheel[TC_FL].timestamp_us -= 3000; evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_TIMING);
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  in.traction_control.wheel[TC_FL].timestamp_us += 1; evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_WHEEL);
  reset(TC_MODE_ACTIVE); prime(10);
  in.traction_control.now_us += 1000000u; evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_TIMING);
}

static void cornering_and_front_consistency(void) {
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  float yaw = 0.7f, vx = 10.0f;
  in.traction_control.yaw_rate_rad_s = yaw;
  for (unsigned i = 0; i < 4; ++i) {
    float y = (i % 2 == 0 ? 0.5f : -0.5f) * 1.2f;
    float wheel_v = vx - yaw * y;
    if (i < 2) {
      float delta = atanf(yaw * 1.6f / wheel_v);
      in.traction_control.front_steering_rad[i] = delta;
      wheel_v = wheel_v * cosf(delta) + yaw * 1.6f * sinf(delta);
    }
    in.traction_control.wheel[i].angular_speed_rad_s = wheel_v / 0.25f;
  }
  evaluate(100);
  assert(out.traction_control.fault_flags == 0);
  assert(fabsf(out.traction_control.reference_speed_m_s - vx) < 0.0001f);
  assert(fabsf(out.traction_control.slip_velocity_m_s[0]) < 0.0001f);
  assert(fabsf(out.traction_control.slip_velocity_m_s[1]) < 0.0001f);
  reset(TC_MODE_ACTIVE); fresh(10, 10);
  in.traction_control.wheel[TC_FL].angular_speed_rad_s += 8; evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_FRONT_DISAGREEMENT);
  reset(TC_MODE_ACTIVE); fresh(-1, -1); evaluate(100);
  assert(out.traction_control.fault_flags & TC_FAULT_REVERSE);
}

static void antiwindup_and_single_rear_slip(void) {
  reset(TC_MODE_ACTIVE); prime(10);
  for (unsigned i = 0; i < 1000; ++i) {
    fresh(10, 10);
    in.traction_control.wheel[TC_RR].angular_speed_rad_s = 100;
    evaluate(100);
    assert(out.torque_cmd == 0 && s.integral_trim_nm == 0);
  }
  fresh(10, 10); evaluate(100);
  assert(out.torque_cmd <= 10.001f);
  for (unsigned i = 0; i < 100; ++i) { fresh(10, 11.5f); evaluate(100); }
  assert(s.integral_trim_nm > 0);
  for (unsigned i = 0; i < 100; ++i) { fresh(10, 10); evaluate(100); }
  assert(s.integral_trim_nm == 0 && out.torque_cmd == 100);
}

int main(void) {
  modes_and_stationary();
  slip_recovery_and_release();
  stale_frozen_and_recovery();
  validity_and_finite();
  timing_and_wrap();
  cornering_and_front_consistency();
  antiwindup_and_single_rear_slip();
  puts("TC: 7 scenario groups passed (modes, launch, slip, recovery, faults, timing, geometry, antiwindup).");
  return 0;
}
