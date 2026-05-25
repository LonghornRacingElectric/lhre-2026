#include "vcu_model/components/TorqueMap.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <cstring>
#include <gtest/gtest.h>

class TorqueMapTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  torque_map_state_t state;

  void SetUp() override {
    memset(&params, 0, sizeof(params));

    params.torque_map.max_torque_nm = 220.0f;
    params.torque_map.pedal_exponent = 1.0f;
    params.torque_map.power_limit_w = 78000.0f;
    params.torque_map.current_limit_a = 200.0f;
    params.torque_map.hard_current_cut_a = 240.0f;
    params.torque_map.hard_power_cut_w = 80000.0f;
    params.torque_map.ocv_lpf_time_constant_s = 1.0f;
    params.torque_map.min_cell_ocv_derate_start_v = 3.2f;
    params.torque_map.min_cell_ocv_derate_cutoff_v = 3.0f;
    params.torque_map.power_limit_trim_limit_nm = 20.0f;
    params.torque_map.power_limit_kp = 0.002f;
    params.torque_map.power_limit_ki = 0.0f;
    params.torque_map.power_limit_kd = 0.0f;

    const float rpm[VCU_POWER_LIMIT_TORQUE_MAP_POINTS] = {
        0.0f,    500.0f,  1000.0f, 1500.0f, 2000.0f,
        2500.0f, 3000.0f, 3500.0f, 4000.0f, 4500.0f,
        5000.0f, 5500.0f, 6000.0f,
    };
    const float torque[VCU_POWER_LIMIT_TORQUE_MAP_POINTS] = {
        220.0f, 220.0f, 220.0f, 220.0f, 220.0f,
        220.0f, 220.0f, 200.0f, 173.0f, 151.0f,
        134.0f, 120.0f, 107.0f,
    };
    memcpy(params.torque_map.power_limit_torque_rpm, rpm, sizeof(rpm));
    memcpy(params.torque_map.power_limit_torque_nm, torque, sizeof(torque));

    in = {0};
    out = {0};
    in.inverter_power_valid = true;
    in.inverter_speed_valid = true;
    in.inverter_dc_bus_voltage_v = 400.0f;
    in.inverter_dc_bus_current_a = 0.0f;
    in.battery_status_valid = true;
    in.battery_voltage_v = 455.0f;
    in.min_cell_voltage_valid = true;
    in.min_cell_voltage_v = 3.5f;

    torque_map_init(&state, &params);
  }
};

TEST_F(TorqueMapTest, PedalRequestsPercentOfAvailableTorque) {
  params.torque_map.power_limit_kp = 0.0f;

  out.accel_pedal_travel = 0.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 220.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);

  out.accel_pedal_travel = 0.5f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 220.0f);
  EXPECT_FLOAT_EQ(out.torque_derated, 220.0f);
  EXPECT_FLOAT_EQ(out.debug.pedal_shaped_pct, 0.5f);
  EXPECT_FLOAT_EQ(out.torque_power_limited, 110.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 110.0f);

  out.accel_pedal_travel = 1.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 220.0f);
}

TEST_F(TorqueMapTest, PedalExponentShapesTorqueRequest) {
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.pedal_exponent = 2.0f;
  out.accel_pedal_travel = 0.5f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.debug.pedal_shaped_pct, 0.25f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 55.0f);
}

TEST_F(TorqueMapTest, OneDimensionalPowerLimitTableInterpolatesByRpm) {
  params.torque_map.power_limit_kp = 0.0f;
  out.accel_pedal_travel = 1.0f;
  in.motor_speed_rpm = 5250.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_lookup_output, 127.0f);
  EXPECT_FLOAT_EQ(out.debug.power_limit_feedforward_torque_nm, 127.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 127.0f);
}

