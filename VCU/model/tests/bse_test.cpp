#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include "vcu_model/util/BSE.h"
#include <gtest/gtest.h>

class BSETest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  bse_state_t state;

  void SetUp() override {
    params.bse.bse_adc_at_min_psi = 100;
    params.bse.bse_adc_at_max_psi = 900;
    params.bse.bse_max_psi = 1000.0f;

    params.bse.bse_off_psi = 30.0f;
    params.bse.bse_on_psi = 50.0f;

    params.bse.max_pedal_while_braking = 0.25f;
    params.bse.max_pedal_restore_threshold = 0.05f;

    in = {0};
    out = {0};
    state = {0};
  }
};

TEST_F(BSETest, ADCToPSI) {
  // Note: bse_adc_to_psi divides adc / bse_adc_at_max_psi directly based on the
  // user code: "linear_interp(0.0f, params->bse.bse_max_psi, (float)adc_clamped
  // / (float)params->bse.bse_adc_at_max_psi);" Oh wait, if bse_adc_at_min_psi
  // is 100, clamped value at 100/900 is 111.1 psi? Yes, that's what their
  // calculation does. It clamp at min threshold but assumes 0 psi is at 0 ADC
  // natively.

  // Testing clamped min (ADC <= 100) -> 0 PSI
  EXPECT_FLOAT_EQ(bse_adc_to_psi(50, &params), 0.0f);

  // Testing mid (ADC 500 is exactly in the middle of 100-900) -> 50% = 500 PSI
  EXPECT_FLOAT_EQ(bse_adc_to_psi(500, &params), 500.0f);

  // Testing max (ADC >= 900) -> 1000 PSI
  EXPECT_FLOAT_EQ(bse_adc_to_psi(1000, &params), 1000.0f);
}

TEST_F(BSETest, ActiveHysteresis) {
  // 1. Initial State: To clear static state just in case, input 0
  EXPECT_FALSE(bse_is_active(0.0f, &state, &params));

  // 2. Rising pressure, cross off point but not on point
  EXPECT_FALSE(bse_is_active(40.0f, &state, &params));

  // 3. Cross ON point
  EXPECT_TRUE(bse_is_active(60.0f, &state, &params));

  // 4. Falling pressure, cross ON point but not OFF point
  EXPECT_TRUE(bse_is_active(40.0f, &state, &params));

  // 5. Cross OFF point
  EXPECT_FALSE(bse_is_active(20.0f, &state, &params));
}

TEST_F(BSETest, EvaluateLatches) {
  // Start with pedal low (not pressing gas) and unlatched
  in.bse_raw = 45; // 5% -> 50 PSI -> ON
  out.pedal_filtered = 0.0f;

  // Evaluate multiple times to set initial state safely
  bse_evaluate(&in, &out, &state, &params, 10);

  // Clear brake latched by having pedal low
  in.bse_raw = 0;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.brake_active);
  EXPECT_FALSE(out.brake_latched);

  // Now test latched functionality

  // 1. Press brake
  in.bse_raw = 900; // Full 1000 PSI -> Brake Active
  out.pedal_filtered = 0.0f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.brake_active);
  EXPECT_FALSE(out.brake_latched);

  // 2. Press pedal slightly (<= 0.25)
  out.pedal_filtered = 0.20f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.brake_latched); // shouldn't latch

  // 3. Press pedal hard (> 0.25) while braking -> LATCH
  out.pedal_filtered = 0.30f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.brake_latched);

  // 4. Release brake completely -> LATCH REMAINS because pedal > 0.05
  in.bse_raw = 0;
  out.pedal_filtered = 0.10f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.brake_active); // brake off
  EXPECT_TRUE(out.brake_latched); // latch maintained

  // 5. Release pedal completely
  out.pedal_filtered = 0.0f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.brake_latched); // latch cleared
}
