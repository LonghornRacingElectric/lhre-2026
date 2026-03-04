
#include <gtest/gtest.h>
#include "VcuModel.h"

TEST(Vcu, NormalTorqueRequests) {
  VcuModel vcuModel;
  VcuParameters params;
  VcuInput input;
  VcuOutput output;

  params.appsLowPassFilterTimeConstant = 0.0f;
  params.apps1VoltageMin = 1.0f;
  params.apps1VoltageMax = 4.0f;
  params.apps2VoltageMin = 0.5f;
  params.apps2VoltageMax = 2.0f;
  params.appsDeadZonePct = 0.05f;
  params.appsPlausibilityRange = 0.10f;
  params.appsImplausibilityTime = 0.100f;

  params.bseLowPassFilterTimeConstant = 0.005f;
  params.bseImplausibilityTime = 0.1f;
  params.bseVoltageMin = 0.5f;
  params.bseVoltageMax = 4.5f;

  params.prndlBrakeToStartThreshold = 0.1f;
  params.prndlSwitchDebounceDuration = 0.05f;
  params.prndlBuzzerDuration = 1.0f;

  params.mapPedalToTorqueRequest = CurveParameter(1.0f, 230.0f);

  vcuModel.setParameters(&params);

  // put car into drive
  input.apps1 = 1.05f;
  input.apps2 = 0.55f;
  input.bse1 = 4.0f;
  input.bse2 = 4.0f;
  input.inverterReady = true;
  input.driveSwitch = false;
  for (int i = 0; i < 100; i++) {
    vcuModel.evaluate(&input, &output, 0.001f);
  }
  input.driveSwitch = true;
  vcuModel.evaluate(&input, &output, 0.001f);
  for (int i = 0; i < 100; i++) {
    vcuModel.evaluate(&input, &output, 0.001f);
  }
  EXPECT_TRUE(output.prndlState);
  EXPECT_TRUE(output.r2dBuzzer);
  for (int i = 0; i < 100; i++) {
    vcuModel.evaluate(&input, &output, 0.1f);
  }
  EXPECT_TRUE(output.prndlState);
  EXPECT_FALSE(output.r2dBuzzer);
  input.bse1 = 0.5f;
  input.bse2 = 0.5f;

  // apps 0%
  input.apps1 = 1.0f;
  input.apps2 = 0.5f;
  vcuModel.evaluate(&input, &output, 0.1f);
  EXPECT_NEAR(output.inverterTorqueRequest, 0.0f, 1.0f);
  EXPECT_TRUE(output.enableInverter);
  printf("\napps: %.2f V, %.2f V\n", input.apps1, input.apps2);
  printf("torque request: %.2f Nm\n", output.inverterTorqueRequest);

  // apps 50%
  input.apps1 = 2.5f;
  input.apps2 = 1.25f;
  vcuModel.evaluate(&input, &output, 0.1f);
  EXPECT_NEAR(output.inverterTorqueRequest, 115.0f, 1.0f);
  EXPECT_TRUE(output.enableInverter);
  printf("\napps: %.2f V, %.2f V\n", input.apps1, input.apps2);
  printf("torque request: %.2f Nm\n", output.inverterTorqueRequest);

  // apps 100%
  input.apps1 = 4.0f;
  input.apps2 = 2.0f;
  vcuModel.evaluate(&input, &output, 0.1f);
  EXPECT_NEAR(output.inverterTorqueRequest, 230.0f, 1.0f);
  EXPECT_TRUE(output.enableInverter);
  printf("\napps: %.2f V, %.2f V\n", input.apps1, input.apps2);
  printf("torque request: %.2f Nm\n", output.inverterTorqueRequest);

  printf("\n\n");
}