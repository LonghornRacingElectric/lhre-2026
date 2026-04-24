#include "vcu_model/components/TractionControl.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace {

void set_valid_traction_control_params(vcu_parameters_t *params) {
  params->traction_control.tc_disable = false;
  params->traction_control.tc_wheel_radius_m = 0.30f;
  params->traction_control.tc_final_drive_ratio = 4.0f;
  params->traction_control.tc_base_target_slip = 0.08f;
  params->traction_control.tc_min_target_slip = 0.03f;
  params->traction_control.tc_max_target_slip = 0.16f;
  params->traction_control.tc_slip_hysteresis = 0.0f;
  params->traction_control.tc_lateral_accel_limit_mps2 = 11.0f;
  params->traction_control.tc_aero_lateral_accel_gain_per_mps2 = 0.0f;
  params->traction_control.tc_lateral_slip_reduction_gain = 1.0f;
  params->traction_control.tc_min_vehicle_speed_mps = 2.0f;
  params->traction_control.tc_min_torque_nm = 1.0f;
  params->traction_control.tc_max_wheel_speed_mps = 90.0f;
  params->traction_control.tc_max_reference_accel_mps2 = 1000.0f;
  params->traction_control.tc_front_disagreement_mps = 3.0f;
  params->traction_control.tc_rear_disagreement_mps = 8.0f;
  params->traction_control.tc_motor_rear_disagreement_mps = 8.0f;
  params->traction_control.tc_speed_lpf_time_constant_s = 0.0f;
  params->traction_control.tc_slip_lpf_time_constant_s = 0.0f;
  params->traction_control.tc_feedback_lpf_time_constant_s = 0.0f;
  params->traction_control.tc_reference_accel_blend = 0.0f;
  params->traction_control.tc_kp_nm_per_slip = 1000.0f;
  params->traction_control.tc_ki_nm_per_slip_s = 0.0f;
  params->traction_control.tc_kd_nm_per_slip_rate = 0.0f;
  params->traction_control.tc_driven_accel_gain_nm_per_mps2 = 0.0f;
  params->traction_control.tc_integral_limit_nm = 40.0f;
  params->traction_control.tc_max_torque_reduction_nm = 220.0f;
  params->traction_control.tc_cut_slew_nm_per_s = 100000.0f;
  params->traction_control.tc_recovery_slew_nm_per_s = 100000.0f;
}

} // namespace

class TractionControlTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  traction_control_state_t state;

  void SetUp() override {
    params = {};
    set_valid_traction_control_params(&params);

    in = {};
    in.wheel_speeds_valid = true;
    in.inverter_speed_valid = true;
    setVehicleAndDrivenSpeed(20.0f, 20.0f);

    out = {};
    out.torque_cmd = 100.0f;
    traction_control_init(&state);
  }

  void setVehicleAndDrivenSpeed(float vehicle_mps, float driven_mps) {
    float front_rad_s = vehicle_mps / params.traction_control.tc_wheel_radius_m;
    float rear_rad_s = driven_mps / params.traction_control.tc_wheel_radius_m;
    in.wheel_speed_fl_rad_s = front_rad_s;
    in.wheel_speed_fr_rad_s = front_rad_s;
    in.wheel_speed_rl_rad_s = rear_rad_s;
    in.wheel_speed_rr_rad_s = rear_rad_s;

    float motor_rad_s =
        rear_rad_s * params.traction_control.tc_final_drive_ratio;
    in.motor_speed_rpm = motor_rad_s * 60.0f / 6.28318530718f;
  }
};

TEST_F(TractionControlTest, DisabledPassesTorqueThrough) {
  params.traction_control.tc_disable = true;
  in.wheel_speeds_valid = false;

  traction_control_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 100.0f);
  EXPECT_FALSE(out.faults.tc_input_fault);
}

TEST_F(TractionControlTest, MissingWheelSpeedsFailsOpen) {
  in.wheel_speeds_valid = false;

  traction_control_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 100.0f);
  EXPECT_TRUE(out.faults.tc_input_fault);
}

TEST_F(TractionControlTest, ImplausibleReferenceSpeedsFailOpen) {
  in.wheel_speed_fl_rad_s = 500.0f;
  in.wheel_speed_fr_rad_s = 500.0f;

  traction_control_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 100.0f);
  EXPECT_TRUE(out.faults.tc_input_fault);
  EXPECT_TRUE(out.debug.tc_sensor_fault_flags & TC_SENSOR_NO_REFERENCE_SPEED);
}

TEST_F(TractionControlTest, MatchingDrivenAndReferenceSpeedPassesTorque) {
  traction_control_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 100.0f);
  EXPECT_NEAR(out.debug.tc_slip_ratio, 0.0f, 0.001f);
}

TEST_F(TractionControlTest, FrontDisagreementUsesPlausibleReference) {
  params.traction_control.tc_max_reference_accel_mps2 = 10000.0f;
  traction_control_evaluate(&in, &out, &state, &params, 10);

  out = {};
  out.torque_cmd = 100.0f;
  in.wheel_speed_fl_rad_s = 20.0f / params.traction_control.tc_wheel_radius_m;
  in.wheel_speed_fr_rad_s = 35.0f / params.traction_control.tc_wheel_radius_m;
  in.wheel_speed_rl_rad_s = 22.0f / params.traction_control.tc_wheel_radius_m;
  in.wheel_speed_rr_rad_s = 22.0f / params.traction_control.tc_wheel_radius_m;

  traction_control_evaluate(&in, &out, &state, &params, 10);

  EXPECT_TRUE(out.debug.tc_sensor_fault_flags & TC_SENSOR_FRONT_DISAGREE);
  EXPECT_NEAR(out.debug.tc_vehicle_speed_mps, 20.0f, 0.1f);
}

