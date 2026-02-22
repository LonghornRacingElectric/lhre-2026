#include "vcu_model/inc/vcu_model.h"
#include <gtest/gtest.h>


class VCUModelTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;

  void SetUp() override {
    // Basic apps params
    params.apps.apps1_min_adc = 1000;
    params.apps.apps1_max_adc = 4000;
    params.apps.apps2_min_adc = 500;
    params.apps.apps2_max_adc = 2000;
    params.apps.min_travel_threshold = 0.10f;
    params.apps.max_allowable_diff = 0.10f;
    params.apps.implaus_debounce_time_ms = 100;
    params.apps.max_travel_restore_threshold = 0.05f;

    // Basic bse params
    params.bse.bse_adc_at_min_psi = 100;
    params.bse.bse_adc_at_max_psi = 900;
    params.bse.bse_max_psi = 1000.0f;
    params.bse.bse_off_psi = 30.0f;
    params.bse.bse_on_psi = 50.0f;
    params.bse.max_pedal_while_braking = 0.25f;
    params.bse.max_pedal_restore_threshold = 0.05f;

    // Basic torque map
    params.torque_map.max_torque_nm = 100.0f;

    // Buzzer
    params.buzzer_duration_ms = 3000;

    in = {0};
    out = {0};

    vcu_model_init(&params);
  }

  // Helper to transition to drive
  void TransitionToDrive() {
    in.contactors_closed = true;
    in.bse_raw = 600; // Above 50 psi ON threshold
    in.drive_switch = false;
    vcu_model_step(&in, &out, 10); // Update internal switch state

    in.drive_switch = true; // Rising edge
    vcu_model_step(&in, &out, 10);
  }
};

TEST_F(VCUModelTest, InitialStateIsPark) {
  in.apps1_raw = 2500; // 50% pedal
  in.apps2_raw = 1250; // 50% pedal
  vcu_model_step(&in, &out, 10);

  // In park, torque should be 0 despite pedal
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_FALSE(out.buzzer_active);
}

TEST_F(VCUModelTest, TransitionToDriveAndNormalOperation) {
  TransitionToDrive();

  // Currently braking to get into drive, pedal at 0
  in.apps1_raw = 1000;
  in.apps2_raw = 500;
  vcu_model_step(&in, &out, 10);
  EXPECT_TRUE(out.buzzer_active); // Since drive just started
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);

  // Release brake, press pedal
  in.bse_raw = 100; // 0 psi
  vcu_model_step(&in, &out, 10);

  in.apps1_raw = 2500; // 50% pedal
  in.apps2_raw = 1250;
  vcu_model_step(&in, &out, 10);

  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);
  EXPECT_FALSE(out.apps_implaus);
  EXPECT_FALSE(out.brake_latched);
}

TEST_F(VCUModelTest, AppsImplausibilityDisablesTorque) {
  TransitionToDrive();

  // Release brake
  in.bse_raw = 100;

  // Press pedal to 50% but disagree
  in.apps1_raw = 2500; // 50%
  in.apps2_raw = 500;  // 0%

  // Evaluate > 100ms
  for (int i = 0; i < 15; i++) {
    vcu_model_step(&in, &out, 10);
  }

  EXPECT_TRUE(out.apps_implaus);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(VCUModelTest, BrakeLatchDisablesTorque) {
  TransitionToDrive();

  // Release brake
  in.bse_raw = 100;
  vcu_model_step(&in, &out, 10);

  // Press pedal to 50%
  in.apps1_raw = 2500;
  in.apps2_raw = 1250;
  vcu_model_step(&in, &out, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);

  // Now press brake while pedal is > 25%
  in.bse_raw = 900; // Max psi
  vcu_model_step(&in, &out, 10);

  EXPECT_TRUE(out.brake_latched);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(VCUModelTest, TransitionToParkOnContactorLoss) {
  TransitionToDrive();

  // Normal operation
  in.bse_raw = 100;
  in.apps1_raw = 2500;
  in.apps2_raw = 1250;
  vcu_model_step(&in, &out, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);

  // Contactors open
  in.contactors_closed = false;
  vcu_model_step(&in, &out, 10);

  // Now we should be in park
  in.bse_raw = 100;
  in.apps1_raw = 2500;
  in.apps2_raw = 1250;
  vcu_model_step(&in, &out, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}
