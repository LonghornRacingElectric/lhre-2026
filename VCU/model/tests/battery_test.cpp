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
    const float soe_lookup[11] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f,
                                  60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
    memcpy(params.battery.soe_from_cell_voltage, soe_lookup,
           sizeof(soe_lookup));

    in.min_cell_voltage_v = 3.6f;
    in.max_cell_voltage_v = 4.0f;
    in.battery_current_a = 0.0f;

    battery_init(&state, &params);
  }
};

TEST_F(BatteryTest, UsesFilteredMinOpenCircuitCellVoltageForSoe) {
  battery_evaluate(&in, &out, &state, &params, 1000);

  EXPECT_FLOAT_EQ(out.open_circuit_cell_voltage, 3.6f);
  EXPECT_FLOAT_EQ(out.max_open_circuit_cell_voltage, 4.0f);
  EXPECT_NEAR(out.soe_pct, 50.0f, 0.001f);
}

TEST_F(BatteryTest, HoldsMinOpenCircuitVoltageWhenCurrentIsNotLow) {
  battery_evaluate(&in, &out, &state, &params, 1000);

  in.min_cell_voltage_v = 3.0f;
  in.max_cell_voltage_v = 3.2f;
  in.battery_current_a = 50.0f;
  battery_evaluate(&in, &out, &state, &params, 1000);

  EXPECT_FLOAT_EQ(out.open_circuit_cell_voltage, 3.6f);
  EXPECT_FLOAT_EQ(out.max_open_circuit_cell_voltage, 4.0f);
  EXPECT_NEAR(out.soe_pct, 50.0f, 0.001f);
}
