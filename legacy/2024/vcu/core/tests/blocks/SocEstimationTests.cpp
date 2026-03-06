#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/SocEstimation.h"

TEST(SocEstimation, SocEstimation) {
  VcuParameters params;
  params.socHvVoltageCurve = CurveParameter(600.0f, 1.0f);
  params.socHvRemembered = 0.50f;
  params.socHvCharge = 100.0f;
  params.socHvLowCurrent = 1.0f;
  params.socCurrentLpfTimeConstant = 0.5f;

  SocEstimationInput socEstimationInput;
  SocEstimationOutput socEstimationOutput;
  SocEstimation socEstimation;

  socEstimation.setParameters(&params);

  socEstimationInput = {
      450.0f,
      -0.5f,
      0,
      0,
  };

  for(int i = 0; i < 10; i++) {
    socEstimationInput.hvBatteryCurrent += 0.1f;
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
    EXPECT_FLOAT_EQ(socEstimationOutput.hvBatterySoc, 0.75f);
  }

  socEstimationInput = {
      450.0f,
      2.0f,
      0,
      0,
  };

  for(int i = 0; i < 100; i++) {
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
    EXPECT_FLOAT_EQ(socEstimationOutput.hvBatterySoc, 0.75f);
  }
  for(int i = 0; i < 800; i++) {
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
  }
  for(int i = 0; i < 100; i++) {
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
    EXPECT_LT(socEstimationOutput.hvBatterySoc, 0.75f);
  }

  socEstimationInput = {
      444.0f,
      0.0f,
      0,
      0,
  };

  for(int i = 0; i < 100; i++) {
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
    EXPECT_LT(socEstimationOutput.hvBatterySoc, 0.75f);
  }
  for(int i = 0; i < 800; i++) {
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
  }
  for(int i = 0; i < 100; i++) {
    socEstimation.evaluate(&params, &socEstimationInput, &socEstimationOutput, 0.001f);
    EXPECT_FLOAT_EQ(socEstimationOutput.hvBatterySoc, 0.74f);
  }
}