#include "vcu_model/components/TorqueMap.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <gtest/gtest.h>

namespace {

void set_valid_torque_map_params(vcu_parameters_t *params, float max_torque_nm) {
  params->torque_map.max_torque_nm = max_torque_nm;
  params->torque_map.pedal_exponential_factor = 0.0f;
  params->torque_map.power_limit_w = 80000.0f;
  params->torque_map.current_limit_a = 200.0f;
  params->torque_map.hard_current_cut_a = 240.0f;
  params->torque_map.hard_power_cut_w = 85000.0f;
  params->torque_map.ocv_cell_count = 128.0f;
  params->torque_map.ocv_lpf_time_constant_s = 1.0f;
  params->torque_map.current_lpf_time_constant_s = 0.2f;
  params->torque_map.measured_power_lpf_time_constant_s = 0.010f;
  params->torque_map.power_limit_min_rpm = 100.0f;
  params->torque_map.power_limit_trim_limit_nm = 20.0f;
  params->torque_map.power_limit_kp = 0.002f;
  params->torque_map.power_limit_ki = 0.0f;
  params->torque_map.power_limit_kd = 0.0f;
  params->torque_map.launch_mode_disable = true;
  const float rpm_map[VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS] = {
      0.0f,    550.0f,  1100.0f, 1650.0f, 2200.0f, 2750.0f,
      3300.0f, 3850.0f, 4400.0f, 4950.0f, 5500.0f,
  };
  const float efficiency_map[VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS] = {
      0.86f, 0.89f, 0.92f, 0.94f, 0.95f, 0.955f,
      0.955f, 0.95f, 0.945f, 0.93f, 0.90f,
  };
  for (int i = 0; i < VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS; ++i) {
    params->torque_map.power_limit_motor_efficiency_rpm[i] = rpm_map[i];
    params->torque_map.power_limit_motor_efficiency[i] = efficiency_map[i];
  }
}

} // namespace

class TorqueMapTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  torque_map_state_t state;

  void SetUp() override {
    params = {};
    set_valid_torque_map_params(&params, 100.0f);
    in = {0};
    in.battery_voltage_v = 500.0f;
    in.battery_current_a = 0.0f;
    in.battery_status_valid = true;
    in.inverter_dc_bus_voltage_v = 500.0f;
    in.inverter_dc_bus_current_a = 0.0f;
    in.inverter_power_valid = true;
    in.motor_speed_rpm = 0.0f;
    in.inverter_speed_valid = true;
    out = {0};
    torque_map_init(&state);
  }
};

static void enable_launch_mode(vcu_parameters_t *params) {
  params->torque_map.launch_mode_disable = false;
  params->torque_map.launch_enter_rpm = 10.0f;
  params->torque_map.launch_exit_rpm = 75.0f;
  params->torque_map.launch_pedal_min = 0.05f;
  params->torque_map.launch_pedal_max = 0.80f;
  params->torque_map.launch_brake_min_psi = 50.0f;
  params->torque_map.launch_preload_torque_nm = 5.0f;
  params->torque_map.launch_preload_ramp_rate_nm_per_s = 100.0f;
  params->torque_map.launch_release_ramp_rate_nm_per_s = 200.0f;
}

TEST_F(TorqueMapTest, BasicMapping) {
  // 0 pedal -> 0 torque
  out.accel_pedal_travel = 0.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);

  // 50% pedal -> 50 torque
  out.accel_pedal_travel = 0.5f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);

  // 100% pedal -> 100 torque
  out.accel_pedal_travel = 1.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 100.0f);
}

TEST_F(TorqueMapTest, UsesEfficiencyMapForPowerLimitAtHighSpeed) {
  params.torque_map.max_torque_nm = 230.0f;
  params.torque_map.power_limit_w = 70000.0f;
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.power_limit_ki = 0.0f;
  params.torque_map.power_limit_kd = 0.0f;

  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 520.0f;
  in.motor_speed_rpm = 3000.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  // 3000 rpm lands on the 0.955 efficiency plateau, so the available torque is
  // below the 2024 constant-100%-efficiency result.
  EXPECT_NEAR(out.debug.motor_efficiency, 0.955f, 0.001f);
  EXPECT_NEAR(out.torque_cmd, 212.8f, 0.6f);
}

TEST_F(TorqueMapTest, InterpolatesEfficiencyUsingConfiguredRpmBreakpoints) {
  params.torque_map.max_torque_nm = 230.0f;
  params.torque_map.power_limit_w = 70000.0f;
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.power_limit_ki = 0.0f;
  params.torque_map.power_limit_kd = 0.0f;
  params.torque_map.power_limit_motor_efficiency_rpm[5] = 2500.0f;
  params.torque_map.power_limit_motor_efficiency_rpm[6] = 3500.0f;
  params.torque_map.power_limit_motor_efficiency[5] = 0.90f;
  params.torque_map.power_limit_motor_efficiency[6] = 1.00f;

  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 520.0f;
  in.inverter_dc_bus_voltage_v = 520.0f;
  in.motor_speed_rpm = 3000.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_NEAR(out.debug.motor_efficiency, 0.95f, 0.001f);
}

