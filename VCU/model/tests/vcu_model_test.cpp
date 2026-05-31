#include "vcu_model/inc/vcu_model.h"
#include <gtest/gtest.h>

class VCUModelTest : public ::testing::Test {
protected:
  vcu_parameters_t params;
  vcu_inputs_t in;
  vcu_outputs_t out;
  vcu_model_context_t ctx;

  void SetUp() override {
    // Basic apps params
    params.apps.apps1_min_adc_v = 1000;
    params.apps.apps1_max_adc_v = 4000;
    params.apps.apps2_min_adc_v = 500;
    params.apps.apps2_max_adc_v = 2000;
    params.apps.min_travel_threshold = 0.10f;
    params.apps.min_travel_deadzone = 0.10f;
    params.apps.max_travel_deadzone = 0.90f;
    params.apps.max_allowable_diff = 0.10f;
    params.apps.implaus_debounce_time_ms = 100;
    params.apps.max_travel_restore_threshold = 0.05f;
    params.apps.pedal_ema_alpha =
        1.0f; // No filtering by default for basic tests

    // Basic bse params
    params.bse.bse1_adc_at_min_psi_v = 100;
    params.bse.bse1_adc_at_max_psi_v = 900;
    params.bse.bse2_adc_at_min_psi_v = 150;
    params.bse.bse2_adc_at_max_psi_v = 950;
    params.bse.min_psi_deadzone = 0.0f;
    params.bse.max_psi_deadzone = 1.0f;
    params.bse.bse_ema_alpha = 1.0f;
    params.bse.brake_light_min_pct = 0.05f;
    params.bse.brake_light_max_pct = 0.30f;
    params.bse.bse_max_psi = 1000.0f;
    params.bse.bse_off_psi = 30.0f;
    params.bse.bse_on_psi = 50.0f;
    params.bse.max_pedal_while_braking = 0.30f;
    params.bse.max_pedal_restore_threshold = 0.05f;
    params.bse.bse_brake_light_psi = 30.0f;

    // Buzzer
    params.buzzer_duration_ms = 3000;

    // Torque map
    float temp_torque_map[11] = {100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
                                 100.0f, 100.0f, 100.0f, 100.0f, 100.0f};
    memcpy(params.torque_map.power_limit_torque, temp_torque_map,
           sizeof(temp_torque_map));
    float temp_pedal_map[11] = {0.0f,  0.10f, 0.20f, 0.30f, 0.40f, 0.50f,
                                0.60f, 0.70f, 0.80f, 0.90f, 1.0f};
    memcpy(params.torque_map.pedal_map, temp_pedal_map, sizeof(temp_pedal_map));
    params.torque_map.pedal_curve_exponent = 1.0f;

    // Regen linelock positive-torque override parameters
    params.regen_linelock.disable = false;
    params.regen_linelock.pressure_only_test_mode = false;
    params.regen_linelock.rear_pressure_zero_torque_psi = 0.0f;
    params.regen_linelock.rear_pressure_reference_psi = 500.0f;
    params.regen_linelock.rear_pressure_min_engage_psi = 10.0f;
    params.regen_linelock.regen_torque_at_reference_pressure_nm = 76.0f;
    params.regen_linelock.absolute_regen_torque_cap_nm = 230.0f;
    params.regen_linelock.pedal_torque_release_threshold_nm = 20.0f;
    params.regen_linelock.linelock_close_delay_ms = 100u;
    params.regen_linelock.pack_current_limit_a = 45.0f;
    params.regen_linelock.pack_terminal_voltage_limit_v = 546.0f;
    params.regen_linelock.pack_series_cell_count = 130.0f;
    params.regen_linelock.pack_ocv_enable_v = 520.11f;

    in = {0};
    out = {0};
    ctx = {};

    vcu_model_init(&ctx, &params);
  }

  // Helper to transition to drive
  void TransitionToDrive() {
    in.contactors_closed = true;
    in.bse1_raw = 600; // Above 50 psi ON threshold
    in.bse2_raw = 650;
    in.drive_switch = false;
    vcu_model_step(&ctx, &in, &out, 10); // Update internal switch state

    in.drive_switch = true; // Rising edge
    vcu_model_step(&ctx, &in, &out, 10);
  }
};

