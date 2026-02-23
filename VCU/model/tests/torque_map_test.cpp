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
    params.torque_map.max_torque_nm = 100.0f;
    in = {0};
    out = {0};
  }
};

TEST_F(TorqueMapTest, BasicMapping) {
  // 0 pedal -> 0 torque
  out.accel_pedal_travel = 0.0f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);

  // 50% pedal -> 50 torque
  out.accel_pedal_travel = 0.5f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);

  // 100% pedal -> 100 torque
  out.accel_pedal_travel = 1.0f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 100.0f);
}
