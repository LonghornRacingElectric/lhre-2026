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
    float temp_torque_map[11][11] = {
              /* rpm=    0 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 100.0f, 110.0f, 120.0f, 130.0f,  140.0f,  150.00f },
              /* rpm=  600 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 100.0f, 110.0f, 120.0f, 130.0f,  140.0f,  150.00f },
              /* rpm= 1200 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 120.0f, 130.0f, 150.0f,  160.0f,  170.00f },
              /* rpm= 1800 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 120.0f, 130.0f, 150.0f,  170.0f,  190.00f },
              /* rpm= 2400 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  188.0f,  200.00f },
              /* rpm= 3000 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  220.00f },
              /* rpm= 3600 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 176.0f,  198.0f,  203.72f },
              /* rpm= 4200 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 154.0f, 174.62f, 174.62f, 174.62f },
              /* rpm= 4800 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 152.79f, 152.79f, 152.79f, 152.79f },
              /* rpm= 5400 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 132.0f, 135.81f, 135.81f, 135.81f, 135.81f },
              /* rpm= 6000 */ {   0.0f,  22.0f,  44.0f,  66.0f,  88.0f, 110.0f, 122.23f, 122.23f, 122.23f, 122.23f, 122.23f },
            };
    memcpy(params.torque_map.torque_map, temp_torque_map, sizeof(temp_torque_map));
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

  // 50% pedal -> 50 torque
  out.accel_pedal_travel = 0.5f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 100.0f);

  // 100% pedal -> 100 torque
  out.accel_pedal_travel = 1.0f;
  torque_map_evaluate(&in, &out, &params, 10);
  EXPECT_FLOAT_EQ(out.torque_lookup_output, 150.0f);
}
