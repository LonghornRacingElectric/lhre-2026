#include "vcu_model/components/Battery.h"

#include <cstring>

#include <gtest/gtest.h>

class BatteryTest : public ::testing::Test {
protected:
  vcu_parameters_t params{};
  vcu_inputs_t in{};
  vcu_outputs_t out{};
  battery_state_t state{};

  void SetUp() override {
    params.battery.cell_voltage_ema_alpha = 1.0f;
    params.battery.min_soe_cell_voltage = 3.0f;
    params.battery.max_soe_cell_voltage = 4.2f;
    float soe_lookup[11] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f,
                            60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
    memcpy(params.battery.soe_from_cell_voltage, soe_lookup,
           sizeof(soe_lookup));

    in.min_cell_voltage_v = 3.8f;
    in.max_cell_voltage_v = 4.0f;
    in.battery_current_a = 0.0f;
    in.inverter_current_valid = true;

    battery_init(&state, &params);
  }
};

TEST_F(BatteryTest, UsesMaxOpenCircuitCellVoltageForSoe) {
  battery_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FLOAT_EQ(out.max_open_circuit_cell_voltage, 4.0f);
  EXPECT_NEAR(out.soe_pct, 83.333f, 0.01f);
}

TEST_F(BatteryTest, HoldsOcvWhenCurrentIsNotLow) {
  battery_evaluate(&in, &out, &state, &params, 3);

  in.min_cell_voltage_v = 3.0f;
  in.max_cell_voltage_v = 3.2f;
  in.battery_current_a = 50.0f;
  battery_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FLOAT_EQ(out.max_open_circuit_cell_voltage, 4.0f);
  EXPECT_NEAR(out.soe_pct, 83.333f, 0.01f);
}

TEST_F(BatteryTest, HoldsOcvWhenCurrentSignalIsInvalid) {
  battery_evaluate(&in, &out, &state, &params, 3);

  in.min_cell_voltage_v = 3.0f;
  in.max_cell_voltage_v = 3.2f;
  in.battery_current_a = 0.0f;
  in.inverter_current_valid = false;
  battery_evaluate(&in, &out, &state, &params, 3);

  EXPECT_FLOAT_EQ(out.max_open_circuit_cell_voltage, 4.0f);
  EXPECT_NEAR(out.soe_pct, 83.333f, 0.01f);
}
