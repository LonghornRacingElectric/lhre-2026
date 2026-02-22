#include "vcu_model/components/APPS.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <gtest/gtest.h>

class APPSTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  apps_state_t state;

  void SetUp() override {
    // Initialize default parameters for testing
    params.apps.apps1_min_adc_v = 1000;
    params.apps.apps1_max_adc_v = 4000;
    params.apps.apps2_min_adc_v = 500;
    params.apps.apps2_max_adc_v = 2000;

    params.apps.min_travel_threshold = 0.10f;
    params.apps.max_allowable_diff = 0.10f;
    params.apps.implaus_debounce_time_ms = 100;
    params.apps.max_travel_restore_threshold = 0.05f;

    in = {0};
    out = {0};
    state = {0};
  }
};

TEST_F(APPSTest, ADCToTravelConversion) {
  // Min bound
  EXPECT_FLOAT_EQ(apps_adc_to_travel(1000, 1000, 4000), 0.0f);
  // Max bound
  EXPECT_FLOAT_EQ(apps_adc_to_travel(4000, 1000, 4000), 1.0f);
  // Mid point
  EXPECT_FLOAT_EQ(apps_adc_to_travel(2500, 1000, 4000), 0.5f);

  // Out of bounds (clamped)
  EXPECT_FLOAT_EQ(apps_adc_to_travel(500, 1000, 4000), 0.0f);
  EXPECT_FLOAT_EQ(apps_adc_to_travel(5000, 1000, 4000), 1.0f);
}

TEST_F(APPSTest, EvaluateNormalOperation) {
  in.apps1_raw = 2500; // 50% travel
  in.apps2_raw = 1250; // 50% travel

  apps_evaluate(&in, &out, &state, &params, 10);

  EXPECT_FLOAT_EQ(out.apps1_travel, 0.5f);
  EXPECT_FLOAT_EQ(out.apps2_travel, 0.5f);
  EXPECT_FALSE(out.faults.apps_implaus);
}

TEST_F(APPSTest, ImplausibilityTriggerAndRestore) {
  // 1. Initial State: Normal (apps are matching and < 10% diff)
  // To ensure a clean slate (since apps_implausible uses static state),
  // we drive it to 0% first to clear anything from previous tests or if state
  // leaked.
  in.apps1_raw = 1000; // 0%
  in.apps2_raw = 500;  // 0%
  apps_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.faults.apps_implaus);

  // 2. Trigger implausibility deviation (>10% diff)
  in.apps1_raw = 2500; // 50%
  in.apps2_raw = 500;  // 0% -> 50% diff

  // Evaluate for less than debounce time (100ms)
  for (int i = 0; i < 9; i++) {
    apps_evaluate(&in, &out, &state, &params, 10); // 90ms total
    EXPECT_FALSE(out.faults.apps_implaus);
  }

  // Evaluate again crossing debounce threshold
  apps_evaluate(&in, &out, &state, &params, 15); // +15ms = 105ms
  EXPECT_TRUE(out.faults.apps_implaus);

  // 3. Diff returns to normal, but pedal is not idle
  in.apps1_raw = 2500; // 50%
  in.apps2_raw = 1250; // 50%
  apps_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.faults.apps_implaus); // Should stay latched!

  // 4. Pedal goes idle (< 5%) to restore
  in.apps1_raw = 1100; // ~3.3%
  in.apps2_raw = 550;  // ~3.3%
  apps_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.faults.apps_implaus); // Should unlatch
}
