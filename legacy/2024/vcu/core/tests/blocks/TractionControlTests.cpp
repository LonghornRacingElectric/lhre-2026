#include <gtest/gtest.h>
#include "VcuParameters.h"
#include "blocks/TractionControl.h"

TEST(TractionControl, TractionControlFunctionality) {
    TractionControl tractionControl;
    TractionControlInput tractionControlInput;
    TractionControlOutput tractionControlOutput;
    VcuParameters vcuParameters;

    vcuParameters.tcsEnabled = true;
    vcuParameters.tcsFeedbackLowPassFilterTimeConstant = 0.01f;

    tractionControl.setParameters(&vcuParameters);

    for(int i = 0; i < 10; i++) {
        tractionControlInput = {
                0.0f,
                0, 0, 0, 0,
                true
        };
        tractionControl.evaluate(&vcuParameters, &tractionControlInput, &tractionControlOutput, 0.001f);
        EXPECT_FLOAT_EQ(tractionControlOutput.regulatedTorqueRequest, 0.0f);
    }

    for(int i = 0; i < 100; i++) {
        float x = i;
        tractionControlInput = {
                100.0f,
                x, x, x*2, x*2,
                true
        };
        tractionControl.evaluate(&vcuParameters, &tractionControlInput, &tractionControlOutput, 0.001f);
        if(i > 0) {
            EXPECT_LT(tractionControlOutput.regulatedTorqueRequest, 100.0f);
//            printf("difference %.3f\n", tractionControlOutput.regulatedTorqueRequest - tractionControlInput.unregulatedTorqueRequest);
        }
    }
}