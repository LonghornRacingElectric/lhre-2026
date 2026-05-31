#include "vcu_model/components/PRNDL.h"
#include <gtest/gtest.h>
#include <vcu_model.h>

class PRNDLTest : public ::testing::Test {
protected:
  prndl_machine_t state = {};
  vcu_inputs_t in = {};
  vcu_outputs_t out = {};

  void SetUp() override {
    state = {};
    prndl_init(&state);
  }
};

TEST_F(PRNDLTest, InitialStateIsPark) { EXPECT_EQ(state.state, PRNDL_PARK); }

TEST_F(PRNDLTest, CorrectlyTransitionToDrive) {
  // Evaluate with brake pressed and pedal at 0 but drive switch off
  out.brake_light_pct = 0.10f;
  out.accel_pedal_travel = 0.0f;
  in.contactors_closed = true;

  // Rising edge of drive switch
  in.drive_switch = true;
  prndl_evaluate(&state, &in, &out, 10);
  EXPECT_EQ(state.state, PRNDL_DRIVE);
}

TEST_F(PRNDLTest, CorrectlyTransitionToParkOnContactorLoss) {
  // Start in Drive
  out.brake_light_pct = 0.10f;
  out.accel_pedal_travel = 0.0f;
  in.contactors_closed = true;
  in.drive_switch = true;
  prndl_evaluate(&state, &in, &out, 0);
  EXPECT_EQ(state.state, PRNDL_DRIVE);

  // Loss of contactors
  in.contactors_closed = false;
  prndl_evaluate(&state, &in, &out, 10);
  EXPECT_EQ(state.state, PRNDL_PARK);
}

TEST_F(PRNDLTest, CorrectlyTransitionToParkOnDriveSwitchLoss) {
  // Start in Drive
  out.brake_light_pct = 0.10f;
  out.accel_pedal_travel = 0.0f;
  in.contactors_closed = true;
  in.drive_switch = true;
  prndl_evaluate(&state, &in, &out, 0);
  EXPECT_EQ(state.state, PRNDL_DRIVE);

  // Loss of drive switch
  in.drive_switch = false;
  prndl_evaluate(&state, &in, &out, 10);
  EXPECT_EQ(state.state, PRNDL_PARK);
}

TEST_F(PRNDLTest, DoesNotTransitionToDriveWithoutBrake) {
  // Evaluate with brake pressed and pedal at 0 but drive switch off
  out.brake_pressed = false;
  out.accel_pedal_travel = 0.0f;
  in.contactors_closed = true;

  // Rising edge of drive switch
  in.drive_switch = true;
  prndl_evaluate(&state, &in, &out, 10);
  EXPECT_EQ(state.state, PRNDL_PARK);
}

TEST_F(PRNDLTest, DoesNotTransitionToDriveWithAccel) {
  // Evaluate with brake pressed and pedal at 0 but drive switch off
  out.brake_light_pct = 0.10f;
  out.accel_pedal_travel = 0.01f;
  in.contactors_closed = true;

  // Rising edge of drive switch
  in.drive_switch = true;
  prndl_evaluate(&state, &in, &out, 10);
  EXPECT_EQ(state.state, PRNDL_PARK);
}
