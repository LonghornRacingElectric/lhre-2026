#include "vcu_model/components/TorqueMap.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <gtest/gtest.h>

class TorqueMapTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  torque_map_state_t state;

  void SetUp() override {
    params = {};
    params.torque_map.max_torque_nm = 100.0f;
    in = {0};
    in.battery_voltage_v = 500.0f;
    in.battery_current_a = 0.0f;
    in.battery_status_valid = true;
    in.motor_speed_rpm = 0.0f;
    in.inverter_speed_valid = true;
    out = {0};
    torque_map_init(&state);
  }
};

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

TEST_F(TorqueMapTest, PedalRequestsTorqueUntilPowerLimit) {
  params.torque_map.max_torque_nm = 230.0f;
  params.torque_map.power_limit_w = 70000.0f;
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.power_limit_ki = 0.0f;
  params.torque_map.power_limit_kd = 0.0f;

  out.accel_pedal_travel = 0.5f;
  in.battery_voltage_v = 520.0f;
  in.motor_speed_rpm = 3000.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  // The pedal maps to an actual torque request. It should not become 50% of
  // the available power-limited torque, which would make the pedal feel dead.
  EXPECT_NEAR(out.torque_cmd, 115.0f, 0.5f);
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

TEST_F(TorqueMapTest, HardCurrentCutDisablesTorque) {
  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 300.0f;
  in.battery_current_a = 250.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.current_safety_cut);
}

TEST_F(TorqueMapTest, HardPowerCutDisablesTorque) {
  out.accel_pedal_travel = 1.0f;
  in.battery_voltage_v = 500.0f;
  in.battery_current_a = 180.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_safety_cut);
}

TEST_F(TorqueMapTest, MissingPowertrainInputsDisablesTorque) {
  out.accel_pedal_travel = 1.0f;
  in.battery_status_valid = false;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_limit_input_fault);
}
