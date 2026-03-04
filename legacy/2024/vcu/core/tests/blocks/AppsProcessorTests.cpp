
#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/AppsProcessor.h"

TEST(AppsProcessor, AppsRulesCompliance) {
    AppsProcessor appsProcessor;
    AppsProcessorInput appsProcessorInput = {};
    AppsProcessorOutput appsProcessorOutput = {};

    VcuParameters vcuParameters = {};
    vcuParameters.apps1VoltageMin = 1.0f;
    vcuParameters.apps1VoltageMax = 4.0f;
    vcuParameters.apps2VoltageMin = 0.5f;
    vcuParameters.apps2VoltageMax = 2.0f;
    vcuParameters.appsImplausibilityTime = 0.1f;
    vcuParameters.appsPlausibilityRange = 0.1f;
    vcuParameters.appsLowPassFilterTimeConstant = 0.005f;

    appsProcessor.setParameters(&vcuParameters);

    // Test 0% pedal travel
    appsProcessorInput = {1.0f, 0.5f};
    appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 1.0f);
    EXPECT_FLOAT_EQ(appsProcessorOutput.apps, 0.0f);
    EXPECT_TRUE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test 50% pedal travel
    appsProcessorInput = {2.5f, 1.25f};
    for(int i = 0; i < 100; i++)
        appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 0.001f);
    EXPECT_NEAR(appsProcessorOutput.apps, 0.5f, 0.001f);
    EXPECT_TRUE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test 100% pedal travel
    appsProcessorInput = {4.0f, 2.0f};
    for(int i = 0; i < 100; i++)
        appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 0.001f);
    EXPECT_NEAR(appsProcessorOutput.apps, 1.0f, 0.001f);
    EXPECT_TRUE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test implausibility
    appsProcessorInput = {3.0f, 2.0f};
    appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 1.0f);
    EXPECT_FALSE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test out of bounds 1
    appsProcessorInput = {4.5f, 1.0f};
    appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 1.0f);
    EXPECT_FALSE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test out of bounds 2
    appsProcessorInput = {3.0f, 0.2f};
    appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 1.0f);
    EXPECT_FALSE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test clock function
    appsProcessorInput = {3.5f, 1.25f};
    appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 0.09f);
    EXPECT_TRUE(appsProcessorOutput.ok);
    appsProcessor.reset();

    // Test clock function
    appsProcessorInput = {4.0f, 0.5f};
    appsProcessor.evaluate(&vcuParameters, &appsProcessorInput, &appsProcessorOutput, 0.11f);
    EXPECT_FALSE(appsProcessorOutput.ok);
    appsProcessor.reset();
}