TEST_F(VCUModelTest, InitialStateIsPark) {
  in.apps1_raw = 2500; // 50% pedal
  in.apps2_raw = 1250; // 50% pedal
  vcu_model_step(&ctx, &in, &out, 10);

  // In park, torque should be 0 despite pedal
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
  EXPECT_FALSE(out.buzzer_active);
}

TEST_F(VCUModelTest, TransitionToDriveAndNormalOperation) {
  TransitionToDrive();

  // Currently braking to get into drive, pedal at 0
  in.apps1_raw = 1000;
  in.apps2_raw = 500;
  vcu_model_step(&ctx, &in, &out, 10);
  EXPECT_TRUE(out.buzzer_active); // Since drive just started
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);

  // Release brake, press pedal
  in.bse1_raw = 100; // 0 psi
  in.bse2_raw = 150;
  vcu_model_step(&ctx, &in, &out, 10);

  in.apps1_raw = 2500; // 50% pedal
  in.apps2_raw = 1250;
  vcu_model_step(&ctx, &in, &out, 10);

  // Due to 10% min deadzone and 90% max deadzone:
  // pedal value spans 10% to 90% mapping 0 to 1
  // 50% true pedal corresponds strictly to (0.50 - 0.10) / 0.80 = 0.50

  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);
  EXPECT_FALSE(out.faults.apps_implaus);
  EXPECT_FALSE(out.faults.brake_latched);
}

TEST_F(VCUModelTest, AppsImplausibilityDisablesTorque) {
  TransitionToDrive();

  // Release brake
  in.bse1_raw = 100;
  in.bse2_raw = 150;

  // Press pedal to 50% but disagree
  in.apps1_raw = 2500; // 50%
  in.apps2_raw = 500;  // 0%

  // Evaluate > 100ms
  for (int i = 0; i < 15; i++) {
    vcu_model_step(&ctx, &in, &out, 10);
  }

  EXPECT_TRUE(out.faults.apps_implaus);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}

TEST_F(VCUModelTest, BrakePressureOpensLinelockAbovePedalTorqueThreshold) {
  TransitionToDrive();

  // Release brake
  in.bse1_raw = 100;
  in.bse2_raw = 150;
  vcu_model_step(&ctx, &in, &out, 10);

  // Press pedal to 50%
  in.apps1_raw = 2500;
  in.apps2_raw = 1250;
  vcu_model_step(&ctx, &in, &out, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);

  // Now press brake while pedal is > 30%
  in.bse1_raw = 900; // Max psi
  in.bse2_raw = 950;
  vcu_model_step(&ctx, &in, &out, 10);

  EXPECT_TRUE(out.faults.brake_latched);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);
  EXPECT_FALSE(out.linelock_enabled);

  // Releasing brake removes the linelock release gate. The brake latch remains
  // telemetry only and no longer globally disables torque.
  in.bse1_raw = 100;
  in.bse2_raw = 150;
  vcu_model_step(&ctx, &in, &out, 10);

  EXPECT_TRUE(out.faults.brake_latched);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);
}

TEST_F(VCUModelTest, TransitionToParkOnContactorLoss) {
  TransitionToDrive();

  // Normal operation
  in.bse1_raw = 100;
  in.bse2_raw = 150;
  in.apps1_raw = 2500;
  in.apps2_raw = 1250;
  vcu_model_step(&ctx, &in, &out, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 50.0f);

  // Contactors open
  in.contactors_closed = false;
  vcu_model_step(&ctx, &in, &out, 10);

  // Now we should be in park
  in.bse1_raw = 100;
  in.bse2_raw = 150;
  in.apps1_raw = 2500;
  in.apps2_raw = 1250;
  vcu_model_step(&ctx, &in, &out, 10);
  EXPECT_FLOAT_EQ(out.torque_cmd, 0.0f);
}
