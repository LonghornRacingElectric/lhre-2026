#include "vcu_model/components/BSE.h"
#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <gtest/gtest.h>

class BSETest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  bse_state_t state;

  void SetUp() override {
    params.bse.bse1_adc_at_min_psi_v = 100;
    params.bse.bse1_adc_at_max_psi_v = 900;
    params.bse.bse2_adc_at_min_psi_v = 100;
    params.bse.bse2_adc_at_max_psi_v = 900;
    params.bse.min_psi_deadzone = 0.0f;
    params.bse.max_psi_deadzone = 1.0f;
    params.bse.bse_ema_alpha = 1.0f;
    params.bse.brake_light_min_pct = 0.05f;
    params.bse.brake_light_max_pct = 0.30f;
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
  // Testing clamped min (ADC <= 100) -> 0 PSI
  EXPECT_FLOAT_EQ(bse_adc_to_psi(50, 100, 900, 1000.0f), 0.0f);

  // Testing mid (ADC 500 is exactly in the middle of 100-900) -> 50% = 500 PSI
  EXPECT_FLOAT_EQ(bse_adc_to_psi(500, 100, 900, 1000.0f), 500.0f);

  // Testing max (ADC >= 900) -> 1000 PSI
  EXPECT_FLOAT_EQ(bse_adc_to_psi(1000, 100, 900, 1000.0f), 1000.0f);
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
  in.bse1_raw = 45; // 5% -> 50 PSI -> ON
  in.bse2_raw = 45;

  // Evaluate multiple times to set initial state safely
  out.accel_pedal_travel = 0.0f;
  bse_evaluate(&in, &out, &state, &params, 10);

  // Clear brake latched by having pedal low
  in.bse1_raw = 0;
  in.bse2_raw = 0;
  out.accel_pedal_travel = 0.0f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.brake_pressed);
  EXPECT_FALSE(out.faults.brake_latched);

  // Now test latched functionality

  // 1. Press brake
  in.bse1_raw = 900; // Full 1000 PSI -> Brake Active
  in.bse2_raw = 900;
  out.accel_pedal_travel = 0.0f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.brake_pressed);
  EXPECT_FALSE(out.faults.brake_latched);

  // 2. Press pedal slightly (<= 0.25)
  out.accel_pedal_travel = 0.20f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.faults.brake_latched); // shouldn't latch

  // 3. Press pedal hard (> 0.25) while braking -> LATCH
  out.accel_pedal_travel = 0.30f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_TRUE(out.faults.brake_latched);

  // 4. Release brake completely -> LATCH REMAINS because pedal > 0.05
  in.bse1_raw = 0;
  in.bse2_raw = 0;
  out.accel_pedal_travel = 0.30f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.brake_pressed);       // brake off
  EXPECT_TRUE(out.faults.brake_latched); // latch maintained

  // 5. Release pedal completely
  out.accel_pedal_travel = 0.0f;
  bse_evaluate(&in, &out, &state, &params, 10);
  EXPECT_FALSE(out.faults.brake_latched); // latch cleared
}

TEST_F(BSETest, CustomParametersEnableBrakeAndLight) {
  params.bse.bse_off_psi = 30.0f;
  params.bse.bse_on_psi = 50.0f;
  params.bse.bse1_adc_at_min_psi_v = 156u;
  params.bse.bse1_adc_at_max_psi_v = 635u;
  params.bse.bse2_adc_at_min_psi_v = 156u;
  params.bse.bse2_adc_at_max_psi_v = 635u;
  params.bse.bse_max_psi = 1000.0f;
  params.bse.max_pedal_while_braking = 0.25f;
  params.bse.max_pedal_restore_threshold = 0.05f;
  params.bse.min_psi_deadzone = 0.0f;
  params.bse.max_psi_deadzone = 1.0f;
  params.bse.bse_ema_alpha = 1.0f;
  params.bse.brake_light_min_pct = 0.0f;
  params.bse.brake_light_max_pct = 0.30f;

  // Set ADC such that PSI evaluates > 50.0f to trigger brake_pressed
  // (204 - 156) / (635 - 156) * 1000 = 100.2 PSI
  in.bse1_raw = 204;
  in.bse2_raw = 204;
  out.accel_pedal_travel = 0.0f;

  // Evaluate the BSE model
  bse_evaluate(&in, &out, &state, &params, 10);

  // Check brake pressed turns on and brake light pct turns high
  EXPECT_TRUE(out.brake_pressed);
  EXPECT_FLOAT_EQ(out.brake_light_pct, 0.30f);

  // Release the brake (ADC 160 -> ~8.35 PSI, which is < 30.0f)
  in.bse1_raw = 160;
  in.bse2_raw = 160;

  // Evaluate the BSE model again
  bse_evaluate(&in, &out, &state, &params, 10);

  // Check brake pressed turns off and brake light pct turns low
  EXPECT_FALSE(out.brake_pressed);
  EXPECT_FLOAT_EQ(out.brake_light_pct, 0.0f);
}

TEST_F(BSETest, VoltageParametersEnableBrakeAndLight) {
  params.bse.bse_off_psi = 30.0f;
  params.bse.bse_on_psi = 50.0f;

  // Configure parameters using the new voltage limits
  params.bse.bse1_adc_at_min_psi_v = 0.297f;
  params.bse.bse1_adc_at_max_psi_v = 0.541f;
  params.bse.bse2_adc_at_min_psi_v = 0.297f;
  params.bse.bse2_adc_at_max_psi_v = 0.541f;

  params.bse.bse_max_psi = 1000.0f;
  params.bse.max_pedal_while_braking = 0.25f;
  params.bse.max_pedal_restore_threshold = 0.05f;
  params.bse.min_psi_deadzone = 0.0f;
  params.bse.max_psi_deadzone = 1.0f;
  params.bse.bse_ema_alpha = 1.0f;
  params.bse.brake_light_min_pct = 0.0f;
  params.bse.brake_light_max_pct = 0.30f;

  // Unpressed state: inputs set to 0.297v
  in.bse1_raw = 0.297f;
  in.bse2_raw = 0.297f;
  out.accel_pedal_travel = 0.0f;

  bse_evaluate(&in, &out, &state, &params, 10);

  // Check brake is not pressed and light is low
  EXPECT_FALSE(out.brake_pressed);
  EXPECT_FLOAT_EQ(out.brake_light_pct, 0.0f);

  // Pressed state: inputs set to 0.541v
  in.bse1_raw = 0.541f;
  in.bse2_raw = 0.541f;

  bse_evaluate(&in, &out, &state, &params, 10);

  // Check brake is pressed and light is high
  EXPECT_TRUE(out.brake_pressed);
  EXPECT_FLOAT_EQ(out.brake_light_pct, 0.30f);
}
