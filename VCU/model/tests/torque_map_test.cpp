#include "vcu_model/components/TorqueMap.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <cmath>
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
    params.torque_map.power_limit_w = 80000.0f;
    params.torque_map.current_limit_a = 300.0f;
    params.torque_map.hard_current_cut_a = 350.0f;
    params.torque_map.hard_power_cut_w = 90000.0f;
    params.torque_map.ocv_cell_count = 130.0f;
    params.torque_map.ocv_lpf_time_constant_s = 1.0f;
    params.torque_map.power_limit_min_rpm = 100.0f;
    params.torque_map.power_limit_trim_limit_nm = 20.0f;
    params.torque_map.power_limit_kp = 0.0f;
    params.torque_map.power_limit_ki = 0.0f;
    params.torque_map.power_limit_kd = 0.0f;
    for (int i = 0; i < VCU_TORQUE_MAP_EFFICIENCY_MAP_POINTS; i++) {
      params.torque_map.power_limit_motor_efficiency_rpm[i] =
          static_cast<float>(i) * 500.0f;
      params.torque_map.power_limit_motor_efficiency[i] = 1.0f;
    }

    in = {0};
    in.inverter_dc_bus_voltage_v = 400.0f;
    in.inverter_dc_bus_current_a = 0.0f;
    in.motor_speed_rpm = 1000.0f;
    in.battery_voltage_v = 520.0f;
    in.battery_status_valid = true;
    in.inverter_power_valid = true;
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

TEST_F(TorqueMapTest, FeedforwardCapsTorqueByPowerAndSpeed) {
  params.torque_map.max_torque_nm = 220.0f;
  params.torque_map.power_limit_w = 30000.0f;
  params.torque_map.current_limit_a = 300.0f;
  in.motor_speed_rpm = 3000.0f;
  out.accel_pedal_travel = 1.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  const float expected_torque =
      30000.0f / (3000.0f * 2.0f * static_cast<float>(M_PI) / 60.0f);
  EXPECT_NEAR(out.torque_cmd, expected_torque, 0.05f);
  EXPECT_NEAR(out.debug.power_limit_feedforward_torque_nm, expected_torque,
              0.05f);
}

TEST_F(TorqueMapTest, TrimPidReducesTorqueWhenMeasuredPowerIsHigh) {
  params.torque_map.max_torque_nm = 220.0f;
  params.torque_map.power_limit_w = 30000.0f;
  params.torque_map.current_limit_a = 300.0f;
  params.torque_map.power_limit_kp = 0.002f;
  in.motor_speed_rpm = 3000.0f;
  in.inverter_dc_bus_voltage_v = 300.0f;
  in.inverter_dc_bus_current_a = 120.0f;
  out.accel_pedal_travel = 1.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  const float feedforward =
      30000.0f / (3000.0f * 2.0f * static_cast<float>(M_PI) / 60.0f);
  EXPECT_NEAR(out.debug.power_limit_feedback_p_nm, -12.0f, 0.05f);
  EXPECT_NEAR(out.torque_cmd, feedforward - 12.0f, 0.05f);
}

TEST_F(TorqueMapTest, InvalidPowerInputsFailClosed) {
  in.inverter_power_valid = false;
  out.accel_pedal_travel = 1.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_limit_input_fault);
}

TEST_F(TorqueMapTest, HardPowerCutFailsClosed) {
  params.torque_map.hard_power_cut_w = 85000.0f;
  in.inverter_dc_bus_voltage_v = 400.0f;
  in.inverter_dc_bus_current_a = 220.0f;
  out.accel_pedal_travel = 1.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_safety_cut);
}

TEST_F(TorqueMapTest, HardCurrentCutUsesLiveBusCurrent) {
  params.torque_map.current_limit_a = 150.0f;
  params.torque_map.hard_current_cut_a = 200.0f;
  params.torque_map.hard_power_cut_w = 90000.0f;
  in.inverter_dc_bus_voltage_v = 300.0f;
  in.inverter_dc_bus_current_a = 201.0f;
  out.accel_pedal_travel = 1.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.current_safety_cut);
}
