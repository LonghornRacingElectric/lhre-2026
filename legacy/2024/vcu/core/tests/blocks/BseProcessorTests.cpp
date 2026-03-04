
#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/BseProcessor.h"

TEST(BseProcessor, BseRulesCompliance) {
  BseProcessor bseProcessor;
  BseProcessorInput bseProcessorInput = {};
  BseProcessorOutput bseProcessorOutput = {};

  VcuParameters vcuParameters = {};
  vcuParameters.bseVoltageMin = 0.5f;
  vcuParameters.bseVoltageMax = 4.5f;
  vcuParameters.bseImplausibilityTime = 0.100f;
  vcuParameters.bseLowPassFilterTimeConstant = 0.050f;

  bseProcessor.setParameters(&vcuParameters);

  // Test 0% brakes
  bseProcessorInput = {0.5f, 0.5f};
  for (int i = 0; i < 1000; i++)
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.001f);
  EXPECT_NEAR(bseProcessorOutput.bse, 0.0f, 1.0f);
  EXPECT_TRUE(bseProcessorOutput.ok);
  bseProcessor.reset();

  // Test 50% brakes
  bseProcessorInput = {2.5f, 2.5f};
  for (int i = 0; i < 1000; i++)
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.001f);
  EXPECT_NEAR(bseProcessorOutput.bse, 0.5f, 0.01f);
  EXPECT_TRUE(bseProcessorOutput.ok);
  bseProcessor.reset();

  // Test 100% brakes
  bseProcessorInput = {4.5f, 4.5f};
  for (int i = 0; i < 1000; i++)
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.001f);
  EXPECT_NEAR(bseProcessorOutput.bse, 1.0f, 0.01f);
  EXPECT_TRUE(bseProcessorOutput.ok);
  bseProcessor.reset();

  // Test out of bounds 1
  bseProcessorInput = {5.0f, 2.5f};
  for (int i = 0; i < 1000; i++)
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.001f);
  EXPECT_NEAR(bseProcessorOutput.bse, 0.5f, 0.01f);
  EXPECT_TRUE(bseProcessorOutput.ok);
  bseProcessor.reset();

  // Test out of bounds 2
  bseProcessorInput = {1.5f, 0.1f};
  for (int i = 0; i < 1000; i++)
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.001f);
  EXPECT_NEAR(bseProcessorOutput.bse, 0.25f, 0.01f);
  EXPECT_TRUE(bseProcessorOutput.ok);
  bseProcessor.reset();

  // Test that previous value is maintained for 100ms if both BSEs are lost
  bseProcessorInput = {1.5f, 1.5f};
  for (int i = 0; i < 1000; i++)
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.001f);
  EXPECT_NEAR(bseProcessorOutput.bse, 0.25f, 0.01f);
  EXPECT_TRUE(bseProcessorOutput.ok);
  bseProcessorInput = {0.0f, 5.0f};
  for (int i = 0; i < 9; i++) {
    bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.010f);
    EXPECT_NEAR(bseProcessorOutput.bse, 0.25f, 0.01f);
    EXPECT_TRUE(bseProcessorOutput.ok);
  }
  bseProcessor.evaluate(&vcuParameters, &bseProcessorInput, &bseProcessorOutput, 0.020f);
  EXPECT_FALSE(bseProcessorOutput.ok);
  bseProcessor.reset();
}
