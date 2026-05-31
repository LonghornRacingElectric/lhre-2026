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

  void SetUp() override {
    float temp_torque_map[11] = {100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f};
    memcpy(params.torque_map.power_limit_torque, temp_torque_map, sizeof(temp_torque_map));
    float temp_pedal_map[11] = {0.0f, 0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f};
    memcpy(params.torque_map.pedal_map, temp_pedal_map, sizeof(temp_pedal_map));
    params.torque_map.pedal_curve_exponent = 2.0f;
    in = {0};
    out = {0};
    in.min_cell_voltage_v = 4.0f;
    torque_map_init(&params);
  }
};

TEST_F(TorqueMapTest, BasicMapping) {
  // 0 pedal -> 0 torque
  out.accel_pedal_travel = 0.0f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 0.0f);

  // 50% pedal -> 25 torque (0.5^2)
  out.accel_pedal_travel = 0.5f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 25.0f);

  // 100% pedal -> 100 torque
  out.accel_pedal_travel = 1.0f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 100.0f);
}
