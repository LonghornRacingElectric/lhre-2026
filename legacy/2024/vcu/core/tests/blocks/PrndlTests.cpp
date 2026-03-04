#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/Prndl.h"

TEST(Prndl, PrndlFunctionality){
    Prndl prndl;
    PrndlInput prndlInput;
    PrndlOutput prndlOutput;
    VcuParameters vcuParameters;

    vcuParameters.prndlBrakeToStartThreshold = 0.1f;
    vcuParameters.prndlSwitchDebounceDuration = 0.05f;
    vcuParameters.prndlBuzzerDuration = 1.0f;

    prndl.setParameters(&vcuParameters);

    // initial state
    prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
    EXPECT_FALSE(prndlOutput.state);
    EXPECT_FALSE(prndlOutput.buzzer);

    // only brakes
    prndlInput = {false, false, 1000};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // brakes and inverter ready
    prndlInput = {false, true, 1000};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // brakes and drive switch
    prndlInput = {true, false, 1000};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // inverter ready and drive switch
    prndlInput = {true, true, 0};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // inverter ready
    prndlInput = {true, false, 0};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // drive switch
    prndlInput = {false, true, 0};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // nothing
    prndlInput = {false, false, 0};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_FALSE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }

    // go into drive
    prndlInput = {true, true, 1000};
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
    }
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_TRUE(prndlOutput.state);
        EXPECT_TRUE(prndlOutput.buzzer);
    }
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.1f);
        EXPECT_TRUE(prndlOutput.state);
    }
    for(int i = 0; i < 100; i++) {
        prndl.evaluate(&vcuParameters, &prndlInput, &prndlOutput, 0.001f);
        EXPECT_TRUE(prndlOutput.state);
        EXPECT_FALSE(prndlOutput.buzzer);
    }
}
