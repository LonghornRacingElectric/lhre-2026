#include "vcu_model/components/RegenLinelock.h"

#include <gtest/gtest.h>

class RegenLinelockTest : public ::testing::Test {
protected:
  vcu_parameters_t params{};
  vcu_inputs_t in{};
  vcu_outputs_t out{};
  regen_linelock_state_t state{};

  void SetUp() override {
    params.regen_linelock.disable = false;
    params.regen_linelock.pressure_only_test_mode = false;
    params.regen_linelock.dc_bus_current_regen_is_negative = true;
    params.regen_linelock.rear_pressure_zero_torque_psi = 0.0f;
    params.regen_linelock.rear_pressure_reference_psi = 500.0f;
    params.regen_linelock.rear_pressure_min_engage_psi = 10.0f;
    params.regen_linelock.regen_torque_at_reference_pressure_nm = 76.0f;
    params.regen_linelock.absolute_regen_torque_cap_nm = 230.0f;
    params.regen_linelock.pack_current_limit_a = 45.0f;
    params.regen_linelock.hard_cut_margin_pct = 0.20f;
    params.regen_linelock.hard_cut_reset_pressure_psi = 100.0f;
    params.regen_linelock.pack_terminal_voltage_limit_v = 546.0f;
    params.regen_linelock.pack_resistance_ohm = 0.442f;
    params.regen_linelock.pack_series_cell_count = 130.0f;
    params.regen_linelock.dynamic_voltage_reserve_v = 6.0f;
    params.regen_linelock.pack_ocv_enable_v = 520.11f;
    params.regen_linelock.pack_ocv_disable_hysteresis_v = 2.0f;
    params.regen_linelock.min_cell_temp_c = 10.0f;
    params.regen_linelock.max_cell_temp_c = 55.0f;
    params.regen_linelock.min_motor_speed_rpm = 219.49f;

    in.battery_voltage_v = 520.0f;
    in.battery_current_a = 0.0f;
    in.motor_speed_rpm = 3000.0f;
    in.max_cell_voltage_v = 4.0f;
    in.min_cell_temp_c = 30.0f;
    in.max_cell_temp_c = 30.0f;
    in.battery_pack_status_valid = true;
    in.inverter_current_valid = true;
    in.motor_speed_valid = true;

    out.max_open_circuit_cell_voltage = 4.0f;
    out.bse2_psi = 250.0f;

    regen_linelock_init(&state, &params);
  }
};

TEST_F(RegenLinelockTest, CommandsNegativeTorqueAndLinelockWhenAllowed) {
  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_TRUE(out.regen_available);
  EXPECT_TRUE(out.linelock_enabled);
  EXPECT_NEAR(out.regen_pressure_requested_torque_nm, 38.0f, 0.001f);
  EXPECT_NEAR(out.torque_cmd, -38.0f, 0.001f);
}

TEST_F(RegenLinelockTest, ClipsTorqueByPackCurrentAndMotorSpeed) {
  out.bse2_psi = 1000.0f;
  in.motor_speed_rpm = 3000.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_TRUE(out.linelock_enabled);
  EXPECT_NEAR(out.regen_pack_current_limit_a, 45.0f, 0.001f);
  EXPECT_NEAR(out.regen_torque_limit_nm, 77.333f, 0.01f);
  EXPECT_NEAR(out.torque_cmd, -77.333f, 0.01f);
}

TEST_F(RegenLinelockTest, HighOcvKeepsRearBrakesMechanical) {
  out.max_open_circuit_cell_voltage = 530.0f / 130.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_TRUE(out.faults.regen_linelock_ocv_too_high);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(RegenLinelockTest, PackStatusDoesNotBlockRegen) {
  in.battery_pack_status_valid = false;
  in.min_cell_temp_c = -10.0f;
  in.max_cell_temp_c = 80.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_TRUE(out.regen_available);
  EXPECT_TRUE(out.linelock_enabled);
  EXPECT_FALSE(out.faults.regen_linelock_pack_temp_low);
  EXPECT_FALSE(out.faults.regen_linelock_pack_temp_high);
  EXPECT_NEAR(out.torque_cmd, -38.0f, 0.001f);
}

TEST_F(RegenLinelockTest, LowMotorSpeedKeepsRearBrakesMechanical) {
  in.motor_speed_rpm = 200.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_TRUE(out.faults.regen_linelock_motor_speed_low);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(RegenLinelockTest, LowRearPressureKeepsRearBrakesMechanical) {
  out.bse2_psi = 9.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(RegenLinelockTest, PositiveTorqueCommandOverridesRegen) {
  out.torque_cmd = 12.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_FLOAT_EQ(out.regen_torque_cmd_nm, 0.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(RegenLinelockTest, PositiveTorquePassesThroughWithoutRegenPressure) {
  out.bse2_psi = 0.0f;
  out.torque_cmd = 12.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_FLOAT_EQ(out.regen_torque_cmd_nm, 0.0f);
  EXPECT_FLOAT_EQ(out.torque_cmd, 12.0f);
}

TEST_F(RegenLinelockTest, PressureOnlyTestModeBypassesAvailabilityGates) {
  params.regen_linelock.pressure_only_test_mode = true;
  in.battery_pack_status_valid = false;
  in.inverter_current_valid = false;
  in.motor_speed_valid = false;
  in.motor_speed_rpm = 0.0f;
  in.min_cell_temp_c = 0.0f;
  in.max_cell_temp_c = 80.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_TRUE(out.regen_available);
  EXPECT_TRUE(out.linelock_enabled);
  EXPECT_NEAR(out.regen_pressure_requested_torque_nm, 38.0f, 0.001f);
  EXPECT_NEAR(out.torque_cmd, -38.0f, 0.001f);
}

TEST_F(RegenLinelockTest, PressureOnlyTestModeKeepsHardCurrentCut) {
  params.regen_linelock.pressure_only_test_mode = true;
  out.bse2_psi = 50.0f;
  in.battery_current_a = -55.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_TRUE(out.faults.regen_linelock_current_hard_cut);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(RegenLinelockTest, HardCurrentCutZerosTorqueAndResetsBelowPressure) {
  in.battery_current_a = -55.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_TRUE(out.faults.regen_linelock_current_hard_cut);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);

  in.battery_current_a = 0.0f;
  out = {};
  out.bse2_psi = 90.0f;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.faults.regen_linelock_current_hard_cut);
  EXPECT_FALSE(out.linelock_enabled);
}

TEST_F(RegenLinelockTest, InverterCurrentTimeoutBlocksRegen) {
  in.inverter_current_valid = false;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_TRUE(out.faults.regen_linelock_input_invalid);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(RegenLinelockTest, MotorSpeedTimeoutBlocksRegen) {
  in.motor_speed_valid = false;

  regen_linelock_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FALSE(out.regen_available);
  EXPECT_FALSE(out.linelock_enabled);
  EXPECT_TRUE(out.faults.regen_linelock_input_invalid);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}