TEST_F(TractionControlTest, ImplausibleReferenceSpikeDoesNotBecomeHistory) {
  params.traction_control.tc_max_reference_accel_mps2 = 35.0f;
  traction_control_evaluate(&in, &out, &state, &params, 10);

  setVehicleAndDrivenSpeed(35.0f, 35.0f);

  out = {};
  out.torque_cmd = 100.0f;
  traction_control_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.faults.tc_input_fault);
  EXPECT_TRUE(out.debug.tc_sensor_fault_flags &
              TC_SENSOR_REFERENCE_ACCEL_IMPLAUS);

  out = {};
  out.torque_cmd = 100.0f;
  traction_control_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.faults.tc_input_fault);
  EXPECT_TRUE(out.debug.tc_sensor_fault_flags &
              TC_SENSOR_REFERENCE_ACCEL_IMPLAUS);
}

TEST_F(TractionControlTest, ExcessDrivenWheelSpeedReducesTorque) {
  setVehicleAndDrivenSpeed(20.0f, 25.0f);

  traction_control_evaluate(&in, &out, &state, &params, 10);

  EXPECT_LT(out.torque_cmd, 5.0f);
  EXPECT_TRUE(out.debug.tc_active);
  EXPECT_GT(out.debug.tc_slip_ratio, out.debug.tc_target_slip_ratio);
}

TEST_F(TractionControlTest, LateralAccelerationTightensSlipTarget) {
  traction_control_state_t no_lateral_state;
  traction_control_state_t lateral_state;
  traction_control_init(&no_lateral_state);
  traction_control_init(&lateral_state);

  params.traction_control.tc_use_accel = true;
  params.traction_control.tc_lateral_accel_limit_mps2 = 10.0f;
  params.traction_control.tc_lateral_slip_reduction_gain = 1.0f;
  setVehicleAndDrivenSpeed(20.0f, 21.8f);

  vcu_outputs_t no_lateral_out = out;
  in.accel_valid = false;
  traction_control_evaluate(&in, &no_lateral_out, &no_lateral_state, &params,
                            10);

  vcu_outputs_t lateral_out = out;
  in.accel_valid = true;
  in.lateral_accel_mps2 = 10.0f;
  traction_control_evaluate(&in, &lateral_out, &lateral_state, &params, 10);

  EXPECT_LT(lateral_out.debug.tc_target_slip_ratio,
            no_lateral_out.debug.tc_target_slip_ratio);
  EXPECT_LT(lateral_out.torque_cmd, no_lateral_out.torque_cmd);
}

TEST_F(TractionControlTest, AeroLateralLimitReducesHighSpeedCornerIntervention) {
  traction_control_state_t no_aero_state;
  traction_control_state_t aero_state;
  traction_control_init(&no_aero_state);
  traction_control_init(&aero_state);

  params.traction_control.tc_use_accel = true;
  params.traction_control.tc_lateral_accel_limit_mps2 = 10.0f;
  params.traction_control.tc_aero_lateral_accel_gain_per_mps2 = 0.004f;
  setVehicleAndDrivenSpeed(35.0f, 38.5f);
  in.accel_valid = true;
  in.lateral_accel_mps2 = 9.0f;

  vcu_outputs_t no_aero_out = out;
  params.traction_control.tc_aero_lateral_limit_enable = false;
  traction_control_evaluate(&in, &no_aero_out, &no_aero_state, &params, 10);

  vcu_outputs_t aero_out = out;
  params.traction_control.tc_aero_lateral_limit_enable = true;
  traction_control_evaluate(&in, &aero_out, &aero_state, &params, 10);

  EXPECT_GT(aero_out.debug.tc_lateral_accel_limit_mps2,
            no_aero_out.debug.tc_lateral_accel_limit_mps2);
  EXPECT_GT(aero_out.debug.tc_target_slip_ratio,
            no_aero_out.debug.tc_target_slip_ratio);
  EXPECT_GT(aero_out.torque_cmd, no_aero_out.torque_cmd);
}

TEST_F(TractionControlTest, ReplayTraceCutsAndRecoversSmoothly) {
  struct Sample {
    float vehicle_mps;
    float driven_mps;
    float torque_nm;
  };

  const Sample trace[] = {
      {20.0f, 20.5f, 120.0f}, {20.5f, 21.0f, 120.0f},
      {21.0f, 24.5f, 120.0f}, {21.5f, 26.0f, 120.0f},
      {22.0f, 24.0f, 120.0f}, {22.5f, 23.0f, 120.0f},
      {23.0f, 23.2f, 120.0f}, {23.5f, 23.7f, 120.0f},
  };

  params.traction_control.tc_speed_lpf_time_constant_s = 0.020f;
  params.traction_control.tc_slip_lpf_time_constant_s = 0.010f;
  params.traction_control.tc_feedback_lpf_time_constant_s = 0.010f;
  params.traction_control.tc_cut_slew_nm_per_s = 1500.0f;
  params.traction_control.tc_recovery_slew_nm_per_s = 300.0f;

  float min_torque = 1000.0f;
  float final_torque = 0.0f;
  for (const Sample &sample : trace) {
    setVehicleAndDrivenSpeed(sample.vehicle_mps, sample.driven_mps);
    out = {};
    out.torque_cmd = sample.torque_nm;
    traction_control_evaluate(&in, &out, &state, &params, 10);
    min_torque = std::min(min_torque, out.torque_cmd);
    final_torque = out.torque_cmd;
  }

  EXPECT_LT(min_torque, 120.0f);
  EXPECT_GT(final_torque, min_torque);
}