TEST_F(TorqueMapTest, PedalScalesAcrossPowerLimitedAvailableTorque) {
  params.torque_map.max_torque_nm = 230.0f;
  params.torque_map.power_limit_w = 70000.0f;
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.power_limit_ki = 0.0f;
  params.torque_map.power_limit_kd = 0.0f;

  out.accel_pedal_travel = 0.5f;
  in.battery_voltage_v = 520.0f;
  in.motor_speed_rpm = 3000.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  // The pedal scales across the available power-limited torque so the full
  // pedal range stays responsive under derate.
  EXPECT_NEAR(out.torque_cmd, 106.4f, 0.6f);
}

TEST_F(TorqueMapTest, CurrentPowerLimitUsesInverterDcBusVoltageInput) {
  params.torque_map.max_torque_nm = 300.0f;
  params.torque_map.power_limit_w = 100000.0f;
  params.torque_map.current_limit_a = 100.0f;
  params.torque_map.hard_power_cut_w = 120000.0f;
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.power_limit_ki = 0.0f;
  params.torque_map.power_limit_kd = 0.0f;

  out.accel_pedal_travel = 0.0f;
  in.battery_voltage_v = 500.0f;
  in.battery_current_a = 0.0f;
  in.inverter_dc_bus_voltage_v = 500.0f;
  in.inverter_dc_bus_current_a = 0.0f;
  in.motor_speed_rpm = 3000.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);

  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 500.0f;
  in.battery_current_a = 0.0f;
  in.inverter_dc_bus_voltage_v = 300.0f;
  in.inverter_dc_bus_current_a = 1.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_NEAR(out.debug.active_power_limit_w, 30000.0f, 0.5f);
  EXPECT_NEAR(out.debug.measured_power_w, 300.0f, 0.5f);
  EXPECT_NEAR(out.torque_cmd, 91.2f, 0.6f);
}

TEST_F(TorqueMapTest, ExponentialPedalSoftensLowPedalTorque) {
  params.torque_map.max_torque_nm = 230.0f;
  params.torque_map.pedal_exponential_factor = 2.0f;
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.power_limit_ki = 0.0f;
  params.torque_map.power_limit_kd = 0.0f;

  out.accel_pedal_travel = 0.5f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_NEAR(out.torque_cmd, 61.9f, 0.5f);
}

TEST_F(TorqueMapTest, LaunchModePreloadsOnBrake) {
  enable_launch_mode(&params);
  out.accel_pedal_travel = 0.5f;
  out.brake_pressed = true;
  out.bse_psi = 100.0f;
  in.motor_speed_rpm = 0.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 1.0f);
  EXPECT_TRUE(out.debug.launch_active);
  EXPECT_EQ(out.debug.launch_state, TORQUE_MAP_LAUNCH_STATE_PRELOAD);
  EXPECT_NEAR(out.debug.launch_raw_torque_cmd_nm, 50.0f, 0.001f);
}

TEST_F(TorqueMapTest, LaunchModeCapsPreloadTorqueWhileBrakeHeld) {
  enable_launch_mode(&params);
  out.accel_pedal_travel = 0.5f;
  out.brake_pressed = true;
  out.bse_psi = 100.0f;
  in.motor_speed_rpm = 0.0f;

  for (int i = 0; i < 10; ++i) {
    torque_map_evaluate(&in, &out, &state, &params, 10);
  }

  EXPECT_FLOAT_EQ(out.torque_cmd, 5.0f);
  EXPECT_EQ(out.debug.launch_state, TORQUE_MAP_LAUNCH_STATE_PRELOAD);
}

TEST_F(TorqueMapTest, LaunchModeRampsTorqueWhenBrakeDrops) {
  enable_launch_mode(&params);
  out.accel_pedal_travel = 0.5f;
  out.brake_pressed = true;
  out.bse_psi = 100.0f;
  in.motor_speed_rpm = 0.0f;

  for (int i = 0; i < 10; ++i) {
    torque_map_evaluate(&in, &out, &state, &params, 10);
  }
  EXPECT_FLOAT_EQ(out.torque_cmd, 5.0f);

  out.brake_pressed = false;
  out.bse_psi = 0.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_GT(out.torque_cmd, 5.0f);
  EXPECT_LT(out.torque_cmd, 50.0f);
  EXPECT_EQ(out.debug.launch_state, TORQUE_MAP_LAUNCH_STATE_RELEASE);
}

TEST_F(TorqueMapTest, LaunchModeBypassesAboveExitSpeed) {
  enable_launch_mode(&params);
  out.accel_pedal_travel = 0.5f;
  out.brake_pressed = true;
  out.bse_psi = 100.0f;
  in.motor_speed_rpm = 80.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);
  EXPECT_FALSE(out.debug.launch_active);
  EXPECT_EQ(out.debug.launch_state, TORQUE_MAP_LAUNCH_STATE_INACTIVE);
}

TEST_F(TorqueMapTest, HardCurrentCutDisablesTorque) {
  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 300.0f;
  in.battery_current_a = 250.0f;
  in.inverter_dc_bus_voltage_v = 300.0f;
  in.inverter_dc_bus_current_a = 250.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.current_safety_cut);
}

TEST_F(TorqueMapTest, HardPowerCutDisablesTorque) {
  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 500.0f;
  in.battery_current_a = 180.0f;
  in.inverter_dc_bus_voltage_v = 500.0f;
  in.inverter_dc_bus_current_a = 180.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_safety_cut);
}

TEST_F(TorqueMapTest, MissingPowertrainInputsDisablesTorque) {
  out.accel_pedal_travel = 1.0f;
  in.inverter_power_valid = false;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_limit_input_fault);
}