TEST_F(TorqueMapTest, CurrentLimitScalesAvailableTorque) {
  params.torque_map.power_limit_kp = 0.0f;
  params.torque_map.current_limit_a = 100.0f;
  out.accel_pedal_travel = 1.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  const float expected_scale = 40000.0f / 78000.0f;
  EXPECT_FLOAT_EQ(out.debug.active_power_limit_w, 40000.0f);
  EXPECT_NEAR(out.torque_derated, 220.0f * expected_scale, 0.01f);
  EXPECT_NEAR(out.torque_cmd, 220.0f * expected_scale, 0.01f);
}

TEST_F(TorqueMapTest, TrimPidUsesLiveMeasuredPowerToReduceTorque) {
  out.accel_pedal_travel = 1.0f;
  in.motor_speed_rpm = 6000.0f;
  in.inverter_dc_bus_current_a = 200.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.debug.measured_power_w, 80000.0f);
  EXPECT_NEAR(out.debug.power_limit_feedback_p_nm, -4.0f, 0.01f);
  EXPECT_NEAR(out.torque_cmd, 103.0f, 0.01f);
}

TEST_F(TorqueMapTest, TrimPidDoesNotAddTorqueBelowPowerTarget) {
  out.accel_pedal_travel = 1.0f;
  in.motor_speed_rpm = 6000.0f;
  in.inverter_dc_bus_current_a = 0.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_GT(out.debug.power_limit_feedback_p_nm, 0.0f);
  EXPECT_FLOAT_EQ(out.debug.power_limit_feedback_torque_nm, 0.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 107.0f);
}

TEST_F(TorqueMapTest, InvalidInverterInputsFailClosed) {
  out.accel_pedal_travel = 1.0f;
  in.inverter_power_valid = false;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_limit_input_fault);
}

TEST_F(TorqueMapTest, InvalidTorqueTableFailsClosed) {
  out.accel_pedal_travel = 1.0f;
  params.torque_map.power_limit_torque_rpm[4] =
      params.torque_map.power_limit_torque_rpm[3];

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_limit_input_fault);
}

TEST_F(TorqueMapTest, HardPowerCutFailsClosedOnLiveBusPower) {
  out.accel_pedal_travel = 1.0f;
  in.inverter_dc_bus_current_a = 201.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.debug.measured_power_w, 80400.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.power_safety_cut);
}

TEST_F(TorqueMapTest, HardCurrentCutFailsClosedOnLiveBusCurrent) {
  out.accel_pedal_travel = 1.0f;
  in.inverter_dc_bus_voltage_v = 300.0f;
  in.inverter_dc_bus_current_a = 241.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_TRUE(out.faults.current_safety_cut);
}

TEST_F(TorqueMapTest, MinCellOcvOnlyAffectsLowVoltageDerate) {
  params.torque_map.power_limit_kp = 0.0f;
  out.accel_pedal_travel = 1.0f;
  in.min_cell_voltage_v = 3.1f;
  in.inverter_dc_bus_voltage_v = 400.0f;
  in.inverter_dc_bus_current_a = 0.0f;

  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_NEAR(out.derate_factor_cell_voltage, 0.5f, 0.01f);
  EXPECT_NEAR(out.debug.active_power_limit_w, 78000.0f, 0.01f);
  EXPECT_NEAR(out.torque_derated, 110.0f, 0.5f);
  EXPECT_NEAR(out.torque_cmd, 110.0f, 0.5f);
}

TEST_F(TorqueMapTest, MinCellOcvHoldsDuringCurrent) {
  params.torque_map.power_limit_kp = 0.0f;
  out.accel_pedal_travel = 1.0f;
  in.min_cell_voltage_v = 3.2f;
  in.inverter_dc_bus_current_a = 0.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);
  EXPECT_NEAR(out.derate_factor_cell_voltage, 1.0f, 0.01f);

  in.min_cell_voltage_v = 2.9f;
  in.inverter_dc_bus_current_a = 100.0f;
  torque_map_evaluate(&in, &out, &state, &params, 10);

  EXPECT_NEAR(out.debug.min_cell_ocv_estimate_v, 3.2f, 0.01f);
  EXPECT_NEAR(out.derate_factor_cell_voltage, 1.0f, 0.01f);
}
